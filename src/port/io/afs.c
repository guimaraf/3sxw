#include "port/io/afs.h"
#include "common.h"
#include "port/debug/debug_log.h"
#include <SDL3/SDL.h>
#include <stdint.h>
#include <stdio.h>

// Inspired by https://github.com/MaikelChan/AFSLib

#define AFS_MAGIC 0x41465300
#define AFS_ATTRIBUTE_HEADER_SIZE 8
#define AFS_ATTRIBUTE_ENTRY_SIZE 48
#define AFS_MAX_NAME_LENGTH 32

#define AFS_MAX_READ_REQUESTS 100

// Uncomment this to enable debug prints
// #define AFS_DEBUG

typedef struct AFSEntry {
    unsigned int offset;
    unsigned int size;
    char name[AFS_MAX_NAME_LENGTH];
} AFSEntry;

typedef struct AFS {
    char* file_path;
    unsigned int entry_count;
    AFSEntry* entries;
} AFS;

typedef struct ReadRequest {
    bool initialized;
    bool close_pending;
    bool release_pending;
    bool stop_requested;
    int index;
    int file_num;
    int sector;
    AFSReadState state;
    SDL_AsyncIO* asyncio;
} ReadRequest;

static AFS afs = { 0 };
static SDL_AsyncIOQueue* asyncio_queue = NULL;
static ReadRequest requests[AFS_MAX_READ_REQUESTS] = { { 0 } };

static bool is_valid_attribute_data(Uint32 attributes_offset, Uint32 attributes_size, Sint64 file_size,
                                    Uint64 entries_end_offset, Uint32 entry_count) {
    if ((attributes_offset == 0) || (attributes_size == 0) || (file_size < 0)) {
        return false;
    }

    if ((Uint64)attributes_size < ((Uint64)entry_count * AFS_ATTRIBUTE_ENTRY_SIZE)) {
        return false;
    }

    if (attributes_offset < entries_end_offset) {
        return false;
    }

    if ((Uint64)attributes_offset + attributes_size > (Uint64)file_size) {
        return false;
    }

    return true;
}

static bool read_string(SDL_IOStream* src, char* dst, size_t dst_size, size_t source_size) {
    if (dst_size == 0 || source_size == 0) {
        return false;
    }

    dst[0] = '\0';

    for (size_t i = 0; i < source_size; i++) {
        char c;

        if (SDL_ReadIO(src, &c, 1) != 1) {
            dst[0] = '\0';
            return false;
        }

        if (i < dst_size - 1) {
            dst[i] = c;
        }

        if (c == '\0') {
            dst[SDL_min(i, dst_size - 1)] = '\0';
            return true;
        }
    }

    dst[dst_size - 1] = '\0';
    return false;
}

static void reset_afs_metadata() {
    SDL_free(afs.file_path);
    SDL_free(afs.entries);
    SDL_zero(afs);
}

static bool fail_afs_initialization(SDL_IOStream* io, const char* message) {
    char saved_error[512];
    SDL_strlcpy(saved_error, message, sizeof(saved_error));

    if (io != NULL) {
        SDL_CloseIO(io);
    }

    reset_afs_metadata();
    SDL_SetError("%s", saved_error);
    return false;
}

static bool init_afs(const char* file_path) {
    afs.file_path = SDL_strdup(file_path);

    if (afs.file_path == NULL) {
        SDL_SetError("Couldn't allocate the AFS file path: %s", file_path);
        return false;
    }

    SDL_IOStream* io = SDL_IOFromFile(file_path, "rb");

    if (io == NULL) {
        reset_afs_metadata();
        return false;
    }

    const Sint64 file_size = SDL_GetIOSize(io);

    if (file_size < 16) {
        char message[512];
        SDL_snprintf(message, sizeof(message), "AFS file is too small or unreadable: %s", file_path);
        return fail_afs_initialization(io, message);
    }

    // Check magic

    Uint32 magic = 0;
    if (!SDL_ReadU32BE(io, &magic)) {
        char message[512];
        SDL_snprintf(message, sizeof(message), "Couldn't read the AFS header: %s", file_path);
        return fail_afs_initialization(io, message);
    }

    if (magic != AFS_MAGIC) {
        char message[512];
        SDL_snprintf(message, sizeof(message), "Invalid AFS header in file: %s", file_path);
        return fail_afs_initialization(io, message);
    }

    // Read entries

    if (!SDL_ReadU32LE(io, &afs.entry_count)) {
        char message[512];
        SDL_snprintf(message, sizeof(message), "Couldn't read the AFS entry count: %s", file_path);
        return fail_afs_initialization(io, message);
    }

    const Uint64 entry_table_size = (Uint64)afs.entry_count * 8;
    if ((afs.entry_count == 0) || (entry_table_size > ((Uint64)file_size - 16)) ||
        ((Uint64)afs.entry_count > (Uint64)(SIZE_MAX / sizeof(AFSEntry)))) {
        char message[512];
        SDL_snprintf(message, sizeof(message), "Invalid AFS entry table in file: %s", file_path);
        return fail_afs_initialization(io, message);
    }

    afs.entries = SDL_malloc(sizeof(AFSEntry) * afs.entry_count);

    if (afs.entries == NULL) {
        char message[512];
        SDL_snprintf(message, sizeof(message), "Couldn't allocate the AFS entry table for: %s", file_path);
        return fail_afs_initialization(io, message);
    }

    SDL_memset(afs.entries, 0, sizeof(AFSEntry) * afs.entry_count);

    Uint32 entries_start_offset = 0;
    Uint64 entries_end_offset = 0;

    for (Uint32 i = 0; i < afs.entry_count; i++) {
        AFSEntry* entry = &afs.entries[i];
        if (!SDL_ReadU32LE(io, &entry->offset) || !SDL_ReadU32LE(io, &entry->size)) {
            char message[512];
            SDL_snprintf(message, sizeof(message), "Couldn't read AFS entry %u from: %s", i, file_path);
            return fail_afs_initialization(io, message);
        }

        if ((Uint64)entry->offset + entry->size > (Uint64)file_size) {
            char message[512];
            SDL_snprintf(message, sizeof(message), "AFS entry %u points outside the file: %s", i, file_path);
            return fail_afs_initialization(io, message);
        }

        if (entry->offset != 0) {
            if ((entries_start_offset == 0) || (entry->offset < entries_start_offset)) {
                entries_start_offset = entry->offset;
            }

            entries_end_offset = SDL_max(entries_end_offset, (Uint64)entry->offset + entry->size);
        }
    }

    // Locate attributes

    Uint32 attributes_offset;
    Uint32 attributes_size;
    bool has_attributes = false;

    if (!SDL_ReadU32LE(io, &attributes_offset) || !SDL_ReadU32LE(io, &attributes_size)) {
        char message[512];
        SDL_snprintf(message, sizeof(message), "Couldn't read the AFS attribute header: %s", file_path);
        return fail_afs_initialization(io, message);
    }

    if (is_valid_attribute_data(attributes_offset, attributes_size, file_size, entries_end_offset, afs.entry_count)) {
        has_attributes = true;
    } else if (entries_start_offset >= AFS_ATTRIBUTE_HEADER_SIZE &&
               SDL_SeekIO(io, entries_start_offset - AFS_ATTRIBUTE_HEADER_SIZE, SDL_IO_SEEK_SET) >= 0 &&
               SDL_ReadU32LE(io, &attributes_offset) && SDL_ReadU32LE(io, &attributes_size)) {

        if (is_valid_attribute_data(attributes_offset, attributes_size, file_size, entries_end_offset, afs.entry_count)) {
            has_attributes = true;
        }
    }

    for (Uint32 i = 0; i < afs.entry_count; i++) {
        AFSEntry* entry = &afs.entries[i];

        if ((entry->offset != 0) && has_attributes) {
            const Sint64 name_offset = (Sint64)attributes_offset + ((Sint64)i * AFS_ATTRIBUTE_ENTRY_SIZE);
            if ((SDL_SeekIO(io, name_offset, SDL_IO_SEEK_SET) < 0) ||
                !read_string(io, entry->name, sizeof(entry->name), AFS_ATTRIBUTE_ENTRY_SIZE)) {
                char message[512];
                SDL_snprintf(message, sizeof(message), "Invalid name for AFS entry %u in: %s", i, file_path);
                return fail_afs_initialization(io, message);
            }
        }
    }

    if (!SDL_CloseIO(io)) {
        char message[512];
        SDL_snprintf(message, sizeof(message), "Couldn't finish reading the AFS file: %s", file_path);
        return fail_afs_initialization(NULL, message);
    }

    return true;
}

static bool init_asyncio(const char* file_path) {
    asyncio_queue = SDL_CreateAsyncIOQueue();

    if (asyncio_queue == NULL) {
        SDL_SetError("Couldn't create the asynchronous AFS queue for: %s", file_path);
    }

    return asyncio_queue != NULL;
}

static bool is_valid_handle(AFSHandle handle) {
    return (handle >= 0) && (handle < SDL_arraysize(requests)) && requests[handle].initialized;
}

static bool request_asyncio_close(ReadRequest* request) {
    if (request->asyncio == NULL || request->close_pending) {
        return true;
    }

    if (!SDL_CloseAsyncIO(request->asyncio, false, asyncio_queue, request)) {
        request->state = AFS_READ_STATE_ERROR;
        return false;
    }

    request->asyncio = NULL;
    request->close_pending = true;
    return true;
}

bool AFS_Init(const char* file_path) {
    if (!init_afs(file_path)) {
        return false;
    }

    if (!init_asyncio(file_path)) {
        char async_error[512];
        SDL_strlcpy(async_error, SDL_GetError(), sizeof(async_error));
        AFS_Finish();
        SDL_SetError("%s", async_error);
        return false;
    }

    return true;
}

void AFS_Finish() {
    if (asyncio_queue != NULL) {
        for (int i = 0; i < SDL_arraysize(requests); i++) {
            ReadRequest* request = &requests[i];

            if (request->initialized) {
                request->release_pending = true;
                request->stop_requested = true;
                request_asyncio_close(request);
            }
        }

        SDL_DestroyAsyncIOQueue(asyncio_queue);
        asyncio_queue = NULL;
    }

    reset_afs_metadata();
    SDL_zeroa(requests);
}

unsigned int AFS_GetFileCount() {
    return afs.entry_count;
}

unsigned int AFS_GetSize(int file_num) {
    if ((file_num < 0) || (file_num >= afs.entry_count)) {
        return 0;
    }

    return afs.entries[file_num].size;
}

// AFS reading

static void process_asyncio_outcome(const SDL_AsyncIOOutcome* outcome) {
    ReadRequest* request = (ReadRequest*)outcome->userdata;

    if (request == NULL || !request->initialized) {
        return;
    }

#if defined(AFS_DEBUG)
    printf("📂 %d: request complete (type = %d, result = %d, offset = 0x%llX, requested = 0x%llX, transferred = "
           "0x%llX)\n",
           request->index,
           outcome->type,
           outcome->result,
           outcome->offset,
           outcome->bytes_requested,
           outcome->bytes_transferred);
#endif

    switch (outcome->type) {
    case SDL_ASYNCIO_TASK_READ:
        if (outcome->result == SDL_ASYNCIO_COMPLETE && outcome->bytes_transferred != outcome->bytes_requested) {
            request->state = AFS_READ_STATE_ERROR;
        } else if (request->stop_requested) {
            request->state = AFS_READ_STATE_IDLE;
        } else {
            switch (outcome->result) {
            case SDL_ASYNCIO_COMPLETE:
                request->state = AFS_READ_STATE_FINISHED;
                break;

            case SDL_ASYNCIO_CANCELED:
                request->state = AFS_READ_STATE_ERROR;
                break;

            case SDL_ASYNCIO_FAILURE:
                request->state = AFS_READ_STATE_ERROR;
                break;
            }
        }

        if (!request->close_pending && !request_asyncio_close(request)) {
            request->state = AFS_READ_STATE_ERROR;
        }

        break;

    case SDL_ASYNCIO_TASK_CLOSE:
        request->close_pending = false;

        if (outcome->result == SDL_ASYNCIO_FAILURE) {
            request->state = AFS_READ_STATE_ERROR;
        } else if (request->stop_requested) {
            request->state = AFS_READ_STATE_IDLE;
        }

        if (request->release_pending) {
            SDL_zerop(request);
            return;
        }

        break;

    case SDL_ASYNCIO_TASK_WRITE:
        // Do nothing
        break;
    }

#if defined(AFS_DEBUG)
    printf("📂 %d: new state = %d\n", request->index, request->state);
#endif

}

void AFS_RunServer() {
    if (asyncio_queue == NULL) {
        return;
    }

    SDL_AsyncIOOutcome outcome;

    while (SDL_GetAsyncIOResult(asyncio_queue, &outcome)) {
        process_asyncio_outcome(&outcome);
    }
}

AFSHandle AFS_Open(int file_num) {
    if ((file_num < 0) || ((unsigned int)file_num >= afs.entry_count) || (asyncio_queue == NULL)) {
        return AFS_NONE;
    }

    AFSHandle retval = AFS_NONE;

    for (int i = 0; i < SDL_arraysize(requests); i++) {
        ReadRequest* request = &requests[i];

        if (request->initialized) {
            continue;
        }

        request->file_num = file_num;
        request->sector = 0;
        request->index = i;
        request->state = AFS_READ_STATE_IDLE;
        request->initialized = true;
        retval = i;
        break;
    }

#if defined(AFS_DEBUG)
    printf("📂 %d: open (file_num = %d, filename = %s)\n", retval, file_num, afs.entries[file_num].name);
#endif

    return retval;
}

void AFS_Read(AFSHandle handle, int sectors, void* buf) {
#if defined(AFS_DEBUG)
    printf("📂 %d: read (sectors = %d, bytes = 0x%X)\n", handle, sectors, sectors * 2048);
#endif

    if (!is_valid_handle(handle)) {
        return;
    }

    ReadRequest* request = &requests[handle];

    if (sectors <= 0 || buf == NULL) {
        request->state = AFS_READ_STATE_ERROR;
        return;
    }

    if (request->state == AFS_READ_STATE_READING || request->close_pending || request->asyncio != NULL) {
        request->state = AFS_READ_STATE_ERROR;
        return;
    }

    const Uint64 entry_size_rounded = ((Uint64)afs.entries[request->file_num].size + 2047) & ~(Uint64)2047;
    const Uint64 relative_offset = (Uint64)request->sector * 2048;
    const Uint64 read_size = (Uint64)sectors * 2048;

    if (relative_offset > entry_size_rounded || read_size > (entry_size_rounded - relative_offset)) {
        request->state = AFS_READ_STATE_ERROR;
        return;
    }

    const Uint64 offset = (Uint64)afs.entries[request->file_num].offset + relative_offset;

    request->stop_requested = false;
    request->state = AFS_READ_STATE_READING;
    request->asyncio = SDL_AsyncIOFromFile(afs.file_path, "r");

    if (request->asyncio == NULL) {
        printf("SDL_AsyncIOFromFile error: %s\n", SDL_GetError());
        request->state = AFS_READ_STATE_ERROR;
        return;
    }

    const bool success = SDL_ReadAsyncIO(request->asyncio, buf, offset, read_size, asyncio_queue, request);

    if (!success) {
        printf("SDL_ReadAsyncIO error: %s\n", SDL_GetError());
        request->state = AFS_READ_STATE_ERROR;
        request_asyncio_close(request);
        return;
    }

    request->sector += sectors;
}

void AFS_ReadSync(AFSHandle handle, int sectors, void* buf) {
#if defined(AFS_DEBUG)
    printf("📂 %d: read sync\n", handle);
#endif

    const Uint64 start_ns = DebugLog_IsEnabled() ? SDL_GetTicksNS() : 0;
    AFS_Read(handle, sectors, buf);

    if (!is_valid_handle(handle) || requests[handle].state == AFS_READ_STATE_ERROR) {
        return;
    }

    SDL_AsyncIOOutcome outcome;
    bool read_completed = false;

    while (SDL_WaitAsyncIOResult(asyncio_queue, &outcome, -1)) {
        ReadRequest* outcome_request = (ReadRequest*)outcome.userdata;
        const bool belongs_to_handle = outcome_request == &requests[handle];
        const SDL_AsyncIOTaskType outcome_type = outcome.type;
        process_asyncio_outcome(&outcome);

        if (belongs_to_handle && outcome_type == SDL_ASYNCIO_TASK_READ) {
            read_completed = true;
        }

        if (read_completed && !requests[handle].close_pending) {
            break;
        }
    }

    if (DebugLog_IsEnabled()) {
        const ReadRequest* request = &requests[handle];
        DebugLog_RecordAfsSyncRead(
            request->file_num, sectors, (uint32_t)(sectors * 2048), (double)(SDL_GetTicksNS() - start_ns) / 1e6);
    }
}

void AFS_Stop(AFSHandle handle) {
#if defined(AFS_DEBUG)
    printf("📂 %d: stop\n", handle);
#endif

    if (!is_valid_handle(handle)) {
        return;
    }

    ReadRequest* request = &requests[handle];
    request->stop_requested = true;

    if (!request_asyncio_close(request)) {
        request->state = AFS_READ_STATE_ERROR;
    } else if (!request->close_pending) {
        request->state = AFS_READ_STATE_IDLE;
    }
}

void AFS_Close(AFSHandle handle) {
#if defined(AFS_DEBUG)
    printf("📂 %d: close\n", handle);
#endif

    if (!is_valid_handle(handle)) {
        return;
    }

    ReadRequest* request = &requests[handle];
    request->release_pending = true;
    AFS_Stop(handle);

    if (!request->close_pending && request->asyncio == NULL) {
        SDL_zerop(request);
    }
}

bool AFS_IsOpen(AFSHandle handle) {
    return is_valid_handle(handle);
}

AFSReadState AFS_GetState(AFSHandle handle) {
    if (!is_valid_handle(handle)) {
        return AFS_READ_STATE_ERROR;
    }

    ReadRequest* request = &requests[handle];

#if defined(AFS_DEBUG)
    printf("📂 %d: get state (%d)\n", handle, request->state);
#endif

    return request->state;
}

unsigned int AFS_GetSectorCount(AFSHandle handle) {
    if (!is_valid_handle(handle)) {
        return 0;
    }

    ReadRequest* request = &requests[handle];
    const unsigned int size = afs.entries[request->file_num].size;
    return (size + 2048 - 1) / 2048;
}
