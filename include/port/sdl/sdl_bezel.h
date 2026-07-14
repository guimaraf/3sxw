#ifndef PORT_SDL_SDL_BEZEL_H
#define PORT_SDL_SDL_BEZEL_H

#include <SDL3/SDL.h>
#include <stdbool.h>

bool SDLBezel_Init(SDL_Renderer* renderer);
void SDLBezel_Quit(void);
bool SDLBezel_Render(SDL_Renderer* renderer, int target_width, int target_height);

#endif
