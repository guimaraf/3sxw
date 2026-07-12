#include "common.h"
#include "port/utils.h"
#include "port/paths.h"

#ifndef SCE_STM_R
#define SCE_STM_R 0x01
#define SCE_STM_W 0x02
#define SCE_STM_X 0x04
#define SCE_STM_C 0x08
#define SCE_STM_D 0x20
#endif

#include <libmc.h>

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <SDL3/SDL.h>

#define MAX_OPEN_FILES 16
#define MC_SAVES_DIR "saves/"
#define MC_SLOT1_DIR "slot1/"
#define MC_SLOT2_DIR "slot2/"
#define MC_OPEN_CREATE 0x0200

typedef void (*OperationFinalizer)(int* result);

typedef struct GetInfoOperation {
    int port;
    int* type;
    int* free;
    int* format;
} GetInfoOperation;

typedef struct OpenOperation {
    int port;
    int fd;
    char* path;
} OpenOperation;

typedef struct CloseOperation {
    int fd;
    int result;
} CloseOperation;

typedef struct ReadWriteOperation {
    int fd;
    const void* write_buf;
    void* read_buf;
    int length;
} ReadWriteOperation;

typedef struct GetDirOperation {
    int port;
    char* name;
    int mode;
    int maxent;
    sceMcTblGetDir* table;
} GetDirOperation;

typedef struct PathOperation {
    int port;
    char* path;
} PathOperation;

typedef struct OpenFile {
    SDL_IOStream* stream;
    char* final_path;
    char* temporary_path;
    bool atomic_write;
    bool write_failed;
} OpenFile;

static GetInfoOperation get_info_operation = { 0 };
static OpenOperation open_operation = { 0 };
static CloseOperation close_operation = { 0 };
static ReadWriteOperation rw_operation = { 0 };
static GetDirOperation get_dir_operation = { 0 };
static PathOperation mkdir_operation = { 0 };
static PathOperation delete_operation = { 0 };
static int registered_operation = 0;

static OpenFile open_files[MAX_OPEN_FILES] = { { 0 } };

static void clear_open_operation(void) {
    if (open_operation.path) {
        SDL_free(open_operation.path);
        open_operation.path = NULL;
    }
}

static void clear_get_dir_operation(void) {
    if (get_dir_operation.name) {
        SDL_free(get_dir_operation.name);
        get_dir_operation.name = NULL;
    }
}

static void clear_path_operation(PathOperation* operation) {
    if (operation->path) {
        SDL_free(operation->path);
        operation->path = NULL;
    }
}

static void clear_registered_operation_state(void) {
    clear_open_operation();
    clear_get_dir_operation();
    clear_path_operation(&mkdir_operation);
    clear_path_operation(&delete_operation);
}

// Helpers
static int normalize_mc_port(int port) {
    return port == 1 ? 1 : 0;
}

static const char* get_mc_slot_dir(int port) {
    return normalize_mc_port(port) == 0 ? MC_SLOT1_DIR : MC_SLOT2_DIR;
}

static char* get_mc_root_path(int port) {
    const char* data_path = Paths_GetDataPath();
    char* saves_dir = NULL;
    char* slot_dir = NULL;

    if (data_path == NULL || SDL_asprintf(&saves_dir, "%s%s", data_path, MC_SAVES_DIR) < 0 || saves_dir == NULL) {
        return NULL;
    }

    if (!SDL_CreateDirectory(saves_dir)) {
        SDL_Log("Couldn't create Memory Card directory '%s': %s", saves_dir, SDL_GetError());
        SDL_free(saves_dir);
        return NULL;
    }

    if (SDL_asprintf(&slot_dir, "%s%s", saves_dir, get_mc_slot_dir(port)) < 0 || slot_dir == NULL) {
        SDL_free(saves_dir);
        return NULL;
    }

    if (!SDL_CreateDirectory(slot_dir)) {
        SDL_Log("Couldn't create Memory Card slot directory '%s': %s", slot_dir, SDL_GetError());
        SDL_free(slot_dir);
        slot_dir = NULL;
    }

    SDL_free(saves_dir);
    return slot_dir;
}

static char* get_mc_path(int port, const char* name) {
    char* root_path = get_mc_root_path(port);
    char* full_path = NULL;

    if (root_path == NULL || name == NULL) {
        SDL_free(root_path);
        return NULL;
    }

    if (name[0] == '/') {
        name++; // Skip leading slash if any
    }

    if (SDL_asprintf(&full_path, "%s%s", root_path, name) < 0) {
        full_path = NULL;
    }
    SDL_free(root_path);
    return full_path;
}

static int alloc_fd() {
    for (int i = 0; i < MAX_OPEN_FILES; i++) {
        if (open_files[i].stream == NULL) {
            return i;
        }
    }
    return -1;
}

static void clear_open_file(OpenFile* file) {
    SDL_free(file->final_path);
    SDL_free(file->temporary_path);
    SDL_zerop(file);
}

static void discard_open_file(OpenFile* file) {
    if (file->stream != NULL) {
        SDL_CloseIO(file->stream);
        file->stream = NULL;
    }

    if (file->temporary_path != NULL) {
        SDL_RemovePath(file->temporary_path);
    }

    clear_open_file(file);
}

static char* create_temporary_mc_path(const char* final_path, int fd) {
    char* temporary_path = NULL;

    if (SDL_asprintf(&temporary_path,
                     "%s.3sx-tmp-%016llx-%08x-%d",
                     final_path,
                     (unsigned long long)SDL_GetTicksNS(),
                     SDL_rand_bits(),
                     fd) < 0) {
        return NULL;
    }

    return temporary_path;
}

// Finalizers
static void finalize_get_info(int* result) {
    char* root_path = get_mc_root_path(get_info_operation.port);

    if (root_path == NULL) {
        *get_info_operation.type = 0;
        *get_info_operation.free = 0;
        *get_info_operation.format = 0;
        *result = sceMcResDeniedPermit;
        return;
    }

    // Pretend we have a formatted 8MB PS2 memory card
    *get_info_operation.type = sceMcTypePS2;
    *get_info_operation.free = 0x1F03; // ~8000 KB free
    *get_info_operation.format = 1;

    SDL_free(root_path);
    *result = sceMcResSucceed;
}

static void finalize_open(int* result) {
    *result = open_operation.fd;
}

static void finalize_close(int* result) {
    *result = close_operation.result;
}

static void finalize_read(int* result) {
    if (rw_operation.fd >= 0 && rw_operation.fd < MAX_OPEN_FILES && open_files[rw_operation.fd].stream &&
        rw_operation.read_buf != NULL && rw_operation.length >= 0) {
        SDL_IOStream* stream = open_files[rw_operation.fd].stream;
        const size_t read = SDL_ReadIO(stream, rw_operation.read_buf, (size_t)rw_operation.length);
        const bool read_succeeded = read == (size_t)rw_operation.length &&
                                    SDL_GetIOStatus(stream) != SDL_IO_STATUS_ERROR;
        *result = read_succeeded ? (int)read : sceMcResDeniedPermit;

        if (!read_succeeded) {
            discard_open_file(&open_files[rw_operation.fd]);
        }
    } else {
        *result = sceMcResNoEntry;
    }
}

static void finalize_write(int* result) {
    if (rw_operation.fd >= 0 && rw_operation.fd < MAX_OPEN_FILES && open_files[rw_operation.fd].stream &&
        rw_operation.write_buf != NULL && rw_operation.length >= 0) {
        OpenFile* file = &open_files[rw_operation.fd];
        const size_t written = SDL_WriteIO(file->stream, rw_operation.write_buf, (size_t)rw_operation.length);
        file->write_failed = file->write_failed || written != (size_t)rw_operation.length ||
                             SDL_GetIOStatus(file->stream) == SDL_IO_STATUS_ERROR;
        *result = file->write_failed ? sceMcResFullDevice : (int)written;

        if (file->write_failed) {
            discard_open_file(file);
        }
    } else {
        *result = sceMcResNoEntry;
    }
}

static void finalize_mkdir(int* result) {
    *result = mkdir_operation.path != NULL && SDL_CreateDirectory(mkdir_operation.path) ? sceMcResSucceed
                                                                                       : sceMcResDeniedPermit;
}

static void finalize_delete(int* result) {
    if (delete_operation.path == NULL) {
        *result = sceMcResDeniedPermit;
        return;
    }

    SDL_PathInfo info;
    if (!SDL_GetPathInfo(delete_operation.path, &info)) {
        *result = sceMcResNoEntry;
        return;
    }

    *result = SDL_RemovePath(delete_operation.path) ? sceMcResSucceed : sceMcResDeniedPermit;
}

typedef struct {
    int maxent;
    int count;
    sceMcTblGetDir* table;
    const char* pattern;
} EnumerateDirData;

static void fill_mc_datetime(sceMcStDateTime* dst, SDL_Time time_value);

static void fill_dir_entry(sceMcTblGetDir* entry, const char* entry_name, const SDL_PathInfo* info) {
    SDL_memset(entry, 0, sizeof(*entry));
    SDL_strlcpy((char*)entry->EntryName, entry_name, sizeof(entry->EntryName));
    entry->FileSizeByte = info->size;

    if (info->type == SDL_PATHTYPE_DIRECTORY) {
        entry->AttrFile = sceMcFileAttrSubdir;
    } else {
        entry->AttrFile = sceMcFileAttrReadable | sceMcFileAttrWritable;
    }

    fill_mc_datetime(&entry->_Create, info->create_time);
    fill_mc_datetime(&entry->_Modify, info->modify_time);
}

static void fill_mc_datetime(sceMcStDateTime* dst, SDL_Time time_value) {
    SDL_DateTime dt;

    SDL_memset(dst, 0, sizeof(*dst));

    if (time_value > 0 && SDL_TimeToDateTime(time_value, &dt, true)) {
        dst->Sec = (unsigned char)dt.second;
        dst->Min = (unsigned char)dt.minute;
        dst->Hour = (unsigned char)dt.hour;
        dst->Day = (unsigned char)dt.day;
        dst->Month = (unsigned char)dt.month;
        dst->Year = (unsigned short)dt.year;
        return;
    }

    dst->Sec = 0;
    dst->Min = 0;
    dst->Hour = 12;
    dst->Day = 1;
    dst->Month = 1;
    dst->Year = 2026;
}

// Simple wildcard match (only matches * at the end for now)
static bool match_pattern(const char* name, const char* pattern) {
    if (!pattern || pattern[0] == '\0' || (pattern[0] == '*' && pattern[1] == '\0')) {
        return true;
    }
    
    int len = SDL_strlen(pattern);
    if (len > 0 && pattern[len-1] == '*') {
        return SDL_strncmp(name, pattern, len - 1) == 0;
    }
    
    return SDL_strcmp(name, pattern) == 0;
}

static int SDLCALL get_dir_callback(void* userdata, const char* dirname, const char* fname) {
    EnumerateDirData* data = (EnumerateDirData*)userdata;
    
    if (data->count >= data->maxent) {
        return 0; // stop enumerating
    }
    
    if (!match_pattern(fname, data->pattern)) {
        return 1; // skip this file, but continue
    }
    
    char* full_path = NULL;

    if (dirname && dirname[0] != '\0') {
        const char* separator = "";
        size_t dirname_len = SDL_strlen(dirname);

        if (dirname[dirname_len - 1] != '/' && dirname[dirname_len - 1] != '\\') {
            separator = "/";
        }

        SDL_asprintf(&full_path, "%s%s%s", dirname, separator, fname);
    } else {
        SDL_asprintf(&full_path, "%s", fname);
    }
    
    SDL_PathInfo info;
    if (SDL_GetPathInfo(full_path, &info)) {
        sceMcTblGetDir* entry = &data->table[data->count];
        fill_dir_entry(entry, fname, &info);
        data->count++;
    }
    
    SDL_free(full_path);
    return 1; // continue
}

static void finalize_get_dir(int* result) {
    char* path = get_mc_root_path(get_dir_operation.port);

    if (path == NULL) {
        *result = sceMcResDeniedPermit;
        return;
    }
    
    // The name passed to sceMcGetDir often has a leading slash
    const char* full_req = get_dir_operation.name;
    if (full_req && full_req[0] == '/') {
        full_req++;
    }
    
    const char* pattern = full_req ? full_req : "";
    const char* last_slash = SDL_strrchr(pattern, '/');
    const bool has_wildcard = pattern && SDL_strchr(pattern, '*') != NULL;

    if (!has_wildcard && pattern[0] != '\0' && get_dir_operation.maxent > 0) {
        char* exact_path = get_mc_path(get_dir_operation.port, full_req);
        SDL_PathInfo info;

        if (exact_path && SDL_GetPathInfo(exact_path, &info)) {
            const char* entry_name = pattern;
            const char* basename = SDL_strrchr(pattern, '/');

            if (basename != NULL) {
                entry_name = basename + 1;
            }

            fill_dir_entry(&get_dir_operation.table[0], entry_name, &info);
            debug_print("sceMcGetDir: exact=%s found=1", exact_path);
            SDL_free(exact_path);
            SDL_free(path);
            *result = 1;
            return;
        }

        SDL_free(exact_path);
    }
    
    if (last_slash) {
        int dir_len = (int)(last_slash - pattern);
        char* dir_part = SDL_malloc(dir_len + 1);
        SDL_strlcpy(dir_part, pattern, dir_len + 1);
        
        pattern = last_slash + 1;
        
        char* new_path = NULL;
        SDL_asprintf(&new_path, "%s%s/", path, dir_part);
        SDL_free(path);
        path = new_path;
        
        SDL_free(dir_part);
    }
    
    EnumerateDirData data;
    data.maxent = get_dir_operation.maxent;
    data.count = 0;
    data.table = get_dir_operation.table;
    data.pattern = pattern;

    SDL_PathInfo directory_info = { 0 };

    if (!SDL_GetPathInfo(path, &directory_info) || directory_info.type != SDL_PATHTYPE_DIRECTORY) {
        SDL_free(path);
        *result = 0;
        return;
    }

    if (!SDL_EnumerateDirectory(path, get_dir_callback, &data)) {
        SDL_Log("Couldn't enumerate Memory Card directory '%s': %s", path, SDL_GetError());
        SDL_free(path);
        *result = sceMcResDeniedPermit;
        return;
    }
    
    debug_print("sceMcGetDir: path=%s pattern=%s found=%d", path, pattern, data.count);
    
    SDL_free(path);
    
    *result = data.count;
}

static OperationFinalizer finalizers[30] = { 0 };

int sceMcInit(void) {
    // Register finalizers
    finalizers[sceMcFuncNoCardInfo] = finalize_get_info;
    finalizers[sceMcFuncNoOpen] = finalize_open;
    finalizers[sceMcFuncNoClose] = finalize_close;
    finalizers[sceMcFuncNoRead] = finalize_read;
    finalizers[sceMcFuncNoWrite] = finalize_write;
    finalizers[sceMcFuncNoMkdir] = finalize_mkdir;
    finalizers[sceMcFuncNoDelete] = finalize_delete;
    finalizers[sceMcFuncNoGetDir] = finalize_get_dir;
    return sceMcIniSucceed;
}

int sceMcSync(int mode, int* cmd, int* result) {
    if (registered_operation > 0) {
        if (cmd) *cmd = registered_operation;
        if (finalizers[registered_operation]) {
            finalizers[registered_operation](result);
        } else {
            *result = sceMcResSucceed;
        }

        clear_registered_operation_state();
        registered_operation = 0;
        return sceMcExecFinish;
    } else {
        return sceMcExecIdle;
    }
}

int sceMcGetInfo(int port, int slot, int* type, int* free, int* format) {
    registered_operation = sceMcFuncNoCardInfo;
    get_info_operation.port = normalize_mc_port(port);
    get_info_operation.type = type;
    get_info_operation.free = free;
    get_info_operation.format = format;
    return 0;
}

int sceMcOpen(int port, int slot, const char* name, int mode) {
    registered_operation = sceMcFuncNoOpen;
    clear_open_operation();
    open_operation.port = normalize_mc_port(port);
    open_operation.path = get_mc_path(open_operation.port, name);

    if (open_operation.path == NULL) {
        open_operation.fd = sceMcResDeniedPermit;
        return 0;
    }
    
    int fd = alloc_fd();
    if (fd == -1) {
        open_operation.fd = sceMcResUpLimitHandle;
        return 0;
    }
    
    OpenFile* open_file = &open_files[fd];
    const bool atomic_write = (mode & SCE_STM_W) != 0 && (mode & MC_OPEN_CREATE) == 0;
    const char* sdl_mode = "rb";

    if (atomic_write) {
        open_file->final_path = SDL_strdup(open_operation.path);
        open_file->temporary_path = create_temporary_mc_path(open_operation.path, fd);
        SDL_PathInfo info;
        const bool target_exists = SDL_GetPathInfo(open_operation.path, &info) && info.type == SDL_PATHTYPE_FILE;

        if (target_exists && open_file->final_path != NULL && open_file->temporary_path != NULL) {
            open_file->stream = SDL_IOFromFile(open_file->temporary_path, "w+b");
            open_file->atomic_write = open_file->stream != NULL;
        }

        sdl_mode = "atomic-w+b";
    } else if (mode & MC_OPEN_CREATE) {
        SDL_PathInfo info;
        const bool already_exists = SDL_GetPathInfo(open_operation.path, &info) && info.type == SDL_PATHTYPE_FILE;
        sdl_mode = already_exists ? "r+b" : "w+b";
        open_file->stream = SDL_IOFromFile(open_operation.path, sdl_mode);
    } else {
        open_file->stream = SDL_IOFromFile(open_operation.path, sdl_mode);
    }

    debug_print("sceMcOpen: file=%s mode=%d mapped=%s", name, mode, sdl_mode);

    if (open_file->stream != NULL) {
        open_operation.fd = fd;
    } else {
        if (open_file->temporary_path != NULL) {
            SDL_RemovePath(open_file->temporary_path);
        }
        clear_open_file(open_file);
        open_operation.fd = sceMcResNoEntry;
    }
    
    return 0;
}

int sceMcClose(int fd) {
    registered_operation = sceMcFuncNoClose;
    close_operation.fd = fd;
    close_operation.result = sceMcResNoEntry;
    if (fd >= 0 && fd < MAX_OPEN_FILES && open_files[fd].stream) {
        OpenFile* file = &open_files[fd];
        const bool close_succeeded = SDL_CloseIO(file->stream);
        file->stream = NULL;
        bool commit_succeeded = close_succeeded && !file->write_failed;

        if (file->atomic_write) {
            if (commit_succeeded) {
                commit_succeeded = SDL_RenamePath(file->temporary_path, file->final_path);
            }

            if (!commit_succeeded) {
                SDL_RemovePath(file->temporary_path);
            }
        }

        close_operation.result = commit_succeeded ? sceMcResSucceed : sceMcResDeniedPermit;
        clear_open_file(file);
    }
    return 0;
}

int sceMcRead(int fd, void* buffer, int size) {
    registered_operation = sceMcFuncNoRead;
    rw_operation.fd = fd;
    rw_operation.read_buf = buffer;
    rw_operation.length = size;
    return 0;
}

int sceMcWrite(int fd, const void* buffer, int size) {
    registered_operation = sceMcFuncNoWrite;
    rw_operation.fd = fd;
    rw_operation.write_buf = buffer;
    rw_operation.length = size;
    return 0;
}

int sceMcMkdir(int port, int slot, const char* name) {
    registered_operation = sceMcFuncNoMkdir;
    clear_path_operation(&mkdir_operation);
    mkdir_operation.port = normalize_mc_port(port);
    mkdir_operation.path = get_mc_path(mkdir_operation.port, name);
    debug_print("sceMcMkdir: %s", name);
    return 0;
}

int sceMcDelete(int port, int slot, const char* name) {
    registered_operation = sceMcFuncNoDelete;
    clear_path_operation(&delete_operation);
    delete_operation.port = normalize_mc_port(port);
    delete_operation.path = get_mc_path(delete_operation.port, name);
    debug_print("sceMcDelete: %s", name);
    return 0;
}

int sceMcFormat(int port, int slot) {
    not_implemented(__func__);
    return 0;
}

int sceMcUnformat(int port, int slot) {
    not_implemented(__func__);
    return 0;
}

int sceMcGetDir(int port, int slot, const char* name, unsigned int mode, int maxent, sceMcTblGetDir* table) {
    registered_operation = sceMcFuncNoGetDir;
    get_dir_operation.port = normalize_mc_port(port);
    get_dir_operation.name = SDL_strdup(name);
    get_dir_operation.mode = mode;
    get_dir_operation.maxent = maxent;
    get_dir_operation.table = table;
    return 0;
}
