#ifndef PORT_DEBUG_DEBUG_LOG_H
#define PORT_DEBUG_DEBUG_LOG_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#define DEBUG_TASK_STATS_COUNT 11

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
    int indexed_texture_updates;
    int indexed_texture_update_pixels;
    double indexed_texture_update_ms;
    int indexed_palette_updates;
    double indexed_palette_update_ms;
    int indexed_texture_rgba_fallbacks;
    double render_sort_ms;
    double render_geometry_ms;
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
} DebugRenderStats;

typedef struct DebugStepStats {
    uint64_t frame;
    double afs_run_server_ms;
    double setup_temp_priority_ms;
    double pad_get_all_ms;
    double key_convert_ms;
    double test_prologue_ms;
    double input_copy_ms;
    double game_main_ms;
    double nj_user_main_ms;
    double seqs_before_process_ms;
    double njdp2d_draw_ms;
    double seqs_after_process_ms;
    double netplay_tick_ms;
    double knj_flush_ms;
    double disp_effect_work_ms;
    double fl_flip_ms;
    double scrn_renew_ms;
    double irl_family_ms;
    double irl_scrn_ms;
    double bgm_server_ms;
} DebugStepStats;

typedef struct DebugTaskStats {
    uint64_t frame;
    int condition[DEBUG_TASK_STATS_COUNT];
    int r_no[DEBUG_TASK_STATS_COUNT][4];
    double task_ms[DEBUG_TASK_STATS_COUNT];
} DebugTaskStats;

void DebugLog_Init(int enabled, int light_profile_enabled, int argc, const char* command_line);
void DebugLog_Shutdown();

bool DebugLog_IsEnabled();
const char* DebugLog_GetSessionPath();
void DebugLog_BeginFrame(uint64_t frame);
void DebugLog_Write(const char* file_name, const char* text);
void DebugLog_Printf(const char* file_name, const char* format, ...);
void DebugLog_PrintSession(const char* format, ...);
void DebugLog_RecordFrameTiming(const DebugFrameTiming* timing);
void DebugLog_RecordRenderStats(const DebugRenderStats* stats);
void DebugLog_RecordStepStats(const DebugStepStats* stats);
void DebugLog_RecordTaskStats(const DebugTaskStats* stats);
void DebugLog_RecordAdxProcess(double elapsed_ms, int queued_bytes, int tracks);
void DebugLog_RecordAdxStartMem(size_t bytes, double elapsed_ms);
void DebugLog_RecordAdxEntryAfs(int file_id, double elapsed_ms);
void DebugLog_RecordAdxStartAfs(int file_id, double elapsed_ms);
void DebugLog_RecordAdxLoadFile(int file_id, uint32_t bytes, double elapsed_ms);
void DebugLog_RecordAudioAfsSyncRead(int file_id, int sectors, uint32_t bytes, double elapsed_ms);
void DebugLog_RecordAfsSyncRead(int file_id, int sectors, uint32_t bytes, double elapsed_ms);
void DebugLog_RecordSpuUpload(uint32_t bytes, double elapsed_ms);
void DebugLog_RecordCseExecServer(double elapsed_ms);
void DebugLog_RecordCseTsbRequest(double elapsed_ms);
void DebugLog_RecordCseSendBdToSpu(uint32_t bytes, double elapsed_ms);

#endif
