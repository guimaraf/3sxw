#include "port/sdl/sdl_scanlines.h"
#include "port/utils.h"

#include <SDL3/SDL.h>

#define GAME_LOGICAL_HEIGHT 224
#define SCANLINE_PATTERN_HEIGHT (GAME_LOGICAL_HEIGHT * 2)

static SDL_Texture* scanlines_texture = NULL;

static int clamp_opacity(int opacity_percent) {
    if (opacity_percent < 0) {
        log_error("Scanline opacity must be between 0 and 100; received %d. Using 0.", opacity_percent);
        return 0;
    }

    if (opacity_percent > 100) {
        log_error("Scanline opacity must be between 0 and 100; received %d. Using 100.", opacity_percent);
        return 100;
    }

    return opacity_percent;
}

static SDL_Surface* create_scanline_surface(void) {
    SDL_Surface* surface = SDL_CreateSurface(1, SCANLINE_PATTERN_HEIGHT, SDL_PIXELFORMAT_RGBA32);

    if (surface == NULL) {
        return NULL;
    }

    if (!SDL_LockSurface(surface)) {
        SDL_DestroySurface(surface);
        return NULL;
    }

    for (int y = 0; y < surface->h; y++) {
        Uint8* pixel = (Uint8*)surface->pixels + ((size_t)y * (size_t)surface->pitch);
        pixel[0] = 0;
        pixel[1] = 0;
        pixel[2] = 0;
        pixel[3] = (y & 1) ? SDL_ALPHA_OPAQUE : SDL_ALPHA_TRANSPARENT;
    }

    SDL_UnlockSurface(surface);
    return surface;
}

bool SDLScanlines_Init(SDL_Renderer* renderer, int opacity_percent) {
    if (scanlines_texture != NULL) {
        return true;
    }

    if (renderer == NULL) {
        log_error("Couldn't initialize scanlines without an SDL renderer.");
        return false;
    }

    SDL_Surface* surface = create_scanline_surface();

    if (surface == NULL) {
        log_error("Couldn't create the scanline pattern: %s", SDL_GetError());
        return false;
    }

    scanlines_texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_DestroySurface(surface);

    if (scanlines_texture == NULL) {
        log_error("Couldn't create the scanline texture: %s", SDL_GetError());
        return false;
    }

    const int clamped_opacity = clamp_opacity(opacity_percent);
    const Uint8 alpha = (Uint8)((clamped_opacity * SDL_ALPHA_OPAQUE) / 100);

    if (!SDL_SetTextureBlendMode(scanlines_texture, SDL_BLENDMODE_BLEND) ||
        !SDL_SetTextureScaleMode(scanlines_texture, SDL_SCALEMODE_NEAREST) ||
        !SDL_SetTextureAlphaMod(scanlines_texture, alpha)) {
        log_error("Couldn't configure the scanline texture: %s", SDL_GetError());
        SDL_DestroyTexture(scanlines_texture);
        scanlines_texture = NULL;
        return false;
    }

    return true;
}

void SDLScanlines_Quit(void) {
    if (scanlines_texture != NULL) {
        SDL_DestroyTexture(scanlines_texture);
        scanlines_texture = NULL;
    }
}

bool SDLScanlines_Render(SDL_Renderer* renderer, const SDL_FRect* destination) {
    if (renderer == NULL || scanlines_texture == NULL || destination == NULL || destination->w <= 0.0f ||
        destination->h <= 0.0f) {
        return false;
    }

    return SDL_RenderTexture(renderer, scanlines_texture, NULL, destination);
}
