#ifndef SF3CONFIG_FONT_H
#define SF3CONFIG_FONT_H

#include <SDL3/SDL.h>
#include <stdbool.h>

bool AppFont_Init(SDL_Renderer* renderer);
void AppFont_Quit(void);
float AppFont_MeasureText(const char* text, float scale);
float AppFont_LineHeight(float scale);
bool AppFont_Draw(SDL_Renderer* renderer,
                  float x,
                  float y,
                  const char* text,
                  SDL_Color color,
                  float scale);

#endif
