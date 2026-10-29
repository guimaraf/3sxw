#include <SDL3/SDL.h>
#include <SDL3/SDL_test.h>

#define NUM_SHAPES 25

typedef struct
{
    SDL_FRect rect;
    float dx;
    float dy;
    Uint8 r, g, b;
} Shape;

static SDLTest_CommonState *state;
static Shape shapes[NUM_SHAPES];
static int done;
static Uint64 next_fps_check;
static Uint32 frames;
static const int fps_check_delay = 2000;

static void InitShapes(SDL_Renderer *renderer)
{
    SDL_Rect viewport;
    int i;

    SDL_GetRenderViewport(renderer, &viewport);
    if (viewport.w < 1) {
        viewport.w = 1;
    }
    if (viewport.h < 1) {
        viewport.h = 1;
    }

    for (i = 0; i < NUM_SHAPES; ++i) {
        shapes[i].rect.w = (float)(32 + SDL_rand(96));
        shapes[i].rect.h = (float)(32 + SDL_rand(96));
        shapes[i].rect.x = (float)SDL_rand(SDL_max(1, viewport.w - (int)shapes[i].rect.w));
        shapes[i].rect.y = (float)SDL_rand(SDL_max(1, viewport.h - (int)shapes[i].rect.h));
        shapes[i].dx = (SDL_rand(200) - 100) / 20.0f;
        shapes[i].dy = (SDL_rand(200) - 100) / 20.0f;
        if (shapes[i].dx == 0.0f) {
            shapes[i].dx = 1.0f;
        }
        if (shapes[i].dy == 0.0f) {
            shapes[i].dy = 1.0f;
        }
        shapes[i].r = (Uint8)(64 + SDL_rand(192));
        shapes[i].g = (Uint8)(64 + SDL_rand(192));
        shapes[i].b = (Uint8)(64 + SDL_rand(192));
    }
}

static void UpdateAndDrawShapes(SDL_Renderer *renderer)
{
    SDL_Rect viewport;
    int i;

    SDL_GetRenderViewport(renderer, &viewport);

    for (i = 0; i < NUM_SHAPES; ++i) {
        Shape *s = &shapes[i];

        s->rect.x += s->dx;
        s->rect.y += s->dy;

        if (s->rect.x < 0.0f) {
            s->rect.x = 0.0f;
            s->dx = SDL_fabsf(s->dx);
        } else if (s->rect.x + s->rect.w > (float)viewport.w) {
            s->rect.x = (float)viewport.w - s->rect.w;
            s->dx = -SDL_fabsf(s->dx);
        }

        if (s->rect.y < 0.0f) {
            s->rect.y = 0.0f;
            s->dy = SDL_fabsf(s->dy);
        } else if (s->rect.y + s->rect.h > (float)viewport.h) {
            s->rect.y = (float)viewport.h - s->rect.h;
            s->dy = -SDL_fabsf(s->dy);
        }

        SDL_SetRenderDrawColor(renderer, s->r, s->g, s->b, 180);
        SDL_RenderFillRect(renderer, &s->rect);
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_RenderRect(renderer, &s->rect);
    }
}

static void DrawOverlay(SDL_Renderer *renderer)
{
    SDL_Rect viewport;
    const char *name = SDL_GetRendererName(renderer);
    char line[128];

    SDL_GetRenderViewport(renderer, &viewport);

    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderLine(renderer, 0.0f, 0.0f, (float)(viewport.w - 1), (float)(viewport.h - 1));
    SDL_RenderLine(renderer, 0.0f, (float)(viewport.h - 1), (float)(viewport.w - 1), 0.0f);

    SDL_snprintf(line, sizeof(line), "renderer: %s", name ? name : "(unknown)");
    SDLTest_DrawString(renderer, 12.0f, 12.0f, line);
    SDLTest_DrawString(renderer, 12.0f, 28.0f, "WinRT testdraw");
}

static void loop(void)
{
    int i;
    SDL_Event event;
    Uint64 now;

    while (SDL_PollEvent(&event)) {
        SDLTest_CommonEvent(state, &event, &done);
    }

    for (i = 0; i < state->num_windows; ++i) {
        SDL_Renderer *renderer = state->renderers[i];
        if (!state->windows[i] || !renderer) {
            continue;
        }

        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, 32, 36, 48, 255);
        SDL_RenderClear(renderer);

        UpdateAndDrawShapes(renderer);
        DrawOverlay(renderer);

        SDL_RenderPresent(renderer);
    }

    frames++;
    now = SDL_GetTicks();
    if (now >= next_fps_check) {
        const Uint64 then = next_fps_check - fps_check_delay;
        const double fps = ((double)frames * 1000.0) / (double)(now - then);
        SDL_Log("%.1f frames per second", fps);
        next_fps_check = now + fps_check_delay;
        frames = 0;
    }
}

int testdraw_main(int argc, char *argv[])
{
    int i;

    SDL_SetHint(SDL_HINT_RENDER_DRIVER, "direct3d12");

    state = SDLTest_CommonCreateState(argv, SDL_INIT_VIDEO);
    if (!state) {
        return 1;
    }

    for (i = 1; i < argc;) {
        int consumed = SDLTest_CommonArg(state, i);
        if (consumed == 0) {
            consumed = -1;
        }
        if (consumed < 0) {
            SDLTest_CommonLogUsage(state, argv[0], NULL);
            SDLTest_CommonQuit(state);
            return 1;
        }
        i += consumed;
    }

    if (!SDLTest_CommonInit(state)) {
        SDLTest_CommonQuit(state);
        return 2;
    }

    for (i = 0; i < state->num_windows; ++i) {
        SDL_Renderer *renderer = state->renderers[i];
        if (!renderer) {
            SDL_Log("No renderer for window %d", i);
            continue;
        }
        SDL_Log("Using renderer: %s", SDL_GetRendererName(renderer));
        InitShapes(renderer);
    }

    frames = 0;
    next_fps_check = SDL_GetTicks() + fps_check_delay;
    done = 0;

    while (!done) {
        loop();
    }

    SDLTest_CleanupTextDrawing();
    SDLTest_CommonQuit(state);
    return 0;
}
