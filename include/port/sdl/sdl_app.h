#ifndef SDL_APP_H
#define SDL_APP_H

#include <SDL3/SDL.h>

#define TARGET_FPS 59.59949

#include "port/sdl/sdl_game_renderer.h"

typedef struct SDLAppFrameTiming {
    double sleep_ms;
    double adx_process_ms;
    double netplay_screen_render_ms;
    double netstats_render_ms;
    double game_renderer_render_ms;
    double screenshot_ms;
    double screen_copy_ms;
    double debug_text_ms;
    double present_ms;
    double cleanup_ms;
    double cursor_ms;
    double pacing_ms;
    double pacing_overhead_ms;
    double sleep_overrun_ms;
    SDLGameRendererStats render_stats;
} SDLAppFrameTiming;

int SDLApp_PreInit();
int SDLApp_FullInit();
void SDLApp_WriteDebugSessionInfo();
void SDLApp_Quit();

/// @brief Poll SDL events.
/// @return `true` if the main loop should continue running, `false` otherwise.
bool SDLApp_PollEvents();

void SDLApp_BeginFrame();
void SDLApp_EndFrame(SDLAppFrameTiming* timing);
void SDLApp_Exit();

#endif
