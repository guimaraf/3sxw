#ifndef SF3CONFIG_UI_H
#define SF3CONFIG_UI_H

#include "config_file.h"

#include <SDL3/SDL.h>

#define CONFIG_UI_WIDTH 780
#define CONFIG_UI_HEIGHT 840

bool ConfigUI_Run(SDL_Window* window, SDL_Renderer* renderer, const char* config_path, ConfigSettings* settings);

#endif
