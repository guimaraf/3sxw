#ifndef PORT_DEBUG_DEBUG_LOG_H
#define PORT_DEBUG_DEBUG_LOG_H

#include <stdbool.h>
#include <stdint.h>

typedef struct DebugFrameTiming {
    uint64_t frame;
    double total_ms;
    double poll_ms;
    double begin_ms;
    double game0_ms;
    double end_ms;
    double game1_ms;
    double sleep_ms;
    int late_flag;
} DebugFrameTiming;

typedef struct DebugRenderStats {
    uint64_t frame;
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
} DebugRenderStats;

void DebugLog_Init(int enabled, int argc, const char* command_line);
void DebugLog_Shutdown();

bool DebugLog_IsEnabled();
const char* DebugLog_GetSessionPath();
void DebugLog_Write(const char* file_name, const char* text);
void DebugLog_Printf(const char* file_name, const char* format, ...);
void DebugLog_PrintSession(const char* format, ...);
void DebugLog_RecordFrameTiming(const DebugFrameTiming* timing);
void DebugLog_RecordRenderStats(const DebugRenderStats* stats);

#endif
