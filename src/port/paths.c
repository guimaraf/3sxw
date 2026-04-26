#include "port/paths.h"

#include <SDL3/SDL.h>

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
        char* full_path = NULL;
        SDL_asprintf(&full_path, "%sdata/", base);
        SDL_CreateDirectory(full_path);
        data_path = full_path;
    }

    return data_path;
}
