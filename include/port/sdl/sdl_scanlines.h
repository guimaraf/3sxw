#ifndef PORT_SDL_SDL_SCANLINES_H
#define PORT_SDL_SDL_SCANLINES_H

#include <SDL3/SDL.h>
#include <stdbool.h>

bool SDLScanlines_Init(SDL_Renderer* renderer, int opacity_percent);
void SDLScanlines_Quit(void);
bool SDLScanlines_Render(SDL_Renderer* renderer, const SDL_FRect* destination);

#endif
