#ifndef SDL_APP_H
#define SDL_APP_H

#include <SDL3/SDL.h>

#include "port/sdl/sdl_game_renderer.h"
#if defined(SF3_UWP)
#include "port/debug/debug_log.h"
/* elapsed_ns=0 marks an event; timed calls below 1 ms are omitted. */
void SDLApp_RecordUWPCombatDetail(const char* kind, int subject, int detail, int value, Uint64 elapsed_ns);
enum { UWP_ATTACK_WRAPPER, UWP_ATTACK_SELECTED, UWP_ATTACK_SHARED, UWP_ATTACK_LANDING,
       UWP_ATTACK_ANIMATION, UWP_ATTACK_MARKS };
void SDLApp_UWPAttackTraceBegin(int character, int subject);
void SDLApp_UWPAttackTraceMark(int mark);
void SDLApp_UWPAttackTraceEnd(void);
void SDLApp_RecordUWPGamePhase(const char* name, Uint64 elapsed_ns);
void SDLApp_RecordUWPGameTiming(const DebugFrameTiming* frame, const DebugStepStats* steps,
                               const DebugTaskStats* tasks);
#endif

typedef struct SDLAppFrameTiming {
    double sleep_ms;
    double adx_process_ms;
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

bool SDLApp_PreInit();
bool SDLApp_FullInit();
void SDLApp_WriteDebugSessionInfo();
void SDLApp_Quit();

/// @brief Poll SDL events.
/// @return `true` if the main loop should continue running, `false` otherwise.
bool SDLApp_PollEvents();
bool SDLApp_ConsumePauseRequest();
bool SDLApp_HasInputFocus();
double SDLApp_GetTargetFrameRate();
const char* SDLApp_GetFrameTimingName();

void SDLApp_BeginFrame();
void SDLApp_EndFrame(SDLAppFrameTiming* timing);
void SDLApp_Exit();

#endif
