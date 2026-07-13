#ifndef PORT_SDL_SDL_SCREENSHOT_H
#define PORT_SDL_SDL_SCREENSHOT_H

#include <SDL3/SDL.h>
#include <stdbool.h>

bool SDLScreenshot_Init(void);
void SDLScreenshot_Quit(void);

// On success, the screenshot worker takes ownership of the surface.
bool SDLScreenshot_Queue(SDL_Surface* surface);

#endif
