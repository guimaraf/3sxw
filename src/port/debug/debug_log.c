#include "port/debug/debug_log.h"
#include "port/paths.h"

#include <SDL3/SDL.h>

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static bool debug_log_enabled = false;
static bool debug_log_initialized = false;
static char* debug_log_session_path = NULL;
static FILE* frame_timing_file = NULL;
static FILE* render_stats_file = NULL;
static FILE* event_log_file = NULL;
static Uint64 session_start_ns = 0;
static double* frame_total_ms_samples = NULL;
static size_t frame_total_ms_count = 0;
static size_t frame_total_ms_capacity = 0;
static double frame_total_ms_sum = 0.0;
static double worst_frame_ms = 0.0;
static Uint64 worst_frame = 0;
static Uint64 late_frame_count = 0;
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
static double worst_render_sort_ms = 0.0;
static double worst_render_geometry_ms = 0.0;
static Uint64 worst_render_sort_frame = 0;
static Uint64 worst_render_geometry_frame = 0;

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
    fprintf(file, "worst_render_sort_ms=%.3f\n", worst_render_sort_ms);
    fprintf(file, "worst_render_sort_frame=%llu\n", (unsigned long long)worst_render_sort_frame);
    fprintf(file, "worst_render_geometry_ms=%.3f\n", worst_render_geometry_ms);
    fprintf(file, "worst_render_geometry_frame=%llu\n", (unsigned long long)worst_render_geometry_frame);
    fprintf(file, "frame_timing_csv=%sframe_timing.csv\n", debug_log_session_path);
    fprintf(file, "render_stats_csv=%srender_stats.csv\n", debug_log_session_path);
    fprintf(file, "event_log_csv=%sevent_log.csv\n", debug_log_session_path);
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
    worst_render_sort_ms = 0.0;
    worst_render_geometry_ms = 0.0;
    worst_render_sort_frame = 0;
    worst_render_geometry_frame = 0;
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
            "indexed_texture_update_ms,render_sort_ms,render_geometry_ms\n");
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

void DebugLog_Init(int enabled, int argc, const char* command_line) {
    if (debug_log_initialized) {
        return;
    }

    debug_log_initialized = true;

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
    open_render_stats_file();
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

    if (timing->late_flag) {
        late_frame_count += 1;
    }
}

void DebugLog_RecordRenderStats(const DebugRenderStats* stats) {
    if (!debug_log_enabled || stats == NULL) {
        return;
    }

    if (render_stats_file != NULL) {
        fprintf(render_stats_file,
                "%llu,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%.3f,%.3f,%.3f\n",
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
                stats->render_sort_ms,
                stats->render_geometry_ms);

        if ((stats->frame % 300) == 0) {
            fflush(render_stats_file);
        }
    }

    if (stats->render_tasks > max_render_tasks) {
        max_render_tasks = stats->render_tasks;
        write_event(stats->frame, "render_task_peak", "%d", stats->render_tasks);
    }

    if (stats->geometry_calls > max_geometry_calls) {
        max_geometry_calls = stats->geometry_calls;
    }

    if (stats->texture_cache_misses > 0) {
        total_texture_cache_misses += (Uint64)stats->texture_cache_misses;
        write_event(stats->frame, "texture_cache_miss", "%d", stats->texture_cache_misses);
    }

    if (stats->texture_cache_misses_first_use > 0) {
        total_texture_cache_misses_first_use += (Uint64)stats->texture_cache_misses_first_use;
        write_event(stats->frame, "texture_cache_miss_first_use", "%d", stats->texture_cache_misses_first_use);
    }

    if (stats->texture_cache_misses_after_palette_unlock > 0) {
        total_texture_cache_misses_after_palette_unlock +=
            (Uint64)stats->texture_cache_misses_after_palette_unlock;
        write_event(stats->frame,
                    "texture_cache_miss_after_palette_unlock",
                    "%d",
                    stats->texture_cache_misses_after_palette_unlock);
    }

    if (stats->texture_cache_misses_after_texture_unlock > 0) {
        total_texture_cache_misses_after_texture_unlock +=
            (Uint64)stats->texture_cache_misses_after_texture_unlock;
        write_event(stats->frame,
                    "texture_cache_miss_after_texture_unlock",
                    "%d",
                    stats->texture_cache_misses_after_texture_unlock);
    }

    if (stats->texture_cache_misses_after_release > 0) {
        total_texture_cache_misses_after_release += (Uint64)stats->texture_cache_misses_after_release;
        write_event(stats->frame, "texture_cache_miss_after_release", "%d", stats->texture_cache_misses_after_release);
    }

    if (stats->texture_cache_misses_unknown > 0) {
        total_texture_cache_misses_unknown += (Uint64)stats->texture_cache_misses_unknown;
        write_event(stats->frame, "texture_cache_miss_unknown", "%d", stats->texture_cache_misses_unknown);
    }

    if (stats->palette_unlocks > 0) {
        total_palette_unlocks += (Uint64)stats->palette_unlocks;
        write_event(stats->frame, "palette_unlock", "%d", stats->palette_unlocks);
    }

    if (stats->texture_unlocks > 0) {
        total_texture_unlocks += (Uint64)stats->texture_unlocks;
        write_event(stats->frame, "texture_unlock", "%d", stats->texture_unlocks);
    }

    if (stats->palette_cache_invalidated_textures > 0) {
        total_palette_cache_invalidated_textures += (Uint64)stats->palette_cache_invalidated_textures;
        write_event(
            stats->frame, "palette_cache_invalidated_textures", "%d", stats->palette_cache_invalidated_textures);
    }

    if (stats->texture_cache_invalidated_textures > 0) {
        total_texture_cache_invalidated_textures += (Uint64)stats->texture_cache_invalidated_textures;
        write_event(
            stats->frame, "texture_cache_invalidated_textures", "%d", stats->texture_cache_invalidated_textures);
    }

    if (stats->release_cache_invalidated_textures > 0) {
        total_release_cache_invalidated_textures += (Uint64)stats->release_cache_invalidated_textures;
        write_event(
            stats->frame, "release_cache_invalidated_textures", "%d", stats->release_cache_invalidated_textures);
    }

    if (stats->indexed_texture_updates > 0) {
        total_indexed_texture_updates += (Uint64)stats->indexed_texture_updates;
        total_indexed_texture_update_pixels += (Uint64)stats->indexed_texture_update_pixels;
        total_indexed_texture_update_ms += stats->indexed_texture_update_ms;
        write_event(stats->frame, "indexed_texture_updates", "%d", stats->indexed_texture_updates);
    }

    if (stats->indexed_texture_update_ms > worst_indexed_texture_update_ms) {
        worst_indexed_texture_update_ms = stats->indexed_texture_update_ms;
        worst_indexed_texture_update_frame = stats->frame;
    }

    if (stats->render_sort_ms > worst_render_sort_ms) {
        worst_render_sort_ms = stats->render_sort_ms;
        worst_render_sort_frame = stats->frame;
    }

    if (stats->render_geometry_ms > worst_render_geometry_ms) {
        worst_render_geometry_ms = stats->render_geometry_ms;
        worst_render_geometry_frame = stats->frame;
    }

    if ((stats->render_sort_ms + stats->render_geometry_ms) >= 4.0) {
        write_event(stats->frame, "render_spike_ms", "%.3f", stats->render_sort_ms + stats->render_geometry_ms);
    }

    if (event_log_file != NULL && (stats->frame % 300) == 0) {
        fflush(event_log_file);
    }
}
