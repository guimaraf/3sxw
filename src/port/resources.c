#include "port/resources.h"
#include "port/paths.h"
#include "utils/sha256.h"

#include <SDL3/SDL.h>
#include <cdio/iso9660.h>

#define EXPECTED_AFS_SHA "f9fa50f3a124ec9fa9465aa9c8546c2d867887eb39f711a070762a0324ba5604"
#define ERROR_LEN_MAX 512
#define CHUNK_SECTORS 16
#define BUFFER_SIZE (ISO_BLOCKSIZE * CHUNK_SECTORS)

typedef enum FlowState { INIT, DIALOG_OPENED, COPY_ERROR, COPY_SUCCESS } ResourceCopyingFlowState;
typedef enum DialogResult { DIALOG_RESULT_NONE, DIALOG_RESULT_SELECTED, DIALOG_RESULT_CANCELED, DIALOG_RESULT_ERROR } DialogResult;

static ResourceCopyingFlowState flow_state = INIT;
static SDL_Window* dialog_owner_window = NULL;
static char error[ERROR_LEN_MAX] = { 0 };
static const char* afs_path = NULL;
static SDL_Mutex* dialog_result_mutex = NULL;
static DialogResult dialog_result = DIALOG_RESULT_NONE;
static char* selected_iso_path = NULL;
static char dialog_error[ERROR_LEN_MAX] = { 0 };
static const SDL_DialogFileFilter iso_dialog_filter = { .name = "Game ISO", .pattern = "iso" };

static void create_dialog_parent_window() {
    if (dialog_owner_window != NULL) {
        return;
    }

    dialog_owner_window = SDL_CreateWindow("3SX", 1, 1, SDL_WINDOW_HIDDEN);

    if (dialog_owner_window == NULL) {
        return;
    }

    SDL_ShowWindow(dialog_owner_window);
    SDL_RaiseWindow(dialog_owner_window);
}

static void destroy_dialog_owner_window() {
    if (dialog_owner_window == NULL) {
        return;
    }

    SDL_DestroyWindow(dialog_owner_window);
    dialog_owner_window = NULL;
}

static bool file_exists(const char* path) {
    SDL_PathInfo path_info = { 0 };
    return SDL_GetPathInfo(path, &path_info) && path_info.type == SDL_PATHTYPE_FILE;
}

static bool create_resources_directory() {
    char* path = Resources_GetPath(NULL);
    const bool created = path != NULL && SDL_CreateDirectory(path);

    if (!created) {
        SDL_snprintf(error,
                     ERROR_LEN_MAX,
                     "Failed to create the resources directory:\n%s\n\n%s",
                     path != NULL ? path : "<unavailable>",
                     SDL_GetError());
    }

    SDL_free(path);
    return created;
}

static void open_file_dialog_callback(void* userdata, const char* const* filelist, int filter) {
    (void)userdata;
    (void)filter;

    DialogResult result = DIALOG_RESULT_SELECTED;
    char* iso_path = NULL;
    char callback_error[ERROR_LEN_MAX] = { 0 };

    if (filelist == NULL) {
        result = DIALOG_RESULT_ERROR;
        SDL_snprintf(callback_error, sizeof(callback_error), "The ISO selection dialog failed:\n\n%s", SDL_GetError());
    } else if (filelist[0] == NULL) {
        result = DIALOG_RESULT_CANCELED;
    } else {
        iso_path = SDL_strdup(filelist[0]);

        if (iso_path == NULL) {
            result = DIALOG_RESULT_ERROR;
            SDL_snprintf(callback_error, sizeof(callback_error), "Couldn't store the selected ISO path:\n\n%s", SDL_GetError());
        }
    }

    if (dialog_result_mutex == NULL) {
        SDL_free(iso_path);
        return;
    }

    SDL_LockMutex(dialog_result_mutex);
    SDL_free(selected_iso_path);
    selected_iso_path = iso_path;
    dialog_result = result;
    SDL_strlcpy(dialog_error, callback_error, sizeof(dialog_error));
    SDL_UnlockMutex(dialog_result_mutex);
}

static bool copy_resources_from_iso(const char* iso_path) {
    iso9660_t* iso = iso9660_open(iso_path);

    if (iso == NULL) {
        SDL_snprintf(error, ERROR_LEN_MAX, "Failed to open the selected ISO:\n%s", iso_path);
        return false;
    }

    iso9660_stat_t* stat = iso9660_ifs_stat(iso, "/THIRD/SF33RD.AFS;1");

    if (stat == NULL) {
        stat = iso9660_ifs_stat(iso, "/SF33RD.AFS;1");

        if (stat == NULL) {
            iso9660_close(iso);
            SDL_snprintf(error,
                         ERROR_LEN_MAX,
                         "The required SF33RD.AFS file was not found in the selected ISO:\n%s",
                         iso_path);
            return false;
        }
    }

    if (!create_resources_directory()) {
        iso9660_stat_free(stat);
        iso9660_close(iso);
        return false;
    }

    const char* dst_path = Resources_GetAFSPath();
    SDL_IOStream* dst_io = SDL_IOFromFile(dst_path, "wb");

    if (dst_io == NULL) {
        iso9660_stat_free(stat);
        iso9660_close(iso);
        SDL_snprintf(error, ERROR_LEN_MAX, "Failed to create the resource file:\n%s\n\n%s", dst_path, SDL_GetError());
        return false;
    }

    uint8_t buffer[BUFFER_SIZE];
    uint64_t bytes_remaining = stat->total_size;
    lsn_t current_lsn = stat->lsn;
    bool copy_failed = false;

#if CHECKSUM
    sha256 sha;
    sha256_init(&sha);
#endif

    while (bytes_remaining > 0) {
        const size_t bytes_to_read = (size_t)SDL_min(sizeof(buffer), bytes_remaining);
        const uint64_t sectors_to_read = (bytes_to_read + ISO_BLOCKSIZE - 1) / ISO_BLOCKSIZE;
        const long bytes_read = iso9660_iso_seek_read(iso, buffer, current_lsn, sectors_to_read);

        if (bytes_read < (long)bytes_to_read || bytes_read > (long)sizeof(buffer)) {
            SDL_snprintf(error,
                         ERROR_LEN_MAX,
                         "Failed to read game resources from ISO:\n%s\n\nExpected %zu bytes, received %ld.",
                         iso_path,
                         bytes_to_read,
                         bytes_read);
            copy_failed = true;
            break;
        }

        const size_t bytes_written = SDL_WriteIO(dst_io, buffer, bytes_to_read);

        if (bytes_written != bytes_to_read) {
            SDL_snprintf(error,
                         ERROR_LEN_MAX,
                         "Failed to write the complete resource file:\n%s\n\n%s",
                         dst_path,
                         SDL_GetError());
            copy_failed = true;
            break;
        }

#if CHECKSUM
        sha256_append(&sha, buffer, bytes_to_read);
#endif

        bytes_remaining -= bytes_to_read;
        current_lsn += sectors_to_read;
    }

    iso9660_stat_free(stat);
    iso9660_close(iso);

    if (!SDL_CloseIO(dst_io)) {
        SDL_snprintf(error, ERROR_LEN_MAX, "Failed to finish writing the resource file:\n%s\n\n%s", dst_path, SDL_GetError());
        copy_failed = true;
    }

    if (copy_failed) {
        SDL_RemovePath(dst_path);
        return false;
    }

#if CHECKSUM
    char hex[SHA256_HEX_SIZE];
    sha256_finalize_hex(&sha, hex);

    if (SDL_strncmp(hex, EXPECTED_AFS_SHA, sizeof(hex)) == 0) {
        return true;
    }

    SDL_snprintf(error,
                 ERROR_LEN_MAX,
                 "Incorrect checksum for the copied resource:\n%s\n\nExpected %s, got %s.",
                 dst_path,
                 EXPECTED_AFS_SHA,
                 hex);
    SDL_RemovePath(dst_path);
    return false;
#else
    return true;
#endif
}

static void open_dialog() {
    if (dialog_result_mutex == NULL) {
        dialog_result_mutex = SDL_CreateMutex();

        if (dialog_result_mutex == NULL) {
            SDL_snprintf(error, ERROR_LEN_MAX, "Failed to initialize the ISO selection dialog:\n\n%s", SDL_GetError());
            flow_state = COPY_ERROR;
            return;
        }
    }

    SDL_LockMutex(dialog_result_mutex);
    SDL_free(selected_iso_path);
    selected_iso_path = NULL;
    dialog_result = DIALOG_RESULT_NONE;
    dialog_error[0] = '\0';
    SDL_UnlockMutex(dialog_result_mutex);

    flow_state = DIALOG_OPENED;
    SDL_ShowOpenFileDialog(
        open_file_dialog_callback, NULL, dialog_owner_window, &iso_dialog_filter, 1, NULL, false);
}

static void process_dialog_result() {
    DialogResult result = DIALOG_RESULT_NONE;
    char* iso_path = NULL;
    char result_error[ERROR_LEN_MAX] = { 0 };

    SDL_LockMutex(dialog_result_mutex);
    result = dialog_result;

    if (result != DIALOG_RESULT_NONE) {
        iso_path = selected_iso_path;
        selected_iso_path = NULL;
        dialog_result = DIALOG_RESULT_NONE;
        SDL_strlcpy(result_error, dialog_error, sizeof(result_error));
        dialog_error[0] = '\0';
    }

    SDL_UnlockMutex(dialog_result_mutex);

    switch (result) {
    case DIALOG_RESULT_NONE:
        break;

    case DIALOG_RESULT_SELECTED:
        flow_state = copy_resources_from_iso(iso_path) ? COPY_SUCCESS : COPY_ERROR;
        break;

    case DIALOG_RESULT_CANCELED:
        SDL_snprintf(error,
                     ERROR_LEN_MAX,
                     "ISO selection was canceled. The required resource is still missing:\n%s",
                     Resources_GetAFSPath());
        flow_state = COPY_ERROR;
        break;

    case DIALOG_RESULT_ERROR:
        SDL_strlcpy(error, result_error, sizeof(error));
        flow_state = COPY_ERROR;
        break;
    }

    SDL_free(iso_path);
}

char* Resources_GetPath(const char* file_path) {
    const char* base = Paths_GetPrefPath();
    char* full_path = NULL;

    if (file_path == NULL) {
        SDL_asprintf(&full_path, "%sresources/", base);
    } else {
        SDL_asprintf(&full_path, "%sresources/%s", base, file_path);
    }

    return full_path;
}

bool Resources_Check() {
    const char* afs_path = Resources_GetAFSPath();
    const bool afs_present = file_exists(afs_path);

    if (!afs_present) {
        return false;
    }

#if CHECKSUM
    sha256 sha;
    sha256_init(&sha);

    const size_t chunk_size = 10 * 1024;
    void* buf = SDL_malloc(chunk_size);
    SDL_IOStream* io = SDL_IOFromFile(afs_path, "rb");

    if (buf == NULL || io == NULL) {
        SDL_free(buf);
        if (io != NULL) {
            SDL_CloseIO(io);
        }
        return false;
    }

    while (true) {
        const size_t bytes_read = SDL_ReadIO(io, buf, chunk_size);

        if (bytes_read == 0) {
            break;
        }

        sha256_append(&sha, buf, bytes_read);
    }

    SDL_free(buf);
    const bool read_failed = SDL_GetIOStatus(io) == SDL_IO_STATUS_ERROR;
    const bool close_failed = !SDL_CloseIO(io);

    if (read_failed || close_failed) {
        return false;
    }

    char hex[SHA256_HEX_SIZE];
    sha256_finalize_hex(&sha, hex);
    return SDL_strncmp(hex, EXPECTED_AFS_SHA, sizeof(hex)) == 0;
#else
    return true;
#endif
}

bool Resources_RunResourceCopyingFlow() {
    switch (flow_state) {
    case INIT:
        create_dialog_parent_window();
        const char* required_path = Resources_GetAFSPath();
        char* missing_message = NULL;
        SDL_asprintf(&missing_message,
                     "3SX requires the following game resource file:\n\n%s\n\nPlace the correctly named file at this path, or choose a valid Street Fighter III: 3rd Strike ISO in the next dialog.",
                     required_path);
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION,
                                 "Required game resource is missing",
                                 missing_message != NULL ? missing_message : required_path,
                                 dialog_owner_window);
        SDL_free(missing_message);
        open_dialog();
        break;

    case DIALOG_OPENED:
        process_dialog_result();
        break;

    case COPY_ERROR:
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", error, dialog_owner_window);
        open_dialog();
        break;

    case COPY_SUCCESS:
        char* resources_path = Resources_GetPath(NULL);
        char* message = NULL;
        SDL_asprintf(&message, "You can find them at:\n%s", resources_path);
        SDL_ShowSimpleMessageBox(
            SDL_MESSAGEBOX_INFORMATION, "Resources copied successfully", message, dialog_owner_window);
        SDL_free(resources_path);
        SDL_free(message);
        destroy_dialog_owner_window();
        SDL_DestroyMutex(dialog_result_mutex);
        dialog_result_mutex = NULL;
        flow_state = INIT;
        return true;
    }

    return false;
}

const char* Resources_GetAFSPath() {
    if (afs_path == NULL) {
        afs_path = Resources_GetPath("SF33RD.AFS");
    }

    return afs_path;
}
