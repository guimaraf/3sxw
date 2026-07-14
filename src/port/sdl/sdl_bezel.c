#include "port/sdl/sdl_bezel.h"
#include "port/paths.h"
#include "port/utils.h"

#include <SDL3/SDL.h>

#define BEZEL_MAX_DIMENSION 8192

static SDL_Texture* bezel_texture = NULL;

static bool dimensions_are_16_by_9(int width, int height) {
    if (width <= 0 || height <= 0) {
        return false;
    }

    Sint64 difference = ((Sint64)width * 9) - ((Sint64)height * 16);

    if (difference < 0) {
        difference = -difference;
    }

    // Accept common near-16:9 modes such as 1366x768 without accepting 16:10.
    return difference <= ((Sint64)height / 10);
}

static bool center_is_transparent(SDL_Surface* surface, const char* path) {
    if (!SDL_LockSurface(surface)) {
        log_error("Couldn't inspect the bezel transparency %s: %s", path, SDL_GetError());
        return false;
    }

    const int center_x = surface->w / 2;
    const int center_y = surface->h / 2;
    const Uint8* center_pixel =
        (const Uint8*)surface->pixels + ((size_t)center_y * (size_t)surface->pitch) + ((size_t)center_x * 4);
    const Uint8 center_alpha = center_pixel[3];
    SDL_UnlockSurface(surface);

    if (center_alpha != SDL_ALPHA_TRANSPARENT) {
        log_error("Bezel image %s must have a fully transparent center.", path);
        return false;
    }

    return true;
}

static SDL_Surface* load_bezel_surface(const char* path) {
    SDL_Surface* loaded_surface = SDL_LoadPNG(path);

    if (loaded_surface == NULL) {
        log_error("Couldn't load bezel image %s: %s", path, SDL_GetError());
        return NULL;
    }

    if (loaded_surface->w > BEZEL_MAX_DIMENSION || loaded_surface->h > BEZEL_MAX_DIMENSION ||
        !dimensions_are_16_by_9(loaded_surface->w, loaded_surface->h)) {
        log_error("Bezel image %s must use a 16:9 resolution up to %dx%d; received %dx%d.",
                  path,
                  BEZEL_MAX_DIMENSION,
                  BEZEL_MAX_DIMENSION,
                  loaded_surface->w,
                  loaded_surface->h);
        SDL_DestroySurface(loaded_surface);
        return NULL;
    }

    SDL_Surface* rgba_surface = SDL_ConvertSurface(loaded_surface, SDL_PIXELFORMAT_RGBA32);
    SDL_DestroySurface(loaded_surface);

    if (rgba_surface == NULL) {
        log_error("Couldn't convert bezel image %s to RGBA: %s", path, SDL_GetError());
        return NULL;
    }

    if (!center_is_transparent(rgba_surface, path)) {
        SDL_DestroySurface(rgba_surface);
        return NULL;
    }

    return rgba_surface;
}

bool SDLBezel_Init(SDL_Renderer* renderer) {
    if (bezel_texture != NULL) {
        return true;
    }

    if (renderer == NULL) {
        log_error("Couldn't initialize the bezel without an SDL renderer.");
        return false;
    }

    const char* data_path = Paths_GetDataPath();

    if (data_path == NULL) {
        log_error("Couldn't locate the portable data directory for the bezel: %s", SDL_GetError());
        return false;
    }

    char* bezel_path = NULL;

    if (SDL_asprintf(&bezel_path, "%simg/bezel.png", data_path) < 0 || bezel_path == NULL) {
        log_error("Couldn't allocate the bezel image path.");
        return false;
    }

    SDL_Surface* surface = load_bezel_surface(bezel_path);

    if (surface == NULL) {
        SDL_free(bezel_path);
        return false;
    }

    bezel_texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_DestroySurface(surface);

    if (bezel_texture == NULL) {
        log_error("Couldn't create the bezel texture %s: %s", bezel_path, SDL_GetError());
        SDL_free(bezel_path);
        return false;
    }

    if (!SDL_SetTextureBlendMode(bezel_texture, SDL_BLENDMODE_BLEND) ||
        !SDL_SetTextureScaleMode(bezel_texture, SDL_SCALEMODE_LINEAR)) {
        log_error("Couldn't configure the bezel texture %s: %s", bezel_path, SDL_GetError());
        SDL_DestroyTexture(bezel_texture);
        bezel_texture = NULL;
        SDL_free(bezel_path);
        return false;
    }

    SDL_free(bezel_path);
    return true;
}

void SDLBezel_Quit(void) {
    if (bezel_texture != NULL) {
        SDL_DestroyTexture(bezel_texture);
        bezel_texture = NULL;
    }
}

bool SDLBezel_Render(SDL_Renderer* renderer, int target_width, int target_height) {
    if (renderer == NULL || bezel_texture == NULL || target_width <= 0 || target_height <= 0) {
        return false;
    }

    const SDL_FRect destination = {
        .x = 0,
        .y = 0,
        .w = (float)target_width,
        .h = (float)target_height,
    };
    return SDL_RenderTexture(renderer, bezel_texture, NULL, &destination);
}
