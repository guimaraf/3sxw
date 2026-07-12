#include "port/paths.h"

#include <SDL3/SDL.h>

#include <string.h>

static const char* pref_path = NULL;

const char* Paths_GetPrefPath() {
    if (pref_path == NULL) {
        pref_path = SDL_GetBasePath();
    }

    return pref_path;
}

const char* Paths_GetBasePath() {
    return SDL_GetBasePath();
}

static const char* data_path = NULL;

const char* Paths_GetDataPath() {
    if (data_path == NULL) {
        const char* base = SDL_GetBasePath();

        if (base == NULL) {
            return NULL;
        }

        char* full_path = NULL;

        if (SDL_asprintf(&full_path, "%sdata/", base) < 0 || full_path == NULL) {
            return NULL;
        }

        if (!SDL_CreateDirectory(full_path)) {
            SDL_free(full_path);
            return NULL;
        }

        data_path = full_path;
    }

    return data_path;
}

bool Paths_ValidatePortableStorage(char* error, size_t error_size) {
    static const char probe_contents[] = "3SX portable storage probe";
    const char* base_path = SDL_GetBasePath();

    if (base_path == NULL) {
        SDL_snprintf(error, error_size, "Couldn't locate the game directory: %s", SDL_GetError());
        return false;
    }

    const char* portable_data_path = Paths_GetDataPath();

    if (portable_data_path == NULL || !SDL_CreateDirectory(portable_data_path)) {
        SDL_snprintf(error,
                     error_size,
                     "Couldn't create the portable data directory:\n%sdata/\n\n%s",
                     base_path,
                     SDL_GetError());
        return false;
    }

    char* probe_path = NULL;

    if (SDL_asprintf(&probe_path,
                     "%s.3sx-write-test-%016llx-%08x.tmp",
                     portable_data_path,
                     (unsigned long long)SDL_GetTicksNS(),
                     SDL_rand_bits()) < 0 ||
        probe_path == NULL) {
        SDL_snprintf(error, error_size, "Couldn't allocate the portable storage test path.");
        return false;
    }

    SDL_IOStream* io = SDL_IOFromFile(probe_path, "wb");

    if (io == NULL) {
        SDL_snprintf(error, error_size, "Couldn't create a file in:\n%s\n\n%s", portable_data_path, SDL_GetError());
        SDL_free(probe_path);
        return false;
    }

    const size_t probe_size = sizeof(probe_contents);
    const size_t written = SDL_WriteIO(io, probe_contents, probe_size);
    const bool write_closed = SDL_CloseIO(io);

    if (written != probe_size || !write_closed) {
        SDL_snprintf(error, error_size, "Couldn't write a complete file in:\n%s\n\n%s", portable_data_path, SDL_GetError());
        SDL_RemovePath(probe_path);
        SDL_free(probe_path);
        return false;
    }

    char read_buffer[sizeof(probe_contents)] = { 0 };
    io = SDL_IOFromFile(probe_path, "rb");

    if (io == NULL) {
        SDL_snprintf(error, error_size, "Couldn't reopen a file in:\n%s\n\n%s", portable_data_path, SDL_GetError());
        SDL_RemovePath(probe_path);
        SDL_free(probe_path);
        return false;
    }

    const size_t bytes_read = SDL_ReadIO(io, read_buffer, sizeof(read_buffer));
    const bool read_closed = SDL_CloseIO(io);
    const bool contents_match = bytes_read == probe_size && memcmp(read_buffer, probe_contents, probe_size) == 0;

    if (!read_closed || !contents_match) {
        SDL_snprintf(error, error_size, "Couldn't verify a file written in:\n%s\n\n%s", portable_data_path, SDL_GetError());
        SDL_RemovePath(probe_path);
        SDL_free(probe_path);
        return false;
    }

    if (!SDL_RemovePath(probe_path)) {
        SDL_snprintf(error, error_size, "Couldn't remove a temporary file from:\n%s\n\n%s", portable_data_path, SDL_GetError());
        SDL_free(probe_path);
        return false;
    }

    SDL_free(probe_path);
    return true;
}
