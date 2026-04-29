#include "port/debug/debug_log.h"
#include "port/paths.h"

#include <SDL3/SDL.h>

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static bool debug_log_enabled = false;
static bool debug_log_initialized = false;
static bool debug_log_light_profile = false;
static char* debug_log_session_path = NULL;
static FILE* frame_timing_file = NULL;
static FILE* render_stats_file = NULL;
static FILE* step_stats_file = NULL;
static FILE* task_stats_file = NULL;
static FILE* audio_stats_file = NULL;
static FILE* event_log_file = NULL;
static Uint64 session_start_ns = 0;
static Uint64 current_frame = 0;
static double current_adx_process_ms = 0.0;
static int current_adx_queued_bytes = 0;
static int current_adx_tracks = 0;
static int current_adx_start_mem_count = 0;
static Uint64 current_adx_start_mem_bytes = 0;
static double current_adx_start_mem_ms = 0.0;
static int current_adx_entry_afs_count = 0;
static int current_adx_start_afs_count = 0;
static int current_adx_last_file_id = -1;
static double current_adx_start_file_ms = 0.0;
static int current_adx_load_file_count = 0;
static Uint64 current_adx_load_file_bytes = 0;
static double current_adx_load_file_ms = 0.0;
static int current_audio_afs_sync_read_count = 0;
static int current_audio_afs_sync_read_sectors = 0;
static Uint64 current_audio_afs_sync_read_bytes = 0;
static double current_audio_afs_sync_read_ms = 0.0;
static int current_audio_event = 0;
static int current_afs_sync_read_count = 0;
static int current_afs_sync_read_sectors = 0;
static Uint64 current_afs_sync_read_bytes = 0;
static double current_afs_sync_read_ms = 0.0;
static int current_spu_upload_count = 0;
static Uint64 current_spu_upload_bytes = 0;
static double current_spu_upload_ms = 0.0;
static int current_cse_exec_server_count = 0;
static double current_cse_exec_server_ms = 0.0;
static int current_cse_tsb_request_count = 0;
static double current_cse_tsb_request_ms = 0.0;
static int current_cse_send_bd_to_spu_count = 0;
static Uint64 current_cse_send_bd_to_spu_bytes = 0;
static double current_cse_send_bd_to_spu_ms = 0.0;
static double* frame_total_ms_samples = NULL;
static size_t frame_total_ms_count = 0;
static size_t frame_total_ms_capacity = 0;
static double frame_total_ms_sum = 0.0;
static double worst_frame_ms = 0.0;
static Uint64 worst_frame = 0;
static Uint64 late_frame_count = 0;
static double worst_poll_ms = 0.0;
static double worst_begin_ms = 0.0;
static double worst_game0_ms = 0.0;
static double worst_end_ms = 0.0;
static double worst_game1_ms = 0.0;
static Uint64 worst_poll_frame = 0;
static Uint64 worst_begin_frame = 0;
static Uint64 worst_game0_frame = 0;
static Uint64 worst_end_frame = 0;
static Uint64 worst_game1_frame = 0;
static double worst_task_ms = 0.0;
static Uint64 worst_task_frame = 0;
static int worst_task_index = 0;
static int max_render_tasks = 0;
static int max_geometry_calls = 0;
static Uint64 total_texture_cache_misses = 0;
static Uint64 total_texture_cache_misses_first_use = 0;
static Uint64 total_texture_cache_misses_after_palette_unlock = 0;
static Uint64 total_texture_cache_misses_after_texture_unlock = 0;
static Uint64 total_texture_cache_misses_after_release = 0;
static Uint64 total_texture_cache_misses_unknown = 0;
static Uint64 total_palette_unlocks = 0;
static Uint64 total_texture_unlocks = 0;
static Uint64 total_palette_cache_invalidated_textures = 0;
static Uint64 total_texture_cache_invalidated_textures = 0;
static Uint64 total_release_cache_invalidated_textures = 0;
static Uint64 total_indexed_texture_updates = 0;
static Uint64 total_indexed_texture_update_pixels = 0;
static double total_indexed_texture_update_ms = 0.0;
static double worst_indexed_texture_update_ms = 0.0;
static Uint64 worst_indexed_texture_update_frame = 0;
static Uint64 total_indexed_palette_updates = 0;
static double total_indexed_palette_update_ms = 0.0;
static double worst_indexed_palette_update_ms = 0.0;
static Uint64 worst_indexed_palette_update_frame = 0;
static Uint64 total_indexed_texture_rgba_fallbacks = 0;
static double worst_render_sort_ms = 0.0;
static double worst_render_geometry_ms = 0.0;
static Uint64 worst_render_sort_frame = 0;
static Uint64 worst_render_geometry_frame = 0;
static double worst_adx_process_ms = 0.0;
static double worst_afs_sync_read_ms = 0.0;
static double worst_audio_afs_sync_read_ms = 0.0;
static double worst_adx_start_mem_ms = 0.0;
static double worst_adx_entry_afs_ms = 0.0;
static double worst_adx_start_afs_ms = 0.0;
static double worst_adx_load_file_ms = 0.0;
static double worst_netplay_screen_render_ms = 0.0;
static double worst_netstats_render_ms = 0.0;
static double worst_game_renderer_render_ms = 0.0;
static double worst_screenshot_ms = 0.0;
static double worst_screen_copy_ms = 0.0;
static double worst_debug_text_ms = 0.0;
static double worst_present_ms = 0.0;
static double worst_cleanup_ms = 0.0;
static double worst_cursor_ms = 0.0;
static double worst_pacing_ms = 0.0;
static double worst_pacing_overhead_ms = 0.0;
static double worst_sleep_overrun_ms = 0.0;
static Uint64 worst_adx_process_frame = 0;
static Uint64 worst_afs_sync_read_frame = 0;
static Uint64 worst_audio_afs_sync_read_frame = 0;
static Uint64 worst_adx_start_mem_frame = 0;
static Uint64 worst_adx_entry_afs_frame = 0;
static Uint64 worst_adx_start_afs_frame = 0;
static Uint64 worst_adx_load_file_frame = 0;
static Uint64 worst_netplay_screen_render_frame = 0;
static Uint64 worst_netstats_render_frame = 0;
static Uint64 worst_game_renderer_render_frame = 0;
static Uint64 worst_screenshot_frame = 0;
static Uint64 worst_screen_copy_frame = 0;
static Uint64 worst_debug_text_frame = 0;
static Uint64 worst_present_frame = 0;
static Uint64 worst_cleanup_frame = 0;
static Uint64 worst_cursor_frame = 0;
static Uint64 worst_pacing_frame = 0;
static Uint64 worst_pacing_overhead_frame = 0;
static Uint64 worst_sleep_overrun_frame = 0;
static Uint64 total_afs_sync_reads = 0;
static Uint64 total_afs_sync_read_bytes = 0;
static double total_afs_sync_read_ms = 0.0;
static Uint64 total_audio_afs_sync_reads = 0;
static Uint64 total_audio_afs_sync_read_bytes = 0;
static double total_audio_afs_sync_read_ms = 0.0;
static Uint64 total_adx_start_mem_count = 0;
static Uint64 total_adx_start_mem_bytes = 0;
static double total_adx_start_mem_ms = 0.0;
static Uint64 total_adx_entry_afs_count = 0;
static double total_adx_entry_afs_ms = 0.0;
static Uint64 total_adx_start_afs_count = 0;
static double total_adx_start_afs_ms = 0.0;
static Uint64 total_adx_load_file_count = 0;
static Uint64 total_adx_load_file_bytes = 0;
static double total_adx_load_file_ms = 0.0;
static int max_adx_queued_bytes = 0;
static int max_adx_tracks = 0;

enum {
    DEBUG_AUDIO_EVENT_ADX_START_MEM = 1 << 0,
    DEBUG_AUDIO_EVENT_ADX_ENTRY_AFS = 1 << 1,
    DEBUG_AUDIO_EVENT_ADX_START_AFS = 1 << 2,
    DEBUG_AUDIO_EVENT_ADX_LOAD_FILE = 1 << 3,
    DEBUG_AUDIO_EVENT_AUDIO_AFS_SYNC_READ = 1 << 4,
};

static bool get_local_time(struct tm* local_time) {
    const time_t now = time(NULL);

    if (now == (time_t)-1) {
        return false;
    }

#if _WIN32
    return localtime_s(local_time, &now) == 0;
#else
    return localtime_r(&now, local_time) != NULL;
#endif
}

static bool ensure_directory(const char* path) {
    SDL_PathInfo path_info;

    if (SDL_GetPathInfo(path, &path_info)) {
        return path_info.type == SDL_PATHTYPE_DIRECTORY;
    }

    return SDL_CreateDirectory(path);
}

static void write_file_v(const char* file_name, const char* mode, const char* format, va_list args) {
    if (!debug_log_enabled || debug_log_session_path == NULL) {
        return;
    }

    char* file_path = NULL;
    SDL_asprintf(&file_path, "%s%s", debug_log_session_path, file_name);

    FILE* file = fopen(file_path, mode);

    if (file == NULL) {
        SDL_Log("Failed to open debug log file: %s", file_path);
        SDL_free(file_path);
        return;
    }

    vfprintf(file, format, args);
    fclose(file);
    SDL_free(file_path);
}

static void write_file(const char* file_name, const char* mode, const char* format, ...) {
    va_list args;
    va_start(args, format);
    write_file_v(file_name, mode, format, args);
    va_end(args);
}

static bool verbose_events_enabled() {
    return !debug_log_light_profile;
}

static FILE* open_session_file(const char* file_name, const char* mode) {
    if (!debug_log_enabled || debug_log_session_path == NULL) {
        return NULL;
    }

    char* file_path = NULL;
    SDL_asprintf(&file_path, "%s%s", debug_log_session_path, file_name);

    FILE* file = fopen(file_path, mode);

    if (file == NULL) {
        SDL_Log("Failed to open debug log file: %s", file_path);
    }

    SDL_free(file_path);
    return file;
}

static int compare_double(const void* a, const void* b) {
    const double left = *(const double*)a;
    const double right = *(const double*)b;

    if (left < right) {
        return -1;
    }

    if (left > right) {
        return 1;
    }

    return 0;
}

static double percentile_ms(double percentile) {
    if (frame_total_ms_count == 0) {
        return 0.0;
    }

    qsort(frame_total_ms_samples, frame_total_ms_count, sizeof(double), compare_double);

    size_t index = (size_t)((percentile * (double)frame_total_ms_count) + 0.999999) - 1;

    if (index >= frame_total_ms_count) {
        index = frame_total_ms_count - 1;
    }

    return frame_total_ms_samples[index];
}

static void write_summary_file() {
    if (!debug_log_enabled) {
        return;
    }

    FILE* file = open_session_file("summary.txt", "w");

    if (file == NULL) {
        return;
    }

    const Uint64 now_ns = SDL_GetTicksNS();
    const double session_seconds = session_start_ns == 0 ? 0.0 : (double)(now_ns - session_start_ns) / 1e9;
    const double avg_frame_ms = frame_total_ms_count == 0 ? 0.0 : frame_total_ms_sum / (double)frame_total_ms_count;
    const double p95_frame_ms = percentile_ms(0.95);
    const double p99_frame_ms = percentile_ms(0.99);

    fprintf(file, "session_seconds=%.3f\n", session_seconds);
    fprintf(file, "frames=%llu\n", (unsigned long long)frame_total_ms_count);
    fprintf(file, "avg_frame_ms=%.3f\n", avg_frame_ms);
    fprintf(file, "p95_frame_ms=%.3f\n", p95_frame_ms);
    fprintf(file, "p99_frame_ms=%.3f\n", p99_frame_ms);
    fprintf(file, "worst_frame_ms=%.3f\n", worst_frame_ms);
    fprintf(file, "worst_frame=%llu\n", (unsigned long long)worst_frame);
    fprintf(file, "late_frames=%llu\n", (unsigned long long)late_frame_count);
    fprintf(file, "worst_poll_ms=%.3f\n", worst_poll_ms);
    fprintf(file, "worst_poll_frame=%llu\n", (unsigned long long)worst_poll_frame);
    fprintf(file, "worst_begin_ms=%.3f\n", worst_begin_ms);
    fprintf(file, "worst_begin_frame=%llu\n", (unsigned long long)worst_begin_frame);
    fprintf(file, "worst_game0_ms=%.3f\n", worst_game0_ms);
    fprintf(file, "worst_game0_frame=%llu\n", (unsigned long long)worst_game0_frame);
    fprintf(file, "worst_end_ms=%.3f\n", worst_end_ms);
    fprintf(file, "worst_end_frame=%llu\n", (unsigned long long)worst_end_frame);
    fprintf(file, "worst_game1_ms=%.3f\n", worst_game1_ms);
    fprintf(file, "worst_game1_frame=%llu\n", (unsigned long long)worst_game1_frame);
    fprintf(file, "worst_task_ms=%.3f\n", worst_task_ms);
    fprintf(file, "worst_task_frame=%llu\n", (unsigned long long)worst_task_frame);
    fprintf(file, "worst_task_index=%d\n", worst_task_index);
    fprintf(file, "max_render_tasks=%d\n", max_render_tasks);
    fprintf(file, "max_geometry_calls=%d\n", max_geometry_calls);
    fprintf(file, "total_texture_cache_misses=%llu\n", (unsigned long long)total_texture_cache_misses);
    fprintf(file,
            "total_texture_cache_misses_first_use=%llu\n",
            (unsigned long long)total_texture_cache_misses_first_use);
    fprintf(file,
            "total_texture_cache_misses_after_palette_unlock=%llu\n",
            (unsigned long long)total_texture_cache_misses_after_palette_unlock);
    fprintf(file,
            "total_texture_cache_misses_after_texture_unlock=%llu\n",
            (unsigned long long)total_texture_cache_misses_after_texture_unlock);
    fprintf(file,
            "total_texture_cache_misses_after_release=%llu\n",
            (unsigned long long)total_texture_cache_misses_after_release);
    fprintf(file,
            "total_texture_cache_misses_unknown=%llu\n",
            (unsigned long long)total_texture_cache_misses_unknown);
    fprintf(file, "total_palette_unlocks=%llu\n", (unsigned long long)total_palette_unlocks);
    fprintf(file, "total_texture_unlocks=%llu\n", (unsigned long long)total_texture_unlocks);
    fprintf(file,
            "total_palette_cache_invalidated_textures=%llu\n",
            (unsigned long long)total_palette_cache_invalidated_textures);
    fprintf(file,
            "total_texture_cache_invalidated_textures=%llu\n",
            (unsigned long long)total_texture_cache_invalidated_textures);
    fprintf(file,
            "total_release_cache_invalidated_textures=%llu\n",
            (unsigned long long)total_release_cache_invalidated_textures);
    fprintf(file, "total_indexed_texture_updates=%llu\n", (unsigned long long)total_indexed_texture_updates);
    fprintf(file,
            "total_indexed_texture_update_pixels=%llu\n",
            (unsigned long long)total_indexed_texture_update_pixels);
    fprintf(file, "total_indexed_texture_update_ms=%.3f\n", total_indexed_texture_update_ms);
    fprintf(file, "worst_indexed_texture_update_ms=%.3f\n", worst_indexed_texture_update_ms);
    fprintf(file,
            "worst_indexed_texture_update_frame=%llu\n",
            (unsigned long long)worst_indexed_texture_update_frame);
    fprintf(file, "total_indexed_palette_updates=%llu\n", (unsigned long long)total_indexed_palette_updates);
    fprintf(file, "total_indexed_palette_update_ms=%.3f\n", total_indexed_palette_update_ms);
    fprintf(file, "worst_indexed_palette_update_ms=%.3f\n", worst_indexed_palette_update_ms);
    fprintf(file,
            "worst_indexed_palette_update_frame=%llu\n",
            (unsigned long long)worst_indexed_palette_update_frame);
    fprintf(file, "total_indexed_texture_rgba_fallbacks=%llu\n", (unsigned long long)total_indexed_texture_rgba_fallbacks);
    fprintf(file, "worst_render_sort_ms=%.3f\n", worst_render_sort_ms);
    fprintf(file, "worst_render_sort_frame=%llu\n", (unsigned long long)worst_render_sort_frame);
    fprintf(file, "worst_render_geometry_ms=%.3f\n", worst_render_geometry_ms);
    fprintf(file, "worst_render_geometry_frame=%llu\n", (unsigned long long)worst_render_geometry_frame);
    fprintf(file, "worst_adx_process_ms=%.3f\n", worst_adx_process_ms);
    fprintf(file, "worst_adx_process_frame=%llu\n", (unsigned long long)worst_adx_process_frame);
    fprintf(file, "worst_afs_sync_read_ms=%.3f\n", worst_afs_sync_read_ms);
    fprintf(file, "worst_afs_sync_read_frame=%llu\n", (unsigned long long)worst_afs_sync_read_frame);
    fprintf(file, "total_afs_sync_reads=%llu\n", (unsigned long long)total_afs_sync_reads);
    fprintf(file, "total_afs_sync_read_bytes=%llu\n", (unsigned long long)total_afs_sync_read_bytes);
    fprintf(file, "total_afs_sync_read_ms=%.3f\n", total_afs_sync_read_ms);
    fprintf(file, "worst_audio_afs_sync_read_ms=%.3f\n", worst_audio_afs_sync_read_ms);
    fprintf(file, "worst_audio_afs_sync_read_frame=%llu\n", (unsigned long long)worst_audio_afs_sync_read_frame);
    fprintf(file, "total_audio_afs_sync_reads=%llu\n", (unsigned long long)total_audio_afs_sync_reads);
    fprintf(file, "total_audio_afs_sync_read_bytes=%llu\n", (unsigned long long)total_audio_afs_sync_read_bytes);
    fprintf(file, "total_audio_afs_sync_read_ms=%.3f\n", total_audio_afs_sync_read_ms);
    fprintf(file, "worst_adx_start_mem_ms=%.3f\n", worst_adx_start_mem_ms);
    fprintf(file, "worst_adx_start_mem_frame=%llu\n", (unsigned long long)worst_adx_start_mem_frame);
    fprintf(file, "total_adx_start_mem_count=%llu\n", (unsigned long long)total_adx_start_mem_count);
    fprintf(file, "total_adx_start_mem_bytes=%llu\n", (unsigned long long)total_adx_start_mem_bytes);
    fprintf(file, "total_adx_start_mem_ms=%.3f\n", total_adx_start_mem_ms);
    fprintf(file, "worst_adx_entry_afs_ms=%.3f\n", worst_adx_entry_afs_ms);
    fprintf(file, "worst_adx_entry_afs_frame=%llu\n", (unsigned long long)worst_adx_entry_afs_frame);
    fprintf(file, "total_adx_entry_afs_count=%llu\n", (unsigned long long)total_adx_entry_afs_count);
    fprintf(file, "total_adx_entry_afs_ms=%.3f\n", total_adx_entry_afs_ms);
    fprintf(file, "worst_adx_start_afs_ms=%.3f\n", worst_adx_start_afs_ms);
    fprintf(file, "worst_adx_start_afs_frame=%llu\n", (unsigned long long)worst_adx_start_afs_frame);
    fprintf(file, "total_adx_start_afs_count=%llu\n", (unsigned long long)total_adx_start_afs_count);
    fprintf(file, "total_adx_start_afs_ms=%.3f\n", total_adx_start_afs_ms);
    fprintf(file, "worst_adx_load_file_ms=%.3f\n", worst_adx_load_file_ms);
    fprintf(file, "worst_adx_load_file_frame=%llu\n", (unsigned long long)worst_adx_load_file_frame);
    fprintf(file, "total_adx_load_file_count=%llu\n", (unsigned long long)total_adx_load_file_count);
    fprintf(file, "total_adx_load_file_bytes=%llu\n", (unsigned long long)total_adx_load_file_bytes);
    fprintf(file, "total_adx_load_file_ms=%.3f\n", total_adx_load_file_ms);
    fprintf(file, "max_adx_queued_bytes=%d\n", max_adx_queued_bytes);
    fprintf(file, "max_adx_tracks=%d\n", max_adx_tracks);
    fprintf(file, "worst_netplay_screen_render_ms=%.3f\n", worst_netplay_screen_render_ms);
    fprintf(file,
            "worst_netplay_screen_render_frame=%llu\n",
            (unsigned long long)worst_netplay_screen_render_frame);
    fprintf(file, "worst_netstats_render_ms=%.3f\n", worst_netstats_render_ms);
    fprintf(file, "worst_netstats_render_frame=%llu\n", (unsigned long long)worst_netstats_render_frame);
    fprintf(file, "worst_game_renderer_render_ms=%.3f\n", worst_game_renderer_render_ms);
    fprintf(file,
            "worst_game_renderer_render_frame=%llu\n",
            (unsigned long long)worst_game_renderer_render_frame);
    fprintf(file, "worst_screenshot_ms=%.3f\n", worst_screenshot_ms);
    fprintf(file, "worst_screenshot_frame=%llu\n", (unsigned long long)worst_screenshot_frame);
    fprintf(file, "worst_screen_copy_ms=%.3f\n", worst_screen_copy_ms);
    fprintf(file, "worst_screen_copy_frame=%llu\n", (unsigned long long)worst_screen_copy_frame);
    fprintf(file, "worst_debug_text_ms=%.3f\n", worst_debug_text_ms);
    fprintf(file, "worst_debug_text_frame=%llu\n", (unsigned long long)worst_debug_text_frame);
    fprintf(file, "worst_present_ms=%.3f\n", worst_present_ms);
    fprintf(file, "worst_present_frame=%llu\n", (unsigned long long)worst_present_frame);
    fprintf(file, "worst_cleanup_ms=%.3f\n", worst_cleanup_ms);
    fprintf(file, "worst_cleanup_frame=%llu\n", (unsigned long long)worst_cleanup_frame);
    fprintf(file, "worst_cursor_ms=%.3f\n", worst_cursor_ms);
    fprintf(file, "worst_cursor_frame=%llu\n", (unsigned long long)worst_cursor_frame);
    fprintf(file, "worst_pacing_ms=%.3f\n", worst_pacing_ms);
    fprintf(file, "worst_pacing_frame=%llu\n", (unsigned long long)worst_pacing_frame);
    fprintf(file, "worst_pacing_overhead_ms=%.3f\n", worst_pacing_overhead_ms);
    fprintf(file, "worst_pacing_overhead_frame=%llu\n", (unsigned long long)worst_pacing_overhead_frame);
    fprintf(file, "worst_sleep_overrun_ms=%.3f\n", worst_sleep_overrun_ms);
    fprintf(file, "worst_sleep_overrun_frame=%llu\n", (unsigned long long)worst_sleep_overrun_frame);
    fprintf(file, "frame_timing_csv=%sframe_timing.csv\n", debug_log_session_path);
    if (debug_log_light_profile) {
        fprintf(file, "render_stats_csv=disabled_light_profile\n");
        fprintf(file, "step_stats_csv=disabled_light_profile\n");
        fprintf(file, "task_stats_csv=disabled_light_profile\n");
    } else {
        fprintf(file, "render_stats_csv=%srender_stats.csv\n", debug_log_session_path);
        fprintf(file, "step_stats_csv=%sstep_stats.csv\n", debug_log_session_path);
        fprintf(file, "task_stats_csv=%stask_stats.csv\n", debug_log_session_path);
    }
    fprintf(file, "event_log_csv=%sevent_log.csv\n", debug_log_session_path);
    fprintf(file, "audio_stats_csv=%saudio_stats.csv\n", debug_log_session_path);
    fclose(file);
}

static void reset_frame_timing_stats() {
    if (frame_timing_file != NULL) {
        fclose(frame_timing_file);
        frame_timing_file = NULL;
    }

    if (render_stats_file != NULL) {
        fclose(render_stats_file);
        render_stats_file = NULL;
    }

    if (step_stats_file != NULL) {
        fclose(step_stats_file);
        step_stats_file = NULL;
    }

    if (task_stats_file != NULL) {
        fclose(task_stats_file);
        task_stats_file = NULL;
    }

    if (audio_stats_file != NULL) {
        fclose(audio_stats_file);
        audio_stats_file = NULL;
    }

    if (event_log_file != NULL) {
        fclose(event_log_file);
        event_log_file = NULL;
    }

    SDL_free(frame_total_ms_samples);
    frame_total_ms_samples = NULL;
    frame_total_ms_count = 0;
    frame_total_ms_capacity = 0;
    frame_total_ms_sum = 0.0;
    worst_frame_ms = 0.0;
    worst_frame = 0;
    late_frame_count = 0;
    debug_log_light_profile = false;
    current_frame = 0;
    current_adx_process_ms = 0.0;
    current_adx_queued_bytes = 0;
    current_adx_tracks = 0;
    current_adx_start_mem_count = 0;
    current_adx_start_mem_bytes = 0;
    current_adx_start_mem_ms = 0.0;
    current_adx_entry_afs_count = 0;
    current_adx_start_afs_count = 0;
    current_adx_last_file_id = -1;
    current_adx_start_file_ms = 0.0;
    current_adx_load_file_count = 0;
    current_adx_load_file_bytes = 0;
    current_adx_load_file_ms = 0.0;
    current_audio_afs_sync_read_count = 0;
    current_audio_afs_sync_read_sectors = 0;
    current_audio_afs_sync_read_bytes = 0;
    current_audio_afs_sync_read_ms = 0.0;
    current_audio_event = 0;
    current_afs_sync_read_count = 0;
    current_afs_sync_read_sectors = 0;
    current_afs_sync_read_bytes = 0;
    current_afs_sync_read_ms = 0.0;
    current_spu_upload_count = 0;
    current_spu_upload_bytes = 0;
    current_spu_upload_ms = 0.0;
    current_cse_exec_server_count = 0;
    current_cse_exec_server_ms = 0.0;
    current_cse_tsb_request_count = 0;
    current_cse_tsb_request_ms = 0.0;
    current_cse_send_bd_to_spu_count = 0;
    current_cse_send_bd_to_spu_bytes = 0;
    current_cse_send_bd_to_spu_ms = 0.0;
    worst_poll_ms = 0.0;
    worst_begin_ms = 0.0;
    worst_game0_ms = 0.0;
    worst_end_ms = 0.0;
    worst_game1_ms = 0.0;
    worst_poll_frame = 0;
    worst_begin_frame = 0;
    worst_game0_frame = 0;
    worst_end_frame = 0;
    worst_game1_frame = 0;
    worst_task_ms = 0.0;
    worst_task_frame = 0;
    worst_task_index = 0;
    session_start_ns = 0;
    max_render_tasks = 0;
    max_geometry_calls = 0;
    total_texture_cache_misses = 0;
    total_texture_cache_misses_first_use = 0;
    total_texture_cache_misses_after_palette_unlock = 0;
    total_texture_cache_misses_after_texture_unlock = 0;
    total_texture_cache_misses_after_release = 0;
    total_texture_cache_misses_unknown = 0;
    total_palette_unlocks = 0;
    total_texture_unlocks = 0;
    total_palette_cache_invalidated_textures = 0;
    total_texture_cache_invalidated_textures = 0;
    total_release_cache_invalidated_textures = 0;
    total_indexed_texture_updates = 0;
    total_indexed_texture_update_pixels = 0;
    total_indexed_texture_update_ms = 0.0;
    worst_indexed_texture_update_ms = 0.0;
    worst_indexed_texture_update_frame = 0;
    total_indexed_palette_updates = 0;
    total_indexed_palette_update_ms = 0.0;
    worst_indexed_palette_update_ms = 0.0;
    worst_indexed_palette_update_frame = 0;
    total_indexed_texture_rgba_fallbacks = 0;
    worst_render_sort_ms = 0.0;
    worst_render_geometry_ms = 0.0;
    worst_render_sort_frame = 0;
    worst_render_geometry_frame = 0;
    worst_adx_process_ms = 0.0;
    worst_netplay_screen_render_ms = 0.0;
    worst_netstats_render_ms = 0.0;
    worst_game_renderer_render_ms = 0.0;
    worst_screenshot_ms = 0.0;
    worst_screen_copy_ms = 0.0;
    worst_debug_text_ms = 0.0;
    worst_present_ms = 0.0;
    worst_cleanup_ms = 0.0;
    worst_cursor_ms = 0.0;
    worst_pacing_ms = 0.0;
    worst_pacing_overhead_ms = 0.0;
    worst_sleep_overrun_ms = 0.0;
    worst_adx_process_frame = 0;
    worst_afs_sync_read_ms = 0.0;
    worst_audio_afs_sync_read_ms = 0.0;
    worst_adx_start_mem_ms = 0.0;
    worst_adx_entry_afs_ms = 0.0;
    worst_adx_start_afs_ms = 0.0;
    worst_adx_load_file_ms = 0.0;
    worst_afs_sync_read_frame = 0;
    worst_audio_afs_sync_read_frame = 0;
    worst_adx_start_mem_frame = 0;
    worst_adx_entry_afs_frame = 0;
    worst_adx_start_afs_frame = 0;
    worst_adx_load_file_frame = 0;
    worst_netplay_screen_render_frame = 0;
    worst_netstats_render_frame = 0;
    worst_game_renderer_render_frame = 0;
    worst_screenshot_frame = 0;
    worst_screen_copy_frame = 0;
    worst_debug_text_frame = 0;
    worst_present_frame = 0;
    worst_cleanup_frame = 0;
    worst_cursor_frame = 0;
    worst_pacing_frame = 0;
    worst_pacing_overhead_frame = 0;
    worst_sleep_overrun_frame = 0;
    total_afs_sync_reads = 0;
    total_afs_sync_read_bytes = 0;
    total_afs_sync_read_ms = 0.0;
    total_audio_afs_sync_reads = 0;
    total_audio_afs_sync_read_bytes = 0;
    total_audio_afs_sync_read_ms = 0.0;
    total_adx_start_mem_count = 0;
    total_adx_start_mem_bytes = 0;
    total_adx_start_mem_ms = 0.0;
    total_adx_entry_afs_count = 0;
    total_adx_entry_afs_ms = 0.0;
    total_adx_start_afs_count = 0;
    total_adx_start_afs_ms = 0.0;
    total_adx_load_file_count = 0;
    total_adx_load_file_bytes = 0;
    total_adx_load_file_ms = 0.0;
    max_adx_queued_bytes = 0;
    max_adx_tracks = 0;
}

static void open_frame_timing_file() {
    frame_timing_file = open_session_file("frame_timing.csv", "w");

    if (frame_timing_file == NULL) {
        return;
    }

    fprintf(frame_timing_file, "frame,total_ms,poll_ms,begin_ms,game0_ms,end_ms,game1_ms,sleep_ms,late_flag\n");
}

static void open_render_stats_file() {
    render_stats_file = open_session_file("render_stats.csv", "w");

    if (render_stats_file == NULL) {
        return;
    }

    fprintf(render_stats_file,
            "frame,render_tasks,geometry_calls,texture_cache_misses,texture_cache_misses_first_use,"
            "texture_cache_misses_after_palette_unlock,texture_cache_misses_after_texture_unlock,"
            "texture_cache_misses_after_release,texture_cache_misses_unknown,palette_unlocks,texture_unlocks,"
            "palette_cache_invalidated_textures,texture_cache_invalidated_textures,"
            "release_cache_invalidated_textures,indexed_texture_updates,indexed_texture_update_pixels,"
            "indexed_texture_update_ms,indexed_palette_updates,indexed_palette_update_ms,"
            "indexed_texture_rgba_fallbacks,render_sort_ms,render_geometry_ms,adx_process_ms,"
            "netplay_screen_render_ms,netstats_render_ms,game_renderer_render_ms,screenshot_ms,"
            "screen_copy_ms,debug_text_ms,present_ms,cleanup_ms,cursor_ms,pacing_ms,"
            "pacing_overhead_ms,sleep_overrun_ms\n");
}

static void open_step_stats_file() {
    step_stats_file = open_session_file("step_stats.csv", "w");

    if (step_stats_file == NULL) {
        return;
    }

    fprintf(step_stats_file,
            "frame,afs_run_server_ms,setup_temp_priority_ms,pad_get_all_ms,key_convert_ms,"
            "test_prologue_ms,input_copy_ms,game_main_ms,nj_user_main_ms,seqs_before_process_ms,"
            "njdp2d_draw_ms,seqs_after_process_ms,netplay_tick_ms,knj_flush_ms,disp_effect_work_ms,"
            "fl_flip_ms,scrn_renew_ms,irl_family_ms,irl_scrn_ms,bgm_server_ms,spu_upload_count,"
            "spu_upload_bytes,spu_upload_ms,cse_exec_server_count,cse_exec_server_ms,"
            "cse_tsb_request_count,cse_tsb_request_ms,cse_send_bd_to_spu_count,"
            "cse_send_bd_to_spu_bytes,cse_send_bd_to_spu_ms\n");
}

static void open_task_stats_file() {
    task_stats_file = open_session_file("task_stats.csv", "w");

    if (task_stats_file == NULL) {
        return;
    }

    fprintf(task_stats_file, "frame");

    for (int i = 0; i < DEBUG_TASK_STATS_COUNT; i++) {
        fprintf(task_stats_file,
                ",task%d_condition,task%d_ms,task%d_r0,task%d_r1,task%d_r2,task%d_r3",
                i,
                i,
                i,
                i,
                i,
                i);
    }

    fprintf(task_stats_file, "\n");
}

static void open_audio_stats_file() {
    audio_stats_file = open_session_file("audio_stats.csv", "w");

    if (audio_stats_file == NULL) {
        return;
    }

    fprintf(audio_stats_file,
            "frame,adx_process_ms,adx_queued_bytes,adx_tracks,afs_sync_read_ms,afs_sync_reads,"
            "afs_sync_read_sectors,afs_sync_read_bytes,audio_afs_sync_read_ms,audio_afs_sync_reads,"
            "audio_afs_sync_read_sectors,audio_afs_sync_read_bytes,adx_start_mem_count,adx_start_mem_bytes,"
            "adx_start_mem_ms,adx_entry_afs_count,adx_start_afs_count,adx_last_file_id,adx_start_file_ms,"
            "adx_load_file_count,adx_load_file_bytes,adx_load_file_ms,audio_event\n");
}

static void open_event_log_file() {
    event_log_file = open_session_file("event_log.csv", "w");

    if (event_log_file == NULL) {
        return;
    }

    fprintf(event_log_file, "frame,event,value\n");
}

static void write_event(Uint64 frame, const char* event, const char* value_format, ...) {
    if (event_log_file == NULL) {
        return;
    }

    fprintf(event_log_file, "%llu,%s,", (unsigned long long)frame, event);

    va_list args;
    va_start(args, value_format);
    vfprintf(event_log_file, value_format, args);
    va_end(args);

    fprintf(event_log_file, "\n");
}

static void update_worst_ms(double value, Uint64 frame, double* worst_value, Uint64* worst_frame) {
    if (value > *worst_value) {
        *worst_value = value;
        *worst_frame = frame;
    }
}

static void write_spike_event(Uint64 frame, const char* event, double value) {
    if (value >= 4.0) {
        write_event(frame, event, "%.3f", value);
    }
}

static void write_audio_stats(Uint64 frame) {
    if (audio_stats_file == NULL) {
        return;
    }

    fprintf(audio_stats_file,
            "%llu,%.3f,%d,%d,%.3f,%d,%d,%llu,%.3f,%d,%d,%llu,%d,%llu,%.3f,%d,%d,%d,%.3f,%d,%llu,%.3f,%d\n",
            (unsigned long long)frame,
            current_adx_process_ms,
            current_adx_queued_bytes,
            current_adx_tracks,
            current_afs_sync_read_ms,
            current_afs_sync_read_count,
            current_afs_sync_read_sectors,
            (unsigned long long)current_afs_sync_read_bytes,
            current_audio_afs_sync_read_ms,
            current_audio_afs_sync_read_count,
            current_audio_afs_sync_read_sectors,
            (unsigned long long)current_audio_afs_sync_read_bytes,
            current_adx_start_mem_count,
            (unsigned long long)current_adx_start_mem_bytes,
            current_adx_start_mem_ms,
            current_adx_entry_afs_count,
            current_adx_start_afs_count,
            current_adx_last_file_id,
            current_adx_start_file_ms,
            current_adx_load_file_count,
            (unsigned long long)current_adx_load_file_bytes,
            current_adx_load_file_ms,
            current_audio_event);

    if ((frame % 300) == 0) {
        fflush(audio_stats_file);
    }
}

static void store_frame_total_sample(double total_ms) {
    if (frame_total_ms_count == frame_total_ms_capacity) {
        const size_t new_capacity = frame_total_ms_capacity == 0 ? 4096 : frame_total_ms_capacity * 2;
        double* new_samples = SDL_realloc(frame_total_ms_samples, new_capacity * sizeof(double));

        if (new_samples == NULL) {
            return;
        }

        frame_total_ms_samples = new_samples;
        frame_total_ms_capacity = new_capacity;
    }

    frame_total_ms_samples[frame_total_ms_count] = total_ms;
    frame_total_ms_count += 1;
}

static void format_timestamp(const struct tm* local_time, char* timestamp, size_t timestamp_size) {
    if (strftime(timestamp, timestamp_size, "%Y-%m-%d %H:%M:%S", local_time) == 0) {
        SDL_snprintf(timestamp, timestamp_size, "unknown-time");
    }
}

static void write_session_file(const char* started_at, int argc, const char* command_line) {
    write_file("session.txt", "w", "debug_mode=1\n");
    write_file("session.txt", "a", "started_at=%s\n", started_at);
    write_file("session.txt", "a", "build_date=%s\n", __DATE__);
    write_file("session.txt", "a", "build_time=%s\n", __TIME__);
    write_file("session.txt", "a", "base_path=%s\n", Paths_GetBasePath());
    write_file("session.txt", "a", "data_path=%s\n", Paths_GetDataPath());
    write_file("session.txt", "a", "session_path=%s\n", debug_log_session_path);
    write_file("session.txt", "a", "argc=%d\n", argc);
    write_file("session.txt", "a", "argv=%s\n", command_line != NULL ? command_line : "");
}

void DebugLog_Init(int enabled, int light_profile_enabled, int argc, const char* command_line) {
    if (debug_log_initialized) {
        return;
    }

    debug_log_initialized = true;
    debug_log_light_profile = light_profile_enabled != 0;

    if (!enabled) {
        return;
    }

    const char* data_path = Paths_GetDataPath();
    char* debug_path = NULL;
    SDL_asprintf(&debug_path, "%sdebug/", data_path);

    if (!ensure_directory(debug_path)) {
        SDL_Log("Failed to create debug directory: %s", debug_path);
        SDL_free(debug_path);
        return;
    }

    struct tm local_time = { 0 };
    char session_timestamp[32] = { 0 };
    char started_at[32] = { 0 };

    if (get_local_time(&local_time)) {
        strftime(session_timestamp, sizeof(session_timestamp), "%Y%m%d-%H%M%S", &local_time);
        format_timestamp(&local_time, started_at, sizeof(started_at));
    } else {
        SDL_snprintf(session_timestamp, sizeof(session_timestamp), "unknown-time");
        SDL_snprintf(started_at, sizeof(started_at), "unknown-time");
    }

    SDL_asprintf(&debug_log_session_path, "%ssession-%s/", debug_path, session_timestamp);
    SDL_free(debug_path);

    if (!ensure_directory(debug_log_session_path)) {
        SDL_Log("Failed to create debug session directory: %s", debug_log_session_path);
        SDL_free(debug_log_session_path);
        debug_log_session_path = NULL;
        return;
    }

    debug_log_enabled = true;
    session_start_ns = SDL_GetTicksNS();
    write_session_file(started_at, argc, command_line);
    open_frame_timing_file();
    if (!debug_log_light_profile) {
        open_render_stats_file();
        open_step_stats_file();
        open_task_stats_file();
    }
    open_audio_stats_file();
    open_event_log_file();
}

void DebugLog_Shutdown() {
    write_summary_file();

    if (frame_timing_file != NULL) {
        fclose(frame_timing_file);
        frame_timing_file = NULL;
    }

    if (render_stats_file != NULL) {
        fclose(render_stats_file);
        render_stats_file = NULL;
    }

    if (step_stats_file != NULL) {
        fclose(step_stats_file);
        step_stats_file = NULL;
    }

    if (task_stats_file != NULL) {
        fclose(task_stats_file);
        task_stats_file = NULL;
    }

    if (audio_stats_file != NULL) {
        fclose(audio_stats_file);
        audio_stats_file = NULL;
    }

    if (event_log_file != NULL) {
        fclose(event_log_file);
        event_log_file = NULL;
    }

    debug_log_enabled = false;
    debug_log_initialized = false;
    reset_frame_timing_stats();

    if (debug_log_session_path != NULL) {
        SDL_free(debug_log_session_path);
        debug_log_session_path = NULL;
    }
}

bool DebugLog_IsEnabled() {
    return debug_log_enabled;
}

const char* DebugLog_GetSessionPath() {
    return debug_log_session_path;
}

void DebugLog_Write(const char* file_name, const char* text) {
    DebugLog_Printf(file_name, "%s", text);
}

void DebugLog_Printf(const char* file_name, const char* format, ...) {
    va_list args;
    va_start(args, format);
    write_file_v(file_name, "a", format, args);
    va_end(args);
}

void DebugLog_PrintSession(const char* format, ...) {
    va_list args;
    va_start(args, format);
    write_file_v("session.txt", "a", format, args);
    va_end(args);
}

void DebugLog_BeginFrame(uint64_t frame) {
    if (!debug_log_enabled) {
        return;
    }

    current_frame = frame;
    current_adx_process_ms = 0.0;
    current_adx_queued_bytes = 0;
    current_adx_tracks = 0;
    current_adx_start_mem_count = 0;
    current_adx_start_mem_bytes = 0;
    current_adx_start_mem_ms = 0.0;
    current_adx_entry_afs_count = 0;
    current_adx_start_afs_count = 0;
    current_adx_last_file_id = -1;
    current_adx_start_file_ms = 0.0;
    current_adx_load_file_count = 0;
    current_adx_load_file_bytes = 0;
    current_adx_load_file_ms = 0.0;
    current_audio_afs_sync_read_count = 0;
    current_audio_afs_sync_read_sectors = 0;
    current_audio_afs_sync_read_bytes = 0;
    current_audio_afs_sync_read_ms = 0.0;
    current_audio_event = 0;
    current_afs_sync_read_count = 0;
    current_afs_sync_read_sectors = 0;
    current_afs_sync_read_bytes = 0;
    current_afs_sync_read_ms = 0.0;
    current_spu_upload_count = 0;
    current_spu_upload_bytes = 0;
    current_spu_upload_ms = 0.0;
    current_cse_exec_server_count = 0;
    current_cse_exec_server_ms = 0.0;
    current_cse_tsb_request_count = 0;
    current_cse_tsb_request_ms = 0.0;
    current_cse_send_bd_to_spu_count = 0;
    current_cse_send_bd_to_spu_bytes = 0;
    current_cse_send_bd_to_spu_ms = 0.0;
}

void DebugLog_RecordFrameTiming(const DebugFrameTiming* timing) {
    if (!debug_log_enabled || timing == NULL) {
        return;
    }

    if (frame_timing_file != NULL) {
        fprintf(frame_timing_file,
                "%llu,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%d\n",
                (unsigned long long)timing->frame,
                timing->total_ms,
                timing->poll_ms,
                timing->begin_ms,
                timing->game0_ms,
                timing->end_ms,
                timing->game1_ms,
                timing->sleep_ms,
                timing->late_flag);

        if ((timing->frame % 300) == 0) {
            fflush(frame_timing_file);
        }
    }

    store_frame_total_sample(timing->total_ms);
    frame_total_ms_sum += timing->total_ms;

    if (timing->total_ms > worst_frame_ms) {
        worst_frame_ms = timing->total_ms;
        worst_frame = timing->frame;
    }

    update_worst_ms(timing->poll_ms, timing->frame, &worst_poll_ms, &worst_poll_frame);
    update_worst_ms(timing->begin_ms, timing->frame, &worst_begin_ms, &worst_begin_frame);
    update_worst_ms(timing->game0_ms, timing->frame, &worst_game0_ms, &worst_game0_frame);
    update_worst_ms(timing->end_ms, timing->frame, &worst_end_ms, &worst_end_frame);
    update_worst_ms(timing->game1_ms, timing->frame, &worst_game1_ms, &worst_game1_frame);

    if (timing->late_flag) {
        late_frame_count += 1;
        write_event(timing->frame, "late_frame_ms", "%.3f", timing->total_ms);
    }

    write_spike_event(timing->frame, "poll_spike_ms", timing->poll_ms);
    write_spike_event(timing->frame, "begin_spike_ms", timing->begin_ms);
    write_spike_event(timing->frame, "game_step_0_spike_ms", timing->game0_ms);
    write_spike_event(timing->frame, "end_frame_spike_ms", timing->end_ms);
    write_spike_event(timing->frame, "game_step_1_spike_ms", timing->game1_ms);
}

void DebugLog_RecordStepStats(const DebugStepStats* stats) {
    if (!debug_log_enabled || stats == NULL) {
        return;
    }

    if (step_stats_file != NULL) {
        fprintf(step_stats_file,
                "%llu,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,"
                "%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%d,%llu,%.3f,%d,%.3f,%d,%.3f,%d,%llu,%.3f\n",
                (unsigned long long)stats->frame,
                stats->afs_run_server_ms,
                stats->setup_temp_priority_ms,
                stats->pad_get_all_ms,
                stats->key_convert_ms,
                stats->test_prologue_ms,
                stats->input_copy_ms,
                stats->game_main_ms,
                stats->nj_user_main_ms,
                stats->seqs_before_process_ms,
                stats->njdp2d_draw_ms,
                stats->seqs_after_process_ms,
                stats->netplay_tick_ms,
                stats->knj_flush_ms,
                stats->disp_effect_work_ms,
                stats->fl_flip_ms,
                stats->scrn_renew_ms,
                stats->irl_family_ms,
                stats->irl_scrn_ms,
                stats->bgm_server_ms,
                current_spu_upload_count,
                (unsigned long long)current_spu_upload_bytes,
                current_spu_upload_ms,
                current_cse_exec_server_count,
                current_cse_exec_server_ms,
                current_cse_tsb_request_count,
                current_cse_tsb_request_ms,
                current_cse_send_bd_to_spu_count,
                (unsigned long long)current_cse_send_bd_to_spu_bytes,
                current_cse_send_bd_to_spu_ms);

        if ((stats->frame % 300) == 0) {
            fflush(step_stats_file);
        }
    }

    write_spike_event(stats->frame, "afs_run_server_spike_ms", stats->afs_run_server_ms);
    write_spike_event(stats->frame, "setup_temp_priority_spike_ms", stats->setup_temp_priority_ms);
    write_spike_event(stats->frame, "pad_get_all_spike_ms", stats->pad_get_all_ms);
    write_spike_event(stats->frame, "key_convert_spike_ms", stats->key_convert_ms);
    write_spike_event(stats->frame, "test_prologue_spike_ms", stats->test_prologue_ms);
    write_spike_event(stats->frame, "input_copy_spike_ms", stats->input_copy_ms);
    write_spike_event(stats->frame, "game_main_spike_ms", stats->game_main_ms);
    write_spike_event(stats->frame, "nj_user_main_spike_ms", stats->nj_user_main_ms);
    write_spike_event(stats->frame, "seqs_before_process_spike_ms", stats->seqs_before_process_ms);
    write_spike_event(stats->frame, "njdp2d_draw_spike_ms", stats->njdp2d_draw_ms);
    write_spike_event(stats->frame, "seqs_after_process_spike_ms", stats->seqs_after_process_ms);
    write_spike_event(stats->frame, "netplay_tick_spike_ms", stats->netplay_tick_ms);
    write_spike_event(stats->frame, "knj_flush_spike_ms", stats->knj_flush_ms);
    write_spike_event(stats->frame, "disp_effect_work_spike_ms", stats->disp_effect_work_ms);
    write_spike_event(stats->frame, "fl_flip_spike_ms", stats->fl_flip_ms);
    write_spike_event(stats->frame, "scrn_renew_spike_ms", stats->scrn_renew_ms);
    write_spike_event(stats->frame, "irl_family_spike_ms", stats->irl_family_ms);
    write_spike_event(stats->frame, "irl_scrn_spike_ms", stats->irl_scrn_ms);
    write_spike_event(stats->frame, "bgm_server_spike_ms", stats->bgm_server_ms);

    if (current_spu_upload_ms >= 1.0) {
        write_event(stats->frame, "spu_upload_ms", "%.3f", current_spu_upload_ms);
        write_event(stats->frame, "spu_upload_bytes", "%llu", (unsigned long long)current_spu_upload_bytes);
    }

    if (current_cse_exec_server_ms >= 1.0) {
        write_event(stats->frame, "cse_exec_server_ms", "%.3f", current_cse_exec_server_ms);
    }

    if (current_cse_tsb_request_ms >= 1.0) {
        write_event(stats->frame, "cse_tsb_request_ms", "%.3f", current_cse_tsb_request_ms);
    }

    if (current_cse_send_bd_to_spu_ms >= 1.0) {
        write_event(stats->frame, "cse_send_bd_to_spu_ms", "%.3f", current_cse_send_bd_to_spu_ms);
        write_event(
            stats->frame, "cse_send_bd_to_spu_bytes", "%llu", (unsigned long long)current_cse_send_bd_to_spu_bytes);
    }
}

void DebugLog_RecordTaskStats(const DebugTaskStats* stats) {
    if (!debug_log_enabled || stats == NULL) {
        return;
    }

    if (task_stats_file != NULL) {
        fprintf(task_stats_file, "%llu", (unsigned long long)stats->frame);

        for (int i = 0; i < DEBUG_TASK_STATS_COUNT; i++) {
            fprintf(task_stats_file,
                    ",%d,%.3f,%d,%d,%d,%d",
                    stats->condition[i],
                    stats->task_ms[i],
                    stats->r_no[i][0],
                    stats->r_no[i][1],
                    stats->r_no[i][2],
                    stats->r_no[i][3]);
        }

        fprintf(task_stats_file, "\n");

        if ((stats->frame % 300) == 0) {
            fflush(task_stats_file);
        }
    }

    for (int i = 0; i < DEBUG_TASK_STATS_COUNT; i++) {
        if (stats->task_ms[i] > worst_task_ms) {
            worst_task_ms = stats->task_ms[i];
            worst_task_frame = stats->frame;
            worst_task_index = i;
        }

        if (stats->task_ms[i] >= 2.0) {
            char event_name[64];
            SDL_snprintf(event_name, sizeof(event_name), "task_%d_spike_ms", i);
            write_event(stats->frame, event_name, "%.3f", stats->task_ms[i]);
        }
    }
}

void DebugLog_RecordAdxProcess(double elapsed_ms, int queued_bytes, int tracks) {
    if (!debug_log_enabled) {
        return;
    }

    current_adx_process_ms = elapsed_ms;
    current_adx_queued_bytes = queued_bytes;
    current_adx_tracks = tracks;

    if (queued_bytes > max_adx_queued_bytes) {
        max_adx_queued_bytes = queued_bytes;
    }

    if (tracks > max_adx_tracks) {
        max_adx_tracks = tracks;
    }
}

void DebugLog_RecordAdxStartMem(size_t bytes, double elapsed_ms) {
    if (!debug_log_enabled) {
        return;
    }

    current_adx_start_mem_count += 1;
    current_adx_start_mem_bytes += (Uint64)bytes;
    current_adx_start_mem_ms += elapsed_ms;
    current_audio_event |= DEBUG_AUDIO_EVENT_ADX_START_MEM;
    total_adx_start_mem_count += 1;
    total_adx_start_mem_bytes += (Uint64)bytes;
    total_adx_start_mem_ms += elapsed_ms;
    update_worst_ms(elapsed_ms, current_frame, &worst_adx_start_mem_ms, &worst_adx_start_mem_frame);
    write_event(current_frame, "adx_start_mem_bytes", "%llu", (unsigned long long)bytes);
    write_spike_event(current_frame, "adx_start_mem_spike_ms", elapsed_ms);
}

void DebugLog_RecordAdxEntryAfs(int file_id, double elapsed_ms) {
    if (!debug_log_enabled) {
        return;
    }

    current_adx_entry_afs_count += 1;
    current_adx_last_file_id = file_id;
    current_adx_start_file_ms += elapsed_ms;
    current_audio_event |= DEBUG_AUDIO_EVENT_ADX_ENTRY_AFS;
    total_adx_entry_afs_count += 1;
    total_adx_entry_afs_ms += elapsed_ms;
    update_worst_ms(elapsed_ms, current_frame, &worst_adx_entry_afs_ms, &worst_adx_entry_afs_frame);
    write_event(current_frame, "adx_entry_file", "%d", file_id);
    write_spike_event(current_frame, "adx_entry_afs_spike_ms", elapsed_ms);
}

void DebugLog_RecordAdxStartAfs(int file_id, double elapsed_ms) {
    if (!debug_log_enabled) {
        return;
    }

    current_adx_start_afs_count += 1;
    current_adx_last_file_id = file_id;
    current_adx_start_file_ms += elapsed_ms;
    current_audio_event |= DEBUG_AUDIO_EVENT_ADX_START_AFS;
    total_adx_start_afs_count += 1;
    total_adx_start_afs_ms += elapsed_ms;
    update_worst_ms(elapsed_ms, current_frame, &worst_adx_start_afs_ms, &worst_adx_start_afs_frame);
    write_event(current_frame, "adx_start_file", "%d", file_id);
    write_spike_event(current_frame, "adx_start_afs_spike_ms", elapsed_ms);
}

void DebugLog_RecordAdxLoadFile(int file_id, uint32_t bytes, double elapsed_ms) {
    if (!debug_log_enabled) {
        return;
    }

    current_adx_load_file_count += 1;
    current_adx_load_file_bytes += bytes;
    current_adx_load_file_ms += elapsed_ms;
    current_adx_last_file_id = file_id;
    current_audio_event |= DEBUG_AUDIO_EVENT_ADX_LOAD_FILE;
    total_adx_load_file_count += 1;
    total_adx_load_file_bytes += bytes;
    total_adx_load_file_ms += elapsed_ms;
    update_worst_ms(elapsed_ms, current_frame, &worst_adx_load_file_ms, &worst_adx_load_file_frame);
    write_event(current_frame, "adx_load_file", "%d", file_id);
    write_spike_event(current_frame, "adx_load_file_spike_ms", elapsed_ms);
}

void DebugLog_RecordAudioAfsSyncRead(int file_id, int sectors, uint32_t bytes, double elapsed_ms) {
    if (!debug_log_enabled) {
        return;
    }

    current_audio_afs_sync_read_count += 1;
    current_audio_afs_sync_read_sectors += sectors;
    current_audio_afs_sync_read_bytes += bytes;
    current_audio_afs_sync_read_ms += elapsed_ms;
    current_adx_last_file_id = file_id;
    current_audio_event |= DEBUG_AUDIO_EVENT_AUDIO_AFS_SYNC_READ;
    total_audio_afs_sync_reads += 1;
    total_audio_afs_sync_read_bytes += bytes;
    total_audio_afs_sync_read_ms += elapsed_ms;
    update_worst_ms(elapsed_ms, current_frame, &worst_audio_afs_sync_read_ms, &worst_audio_afs_sync_read_frame);

    if (elapsed_ms >= 1.0) {
        write_event(current_frame, "afs_sync_read_ms", "%.3f", elapsed_ms);
        write_event(current_frame, "afs_sync_read_file", "%d", file_id);
        write_event(current_frame, "afs_sync_read_bytes", "%u", (unsigned int)bytes);
    }
}

void DebugLog_RecordAfsSyncRead(int file_id, int sectors, uint32_t bytes, double elapsed_ms) {
    if (!debug_log_enabled) {
        return;
    }

    (void)file_id;
    current_afs_sync_read_count += 1;
    current_afs_sync_read_sectors += sectors;
    current_afs_sync_read_bytes += bytes;
    current_afs_sync_read_ms += elapsed_ms;
    total_afs_sync_reads += 1;
    total_afs_sync_read_bytes += bytes;
    total_afs_sync_read_ms += elapsed_ms;
    update_worst_ms(elapsed_ms, current_frame, &worst_afs_sync_read_ms, &worst_afs_sync_read_frame);
}

void DebugLog_RecordSpuUpload(uint32_t bytes, double elapsed_ms) {
    if (!debug_log_enabled) {
        return;
    }

    current_spu_upload_count += 1;
    current_spu_upload_bytes += bytes;
    current_spu_upload_ms += elapsed_ms;
}

void DebugLog_RecordCseExecServer(double elapsed_ms) {
    if (!debug_log_enabled) {
        return;
    }

    current_cse_exec_server_count += 1;
    current_cse_exec_server_ms += elapsed_ms;
}

void DebugLog_RecordCseTsbRequest(double elapsed_ms) {
    if (!debug_log_enabled) {
        return;
    }

    current_cse_tsb_request_count += 1;
    current_cse_tsb_request_ms += elapsed_ms;
}

void DebugLog_RecordCseSendBdToSpu(uint32_t bytes, double elapsed_ms) {
    if (!debug_log_enabled) {
        return;
    }

    current_cse_send_bd_to_spu_count += 1;
    current_cse_send_bd_to_spu_bytes += bytes;
    current_cse_send_bd_to_spu_ms += elapsed_ms;
}

void DebugLog_RecordRenderStats(const DebugRenderStats* stats) {
    if (!debug_log_enabled || stats == NULL) {
        return;
    }

    if (render_stats_file != NULL) {
        fprintf(render_stats_file,
                "%llu,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%.3f,%d,%.3f,%d,%.3f,%.3f,"
                "%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f\n",
                (unsigned long long)stats->frame,
                stats->render_tasks,
                stats->geometry_calls,
                stats->texture_cache_misses,
                stats->texture_cache_misses_first_use,
                stats->texture_cache_misses_after_palette_unlock,
                stats->texture_cache_misses_after_texture_unlock,
                stats->texture_cache_misses_after_release,
                stats->texture_cache_misses_unknown,
                stats->palette_unlocks,
                stats->texture_unlocks,
                stats->palette_cache_invalidated_textures,
                stats->texture_cache_invalidated_textures,
                stats->release_cache_invalidated_textures,
                stats->indexed_texture_updates,
                stats->indexed_texture_update_pixels,
                stats->indexed_texture_update_ms,
                stats->indexed_palette_updates,
                stats->indexed_palette_update_ms,
                stats->indexed_texture_rgba_fallbacks,
                stats->render_sort_ms,
                stats->render_geometry_ms,
                stats->adx_process_ms,
                stats->netplay_screen_render_ms,
                stats->netstats_render_ms,
                stats->game_renderer_render_ms,
                stats->screenshot_ms,
                stats->screen_copy_ms,
                stats->debug_text_ms,
                stats->present_ms,
                stats->cleanup_ms,
                stats->cursor_ms,
                stats->pacing_ms,
                stats->pacing_overhead_ms,
                stats->sleep_overrun_ms);

        if ((stats->frame % 300) == 0) {
            fflush(render_stats_file);
        }
    }

    write_audio_stats(stats->frame);

    if (stats->render_tasks > max_render_tasks) {
        max_render_tasks = stats->render_tasks;
        if (verbose_events_enabled()) {
            write_event(stats->frame, "render_task_peak", "%d", stats->render_tasks);
        }
    }

    if (stats->geometry_calls > max_geometry_calls) {
        max_geometry_calls = stats->geometry_calls;
    }

    if (stats->texture_cache_misses > 0) {
        total_texture_cache_misses += (Uint64)stats->texture_cache_misses;
        if (verbose_events_enabled()) {
            write_event(stats->frame, "texture_cache_miss", "%d", stats->texture_cache_misses);
        }
    }

    if (stats->texture_cache_misses_first_use > 0) {
        total_texture_cache_misses_first_use += (Uint64)stats->texture_cache_misses_first_use;
        if (verbose_events_enabled()) {
            write_event(stats->frame, "texture_cache_miss_first_use", "%d", stats->texture_cache_misses_first_use);
        }
    }

    if (stats->texture_cache_misses_after_palette_unlock > 0) {
        total_texture_cache_misses_after_palette_unlock +=
            (Uint64)stats->texture_cache_misses_after_palette_unlock;
        if (verbose_events_enabled()) {
            write_event(stats->frame,
                        "texture_cache_miss_after_palette_unlock",
                        "%d",
                        stats->texture_cache_misses_after_palette_unlock);
        }
    }

    if (stats->texture_cache_misses_after_texture_unlock > 0) {
        total_texture_cache_misses_after_texture_unlock +=
            (Uint64)stats->texture_cache_misses_after_texture_unlock;
        if (verbose_events_enabled()) {
            write_event(stats->frame,
                        "texture_cache_miss_after_texture_unlock",
                        "%d",
                        stats->texture_cache_misses_after_texture_unlock);
        }
    }

    if (stats->texture_cache_misses_after_release > 0) {
        total_texture_cache_misses_after_release += (Uint64)stats->texture_cache_misses_after_release;
        if (verbose_events_enabled()) {
            write_event(
                stats->frame, "texture_cache_miss_after_release", "%d", stats->texture_cache_misses_after_release);
        }
    }

    if (stats->texture_cache_misses_unknown > 0) {
        total_texture_cache_misses_unknown += (Uint64)stats->texture_cache_misses_unknown;
        if (verbose_events_enabled()) {
            write_event(stats->frame, "texture_cache_miss_unknown", "%d", stats->texture_cache_misses_unknown);
        }
    }

    if (stats->palette_unlocks > 0) {
        total_palette_unlocks += (Uint64)stats->palette_unlocks;
        if (verbose_events_enabled()) {
            write_event(stats->frame, "palette_unlock", "%d", stats->palette_unlocks);
        }
    }

    if (stats->texture_unlocks > 0) {
        total_texture_unlocks += (Uint64)stats->texture_unlocks;
        if (verbose_events_enabled()) {
            write_event(stats->frame, "texture_unlock", "%d", stats->texture_unlocks);
        }
    }

    if (stats->palette_cache_invalidated_textures > 0) {
        total_palette_cache_invalidated_textures += (Uint64)stats->palette_cache_invalidated_textures;
        if (verbose_events_enabled()) {
            write_event(
                stats->frame, "palette_cache_invalidated_textures", "%d", stats->palette_cache_invalidated_textures);
        }
    }

    if (stats->texture_cache_invalidated_textures > 0) {
        total_texture_cache_invalidated_textures += (Uint64)stats->texture_cache_invalidated_textures;
        if (verbose_events_enabled()) {
            write_event(
                stats->frame, "texture_cache_invalidated_textures", "%d", stats->texture_cache_invalidated_textures);
        }
    }

    if (stats->release_cache_invalidated_textures > 0) {
        total_release_cache_invalidated_textures += (Uint64)stats->release_cache_invalidated_textures;
        if (verbose_events_enabled()) {
            write_event(
                stats->frame, "release_cache_invalidated_textures", "%d", stats->release_cache_invalidated_textures);
        }
    }

    if (stats->indexed_texture_updates > 0) {
        total_indexed_texture_updates += (Uint64)stats->indexed_texture_updates;
        total_indexed_texture_update_pixels += (Uint64)stats->indexed_texture_update_pixels;
        total_indexed_texture_update_ms += stats->indexed_texture_update_ms;
        if (verbose_events_enabled()) {
            write_event(stats->frame, "indexed_texture_updates", "%d", stats->indexed_texture_updates);
        }
    }

    if (stats->indexed_texture_update_ms > worst_indexed_texture_update_ms) {
        worst_indexed_texture_update_ms = stats->indexed_texture_update_ms;
        worst_indexed_texture_update_frame = stats->frame;
    }

    if (stats->indexed_palette_updates > 0) {
        total_indexed_palette_updates += (Uint64)stats->indexed_palette_updates;
        total_indexed_palette_update_ms += stats->indexed_palette_update_ms;
        if (verbose_events_enabled()) {
            write_event(stats->frame, "indexed_palette_updates", "%d", stats->indexed_palette_updates);
        }
    }

    if (stats->indexed_palette_update_ms > worst_indexed_palette_update_ms) {
        worst_indexed_palette_update_ms = stats->indexed_palette_update_ms;
        worst_indexed_palette_update_frame = stats->frame;
    }

    if (stats->indexed_texture_rgba_fallbacks > 0) {
        total_indexed_texture_rgba_fallbacks += (Uint64)stats->indexed_texture_rgba_fallbacks;
        write_event(stats->frame, "indexed_texture_rgba_fallbacks", "%d", stats->indexed_texture_rgba_fallbacks);
    }

    update_worst_ms(stats->render_sort_ms, stats->frame, &worst_render_sort_ms, &worst_render_sort_frame);
    update_worst_ms(stats->render_geometry_ms, stats->frame, &worst_render_geometry_ms, &worst_render_geometry_frame);
    update_worst_ms(stats->adx_process_ms, stats->frame, &worst_adx_process_ms, &worst_adx_process_frame);
    update_worst_ms(stats->netplay_screen_render_ms,
                    stats->frame,
                    &worst_netplay_screen_render_ms,
                    &worst_netplay_screen_render_frame);
    update_worst_ms(
        stats->netstats_render_ms, stats->frame, &worst_netstats_render_ms, &worst_netstats_render_frame);
    update_worst_ms(stats->game_renderer_render_ms,
                    stats->frame,
                    &worst_game_renderer_render_ms,
                    &worst_game_renderer_render_frame);
    update_worst_ms(stats->screenshot_ms, stats->frame, &worst_screenshot_ms, &worst_screenshot_frame);
    update_worst_ms(stats->screen_copy_ms, stats->frame, &worst_screen_copy_ms, &worst_screen_copy_frame);
    update_worst_ms(stats->debug_text_ms, stats->frame, &worst_debug_text_ms, &worst_debug_text_frame);
    update_worst_ms(stats->present_ms, stats->frame, &worst_present_ms, &worst_present_frame);
    update_worst_ms(stats->cleanup_ms, stats->frame, &worst_cleanup_ms, &worst_cleanup_frame);
    update_worst_ms(stats->cursor_ms, stats->frame, &worst_cursor_ms, &worst_cursor_frame);
    update_worst_ms(stats->pacing_ms, stats->frame, &worst_pacing_ms, &worst_pacing_frame);
    update_worst_ms(stats->pacing_overhead_ms,
                    stats->frame,
                    &worst_pacing_overhead_ms,
                    &worst_pacing_overhead_frame);
    update_worst_ms(stats->sleep_overrun_ms, stats->frame, &worst_sleep_overrun_ms, &worst_sleep_overrun_frame);

    if ((stats->render_sort_ms + stats->render_geometry_ms) >= 4.0) {
        write_event(stats->frame, "render_spike_ms", "%.3f", stats->render_sort_ms + stats->render_geometry_ms);
    }

    write_spike_event(stats->frame, "adx_process_spike_ms", stats->adx_process_ms);
    write_spike_event(stats->frame, "netplay_screen_render_spike_ms", stats->netplay_screen_render_ms);
    write_spike_event(stats->frame, "netstats_render_spike_ms", stats->netstats_render_ms);
    write_spike_event(stats->frame, "game_renderer_render_spike_ms", stats->game_renderer_render_ms);
    write_spike_event(stats->frame, "screenshot_spike_ms", stats->screenshot_ms);
    write_spike_event(stats->frame, "screen_copy_spike_ms", stats->screen_copy_ms);
    write_spike_event(stats->frame, "debug_text_spike_ms", stats->debug_text_ms);
    write_spike_event(stats->frame, "present_spike_ms", stats->present_ms);
    write_spike_event(stats->frame, "cleanup_spike_ms", stats->cleanup_ms);
    write_spike_event(stats->frame, "cursor_spike_ms", stats->cursor_ms);
    write_spike_event(stats->frame, "pacing_overhead_spike_ms", stats->pacing_overhead_ms);
    write_spike_event(stats->frame, "sleep_overrun_spike_ms", stats->sleep_overrun_ms);

    if (event_log_file != NULL && (stats->frame % 300) == 0) {
        fflush(event_log_file);
    }
}
