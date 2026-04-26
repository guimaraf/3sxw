#ifndef SDL_GAME_RENDERER_H
#define SDL_GAME_RENDERER_H

#include "structs.h"
#include <SDL3/SDL.h>
#include <stdbool.h>

typedef struct SDLGameRenderer_Vertex {
    struct {
        float x;
        float y;
        float z;
        float w;
    } coord;
    unsigned int color;
    TexCoord tex_coord;
} SDLGameRenderer_Vertex;

typedef struct Quad {
    Vec3 v[4];
} Quad;

typedef struct Sprite {
    Vec3 v[4];
    TexCoord t[4];
    unsigned int tex_code;
} Sprite;

typedef struct Sprite2 {
    Vec3 v[2];
    TexCoord t[2];
    unsigned int vertex_color;
    unsigned int tex_code;
    unsigned int id;
} Sprite2;

typedef struct SDLGameRendererStats {
    int render_tasks;
    int geometry_calls;
    int texture_cache_misses;
    int texture_cache_misses_first_use;
    int texture_cache_misses_after_palette_unlock;
    int texture_cache_misses_after_texture_unlock;
    int texture_cache_misses_after_release;
    int texture_cache_misses_unknown;
    int palette_unlocks;
    int texture_unlocks;
    int palette_cache_invalidated_textures;
    int texture_cache_invalidated_textures;
    int release_cache_invalidated_textures;
    double render_sort_ms;
    double render_geometry_ms;
} SDLGameRendererStats;

extern SDL_Texture* cps3_canvas;

void SDLGameRenderer_Init(SDL_Renderer* renderer);
void SDLGameRenderer_SetDebugIndexedTexturePathEnabled(bool enabled);
void SDLGameRenderer_BeginFrame();
void SDLGameRenderer_RenderFrame(SDLGameRendererStats* stats);
void SDLGameRenderer_EndFrame();

void SDLGameRenderer_CreateTexture(unsigned int th);
void SDLGameRenderer_DestroyTexture(unsigned int texture_handle);
void SDLGameRenderer_UnlockTexture(unsigned int th);
void SDLGameRenderer_CreatePalette(unsigned int ph);
void SDLGameRenderer_DestroyPalette(unsigned int palette_handle);
void SDLGameRenderer_UnlockPalette(unsigned int ph);
void SDLGameRenderer_SetTexture(unsigned int th);
void SDLGameRenderer_DrawTexturedQuad(const Sprite* sprite, unsigned int color);
void SDLGameRenderer_DrawSolidQuad(const Quad* vertices, unsigned int color);
void SDLGameRenderer_DrawSprite(const Sprite* sprite, unsigned int color);
void SDLGameRenderer_DrawSprite2(const Sprite2* sprite2);

#endif
