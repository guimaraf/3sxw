#if defined(SF3_UWP)
/* Include the SDK before common.h defines the function-like BOOL macro. */
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#include "port/sdl/sdl_app.h"
#include "port/uwp_diagnostics.h"
#include "common.h"
#include "port/config/config.h"
#include "port/config/keymap.h"
#include "port/debug/debug_log.h"
#include "port/sdl/sdl_bezel.h"
#include "port/sdl/sdl_debug_text.h"
#include "port/sdl/sdl_game_renderer.h"
#include "port/sdl/sdl_message_renderer.h"
#include "port/sdl/sdl_pad.h"
#include "port/sdl/sdl_scanlines.h"
#include "port/sdl/sdl_screenshot.h"
#include "port/sound/adx.h"
#include "port/utils.h"
#include "sf33rd/AcrSDK/ps2/foundaps2.h"

#include <SDL3/SDL.h>
#include <limits.h>

#if defined(SF3_UWP)
#include "port/paths.h"
#include "sf33rd/Source/Game/engine/workuser.h"
#include "sf33rd/Source/Game/engine/plcnt.h"
#include "sf33rd/Source/Game/stage/bg.h"
#include "sf33rd/Source/Game/system/work_sys.h"
#endif

typedef enum ScaleMode {
    SCALEMODE_NEAREST,
    SCALEMODE_LINEAR,
    SCALEMODE_SOFT_LINEAR,
    SCALEMODE_SQUARE_PIXELS,
} ScaleMode;

static const char* app_name = "Street Fighter III: 3rd Strike";
static const float display_target_ratio = 4.0 / 3.0;
static const int window_min_width = 384;
static const int window_min_height = (int)(window_min_width / display_target_ratio);
static const double frame_rate_arcade = 59.59949;
static const double frame_rate_ps2 = 60000.0 / 1001.0;

SDL_Window* window = NULL;
static SDL_Renderer* renderer = NULL;
static SDL_Texture* screen_texture = NULL;
static ScaleMode scale_mode = SCALEMODE_NEAREST;
static bool stretch_aspect_ratio = false;
static bool ps2_frame_timing = false;
static double target_frame_rate = 59.59949;
static Uint64 target_frame_time_ns = 1000000000.0 / 59.59949;

static Uint64 frame_deadline = 0;

static bool should_save_screenshot = false;

#if defined(SF3_UWP)
#define UWP_RENDER_LOG_BUFFER_SIZE 4096

typedef struct UWPRenderLogCapture {
    char messages[UWP_RENDER_LOG_BUFFER_SIZE];
    SDL_LogOutputFunction previous_output;
    void* previous_userdata;
} UWPRenderLogCapture;

static UWPRenderLogCapture uwp_render_log_capture;
static char uwp_window_renderer_error[1024];

static SDL_IOStream* uwp_render_trace_file;
static SDL_LogOutputFunction uwp_trace_previous_output;
static void* uwp_trace_previous_userdata;
static size_t uwp_render_trace_bytes;
static Uint64 uwp_render_trace_frame;
static bool uwp_render_trace_sample;
static unsigned int uwp_render_trace_samples;
#define UWP_TRACE_QUEUE_CAPACITY 128
#define UWP_TRACE_QUEUE_SLOTS (UWP_TRACE_QUEUE_CAPACITY + 1)
#define UWP_TRACE_LINE_SIZE 4096
#define UWP_TRACE_LIMIT (4 * 1024 * 1024)
static char uwp_trace_queue[UWP_TRACE_QUEUE_SLOTS][UWP_TRACE_LINE_SIZE];
static SDL_AtomicInt uwp_trace_head, uwp_trace_tail, uwp_trace_high_water;
static SDL_Semaphore* uwp_trace_wakeup;
static SDL_Thread* uwp_trace_thread;
static SDL_AtomicInt uwp_trace_dropped;
static SDL_AtomicInt uwp_trace_stop;
static bool uwp_trace_verbose;
static SDL_ThreadID uwp_trace_main_thread;
static Uint64 uwp_frame_start_ns, uwp_previous_frame_start_ns;
static double uwp_frame_interval_ms, uwp_pre_render_ms;
static struct {
    const char* name;
    double ms;
    unsigned int calls;
} uwp_game_phases[32];
static unsigned int uwp_game_phase_count;

void SDLApp_RecordUWPGamePhase(const char* name, Uint64 elapsed_ns) {
    if (SF3_UWP_DIAGNOSTIC_DISK_LOGGING && uwp_trace_thread == NULL) {
        return;
    }
    unsigned int i;
    for (i = 0; i < uwp_game_phase_count; i++) {
        if (SDL_strcmp(uwp_game_phases[i].name, name) == 0) {
            break;
        }
    }
    if (i == SDL_arraysize(uwp_game_phases)) {
        return;
    }
    if (i == uwp_game_phase_count) {
        uwp_game_phase_count++;
        uwp_game_phases[i].name = name;
        uwp_game_phases[i].ms = 0.0;
        uwp_game_phases[i].calls = 0;
    }
    uwp_game_phases[i].ms += (double)elapsed_ns / 1e6;
    uwp_game_phases[i].calls++;
}

/* Main-thread-only combat samples; flushed after EndFrame assigns the frame ID.
 * Call durations are inclusive and may contain waits. No hot-path allocation. */
static struct {
    const char* kind;
    int subject, detail, value;
    Uint64 at_ns, elapsed_ns;
} uwp_combat_details[64];
static unsigned int uwp_combat_count, uwp_combat_dropped;
static unsigned int uwp_combat_events, uwp_combat_timings;

/* Only the main game thread uses this trace. No game state is changed. */
static struct {
    bool active, cpu_valid;
    int character, subject;
    Uint64 start_ns, cpu_100ns, marks[UWP_ATTACK_MARKS];
} uwp_attack_trace;

static bool uwp_thread_cpu_time(Uint64* result) {
    FILETIME created, exited, kernel, user;
    if (!GetThreadTimes(GetCurrentThread(), &created, &exited, &kernel, &user)) {
        return false;
    }
    *result = (((Uint64)kernel.dwHighDateTime << 32) | kernel.dwLowDateTime) +
              (((Uint64)user.dwHighDateTime << 32) | user.dwLowDateTime);
    return true;
}

void SDLApp_UWPAttackTraceBegin(int character, int subject) {
    SDL_zero(uwp_attack_trace);
    uwp_attack_trace.active = true;
    uwp_attack_trace.character = character;
    uwp_attack_trace.subject = subject;
    uwp_attack_trace.cpu_valid = uwp_thread_cpu_time(&uwp_attack_trace.cpu_100ns);
    uwp_attack_trace.start_ns = SDL_GetTicksNS();
}

void SDLApp_UWPAttackTraceMark(int mark) {
    if (uwp_attack_trace.active && mark >= 0 && mark < UWP_ATTACK_MARKS &&
        uwp_attack_trace.marks[mark] == 0) {
        uwp_attack_trace.marks[mark] = SDL_GetTicksNS();
    }
}

static double uwp_attack_mark_ms(int mark) {
    return uwp_attack_trace.marks[mark] ?
        (double)(uwp_attack_trace.marks[mark] - uwp_attack_trace.start_ns) / 1e6 : -1.0;
}

void SDLApp_UWPAttackTraceEnd(void) {
    if (!uwp_attack_trace.active) return;
    const Uint64 end_ns = SDL_GetTicksNS();
    Uint64 cpu_end = 0;
    const bool cpu_valid = uwp_attack_trace.cpu_valid && uwp_thread_cpu_time(&cpu_end) &&
                           cpu_end >= uwp_attack_trace.cpu_100ns;
    if (end_ns - uwp_attack_trace.start_ns >= 1000000) {
        SDL_Log("[UWP_COMBAT] kind=attack_entry_trace character=%d subject=%d elapsed_ms=%.3f "
                "cpu_valid=%d cpu_ms=%.3f wrapper_ms=%.3f selected_ms=%.3f shared_ms=%.3f "
                "landing_done_ms=%.3f animation_done_ms=%.3f",
                uwp_attack_trace.character, uwp_attack_trace.subject,
                (double)(end_ns - uwp_attack_trace.start_ns) / 1e6, (int)cpu_valid,
                cpu_valid ? (double)(cpu_end - uwp_attack_trace.cpu_100ns) / 10000.0 : -1.0,
                uwp_attack_mark_ms(UWP_ATTACK_WRAPPER), uwp_attack_mark_ms(UWP_ATTACK_SELECTED),
                uwp_attack_mark_ms(UWP_ATTACK_SHARED), uwp_attack_mark_ms(UWP_ATTACK_LANDING),
                uwp_attack_mark_ms(UWP_ATTACK_ANIMATION));
    }
    uwp_attack_trace.active = false;
}

void SDLApp_RecordUWPCombatDetail(const char* kind, int subject, int detail, int value, Uint64 elapsed_ns) {
    if ((SF3_UWP_DIAGNOSTIC_DISK_LOGGING && uwp_trace_thread == NULL) || G_No[0] != 2 || G_No[1] != 2 ||
        (elapsed_ns != 0 && elapsed_ns < 1000000)) {
        return;
    }
    if (uwp_combat_count == SDL_arraysize(uwp_combat_details) ||
        (elapsed_ns == 0 ? uwp_combat_events >= 512 : uwp_combat_timings >= 1024)) {
        uwp_combat_dropped++;
        return;
    }
    const unsigned int i = uwp_combat_count++;
    uwp_combat_details[i].kind = kind;
    uwp_combat_details[i].subject = subject;
    uwp_combat_details[i].detail = detail;
    uwp_combat_details[i].value = value;
    uwp_combat_details[i].at_ns = SDL_GetTicksNS();
    uwp_combat_details[i].elapsed_ns = elapsed_ns;
    if (elapsed_ns == 0) {
        uwp_combat_events++;
    } else {
        uwp_combat_timings++;
    }
}

/* Vendored SDL serializes log callbacks with SDL_log_function_lock. Atomic
 * publication transfers each completed slot to the single consumer; no queue
 * mutex or disk I/O is used by producers. One extra slot distinguishes full/empty. */
static int uwp_render_trace_worker(void* userdata) {
    (void)userdata;
    SDL_SetCurrentThreadPriority(SDL_THREAD_PRIORITY_LOW);
    char header[512];
    SDL_snprintf(header, sizeof(header),
        "ticks_ms=%llu [UWP_RENDER] session build=%s %s SDL_headers=%d SDL_runtime=%d "
        "render_trace_version=18 limit_bytes=4194304 async_log=1 verbose=%d "
        "logical_cpus=%d main_thread=%llu log_worker=%llu\n",
        (unsigned long long)SDL_GetTicks(), __DATE__, __TIME__, SDL_VERSION, SDL_GetVersion(),
        uwp_trace_verbose, SDL_GetNumLogicalCPUCores(), (unsigned long long)uwp_trace_main_thread,
        (unsigned long long)SDL_GetCurrentThreadID());
    const size_t header_length = SDL_strlen(header);
    uwp_render_trace_bytes = SDL_WriteIO(uwp_render_trace_file, header, header_length);
    Uint64 last_flush = SDL_GetTicks(), last_status = last_flush;
    double max_flush_ms = 0.0;
    unsigned int io_errors = uwp_render_trace_bytes != header_length, capped_lines = 0;
    for (;;) {
        char batch[4][UWP_TRACE_LINE_SIZE];
        unsigned int count = 0, high_water;
        int head = SDL_GetAtomicInt(&uwp_trace_head);
        if (head == SDL_GetAtomicInt(&uwp_trace_tail) && !SDL_GetAtomicInt(&uwp_trace_stop)) {
            SDL_WaitSemaphoreTimeout(uwp_trace_wakeup, 250);
        }
        while (count < SDL_arraysize(batch) && head != SDL_GetAtomicInt(&uwp_trace_tail)) {
            SDL_strlcpy(batch[count++], uwp_trace_queue[head], UWP_TRACE_LINE_SIZE);
            head = (head + 1) % UWP_TRACE_QUEUE_SLOTS;
            SDL_SetAtomicInt(&uwp_trace_head, head);
        }
        const bool done = SDL_GetAtomicInt(&uwp_trace_stop) && head == SDL_GetAtomicInt(&uwp_trace_tail);
        high_water = (unsigned int)SDL_GetAtomicInt(&uwp_trace_high_water);
        for (unsigned int i = 0; i < count; i++) {
            const size_t length = SDL_strlen(batch[i]);
            if (uwp_render_trace_bytes + length > UWP_TRACE_LIMIT - 1024) {
                capped_lines++;
                continue;
            }
            const size_t written = SDL_WriteIO(uwp_render_trace_file, batch[i], length);
            uwp_render_trace_bytes += written;
            io_errors += written != length;
        }
        const Uint64 now = SDL_GetTicks();
        if (done || now - last_status >= 5000) {
            char status[512];
            SDL_snprintf(status, sizeof(status),
                "ticks_ms=%llu [UWP_LOG] shutdown=%d dropped=%d queue_high_water=%u capacity=%d "
                "bytes=%llu capped_lines=%u io_errors=%u max_flush_ms=%.3f\n",
                (unsigned long long)now, done, SDL_GetAtomicInt(&uwp_trace_dropped), high_water,
                UWP_TRACE_QUEUE_CAPACITY, (unsigned long long)uwp_render_trace_bytes,
                capped_lines, io_errors, max_flush_ms);
            if (uwp_render_trace_bytes + SDL_strlen(status) <= UWP_TRACE_LIMIT - (done ? 0 : 512)) {
                const size_t length = SDL_strlen(status);
                const size_t written = SDL_WriteIO(uwp_render_trace_file, status, length);
                uwp_render_trace_bytes += written;
                io_errors += written != length;
            }
            last_status = now;
        }
        if (done || now - last_flush >= 1000) {
            const Uint64 start = SDL_GetTicksNS();
            io_errors += !SDL_FlushIO(uwp_render_trace_file);
            max_flush_ms = SDL_max(max_flush_ms, (double)(SDL_GetTicksNS() - start) / 1e6);
            last_flush = SDL_GetTicks();
        }
        if (done) {
            break;
        }
    }
    return 0;
}

static bool trace_uwp_render_result(bool success, const char* operation) {
    static unsigned int errors;
    if (!success && errors < 32) {
        errors++;
        SDL_Log("[UWP_RENDER] frame=%llu operation_failed=%s error=%s",
                (unsigned long long)uwp_render_trace_frame, operation, SDL_GetError());
    }
    return success;
}
#define UWP_RENDER_CALL(call) trace_uwp_render_result((call), #call)

static void SDLCALL write_uwp_render_trace(void* userdata, int category,
                                         SDL_LogPriority priority, const char* message) {
    (void)userdata;
    if (message != NULL && (SDL_strstr(message, "[UWP_RENDER]") != NULL ||
        SDL_strstr(message, "[UWP_PERF]") != NULL ||
        SDL_strstr(message, "[UWP_COMBAT]") != NULL ||
        SDL_strstr(message, "[UWP_PAD]") != NULL ||
        SDL_strstr(message, "[UWP_AUDIO]") != NULL ||
        ((category == SDL_LOG_CATEGORY_RENDER || category == SDL_LOG_CATEGORY_VIDEO) &&
         priority >= SDL_LOG_PRIORITY_WARN))) {
        char line[4096];
        SDL_snprintf(line, sizeof(line), "ticks_ms=%llu category=%d priority=%d %s\n",
                     (unsigned long long)SDL_GetTicks(), category, (int)priority, message);
        const int tail = SDL_GetAtomicInt(&uwp_trace_tail);
        const int next = (tail + 1) % UWP_TRACE_QUEUE_SLOTS;
        const int head = SDL_GetAtomicInt(&uwp_trace_head);
        if (next != head) {
            SDL_strlcpy(uwp_trace_queue[tail], line, UWP_TRACE_LINE_SIZE);
            SDL_SetAtomicInt(&uwp_trace_tail, next);
            const int queued = (next + UWP_TRACE_QUEUE_SLOTS - head) % UWP_TRACE_QUEUE_SLOTS;
            if (queued > SDL_GetAtomicInt(&uwp_trace_high_water)) {
                SDL_SetAtomicInt(&uwp_trace_high_water, queued);
            }
            SDL_SignalSemaphore(uwp_trace_wakeup);
        } else {
            SDL_AddAtomicInt(&uwp_trace_dropped, 1);
        }
        /* OutputDebugString is also synchronous; don't forward captured diagnostics. */
        return;
    }
    if (uwp_trace_previous_output != NULL) {
        uwp_trace_previous_output(uwp_trace_previous_userdata, category, priority, message);
    }
}

static void SDLCALL discard_uwp_diagnostic_output(void* userdata, int category,
                                                 SDL_LogPriority priority, const char* message) {
    (void)userdata;
    if (message != NULL && SDL_strstr(message, "[UWP_") != NULL) return;
    if (uwp_trace_previous_output != NULL) {
        uwp_trace_previous_output(uwp_trace_previous_userdata, category, priority, message);
    }
}

static void init_uwp_render_trace(void) {
    if (!SF3_UWP_DIAGNOSTIC_DISK_LOGGING) {
        SDL_GetLogOutputFunction(&uwp_trace_previous_output, &uwp_trace_previous_userdata);
        SDL_SetLogOutputFunction(discard_uwp_diagnostic_output, NULL);
        return;
    }
    const char* directory = Paths_GetDataPath();
    char* path = NULL;
    if (directory == NULL || SDL_asprintf(&path, "%suwp_render_check.log", directory) < 0) {
        return;
    }
    uwp_trace_wakeup = SDL_CreateSemaphore(0);
    if (uwp_trace_wakeup != NULL) {
        uwp_render_trace_file = SDL_IOFromFile(path, "wb");
    }
    SDL_free(path);
    if (uwp_render_trace_file == NULL) {
        SDL_DestroySemaphore(uwp_trace_wakeup);
        uwp_trace_wakeup = NULL;
        SDL_Log("Couldn't open the UWP render diagnostic: %s", SDL_GetError());
        return;
    }
    uwp_render_trace_bytes = 0;
    uwp_render_trace_frame = 0;
    uwp_render_trace_samples = 0;
    SDL_SetAtomicInt(&uwp_trace_head, 0);
    SDL_SetAtomicInt(&uwp_trace_tail, 0);
    SDL_SetAtomicInt(&uwp_trace_high_water, 0);
    SDL_SetAtomicInt(&uwp_trace_dropped, 0);
    SDL_SetAtomicInt(&uwp_trace_stop, 0);
    uwp_trace_verbose = SDL_GetHintBoolean("SF3_UWP_VERBOSE_RENDER_TRACE", false);
    uwp_trace_main_thread = SDL_GetCurrentThreadID();
    uwp_trace_thread = SDL_CreateThread(uwp_render_trace_worker, "UWP Diagnostic Writer", NULL);
    if (uwp_trace_thread == NULL) {
        SDL_CloseIO(uwp_render_trace_file);
        uwp_render_trace_file = NULL;
        SDL_DestroySemaphore(uwp_trace_wakeup);
        uwp_trace_wakeup = NULL;
        SDL_Log("Couldn't start the UWP diagnostic worker: %s", SDL_GetError());
        return;
    }
    SDL_GetLogOutputFunction(&uwp_trace_previous_output, &uwp_trace_previous_userdata);
    SDL_SetLogOutputFunction(write_uwp_render_trace, NULL);
}

static void trace_uwp_target(const char* stage, bool force) {
    if (!uwp_render_trace_sample && !force) {
        return;
    }
    SDL_Texture* target = SDL_GetRenderTarget(renderer);
    SDL_Rect viewport = { 0 }, clip = { 0 };
    int current_w = 0, current_h = 0, logical_w = 0, logical_h = 0;
    float scale_x = 0, scale_y = 0;
    SDL_RendererLogicalPresentation logical_mode = SDL_LOGICAL_PRESENTATION_DISABLED;
    const bool size_ok = SDL_GetCurrentRenderOutputSize(renderer, &current_w, &current_h);
    const bool viewport_ok = SDL_GetRenderViewport(renderer, &viewport);
    const bool clip_ok = SDL_GetRenderClipRect(renderer, &clip);
    const bool scale_ok = SDL_GetRenderScale(renderer, &scale_x, &scale_y);
    const bool logical_ok = SDL_GetRenderLogicalPresentation(renderer, &logical_w, &logical_h, &logical_mode);
    SDL_Log("[UWP_RENDER] frame=%llu stage=%s target=%p target_size=%dx%d "
            "current=%dx%d viewport=%d,%d,%d,%d scale=%g,%g "
            "clip_enabled=%d clip=%d,%d,%d,%d logical=%dx%d mode=%d queries_ok=%d%d%d%d%d",
            (unsigned long long)uwp_render_trace_frame, stage, (void*)target,
            target ? target->w : 0, target ? target->h : 0, current_w, current_h,
            viewport.x, viewport.y, viewport.w, viewport.h, scale_x, scale_y,
            SDL_RenderClipEnabled(renderer), clip.x, clip.y, clip.w, clip.h,
            logical_w, logical_h, (int)logical_mode, size_ok, viewport_ok, clip_ok, scale_ok, logical_ok);
}

static void begin_uwp_render_sample(void) {
    static u8 previous_state[8];
    static bool state_known;
    static unsigned int followup_frames;
    u8 state[8];
    SDL_memcpy(state, G_No, 4);
    SDL_memcpy(state + 4, D_No, 4);
    const bool changed = !state_known || SDL_memcmp(state, previous_state, sizeof(state)) != 0;
    if (changed) {
        SDL_memcpy(previous_state, state, sizeof(state));
        state_known = true;
        followup_frames = 3;
    }
    uwp_render_trace_frame++;
    if (changed && uwp_trace_thread != NULL) {
        SDL_Log("[UWP_PERF] state frame=%llu G=%u,%u,%u,%u D=%u,%u,%u,%u "
                "stage=%d area=%d p1=%d p2=%d bg_x=%d p1_x=%d p2_x=%d",
                (unsigned long long)uwp_render_trace_frame,
                G_No[0], G_No[1], G_No[2], G_No[3], D_No[0], D_No[1], D_No[2], D_No[3],
                bg_w.stage, bg_w.area, plw[0].player_number, plw[1].player_number,
                bg_w.bgw[1].wxy[0].disp.pos, plw[0].wu.xyz[0].disp.pos, plw[1].wu.xyz[0].disp.pos);
    }
    uwp_render_trace_sample = uwp_trace_verbose && uwp_trace_thread != NULL && uwp_render_trace_samples < 160 &&
        (changed || followup_frames > 0 || uwp_render_trace_frame % 300 == 0);
    SDL_SetBooleanProperty(SDL_GetRendererProperties(renderer), "sf3.uwp.render_trace_frame", uwp_render_trace_sample);
    if (!uwp_render_trace_sample) {
        return;
    }
    uwp_render_trace_samples++;
    if (followup_frames > 0) {
        followup_frames--;
    }
    int window_w = 0, window_h = 0, pixel_w = 0, pixel_h = 0, output_w = 0, output_h = 0;
    SDL_GetWindowSize(window, &window_w, &window_h);
    SDL_GetWindowSizeInPixels(window, &pixel_w, &pixel_h);
    SDL_GetRenderOutputSize(renderer, &output_w, &output_h);
    SDL_Log("[UWP_RENDER] frame=%llu state_changed=%d G=%u,%u,%u,%u D=%u,%u,%u,%u "
            "scr_sc=%g renderer=%s window=%dx%d pixels=%dx%d output=%dx%d "
            "screen=%dx%d scale_mode=%d stretch=%d",
            (unsigned long long)uwp_render_trace_frame, changed,
            G_No[0], G_No[1], G_No[2], G_No[3], D_No[0], D_No[1], D_No[2], D_No[3],
            scr_sc, SDL_GetRendererName(renderer), window_w, window_h, pixel_w, pixel_h,
            output_w, output_h, screen_texture ? screen_texture->w : 0,
            screen_texture ? screen_texture->h : 0, (int)scale_mode, stretch_aspect_ratio);
}

/* Fixed-size aggregates: no per-frame allocations or synchronous file operations. */
static void log_uwp_cache_stats(const char* kind, const SDLGameRendererStats* stats) {
    SDL_Log("[UWP_PERF] cache kind=%s frame=%llu stage=%d area=%d p1=%d p2=%d "
            "bg_x=%d p1_x=%d p2_x=%d hits=%d retained=%d snapshot_invalidations=%d resize_recreates=%d "
            "palette_invalidations=%d pixel_invalidations=%d release_invalidations=%d "
            "palette_unlocks=%d texture_unlocks=%d pixel_updates=%d updated_pixels=%d "
            "pixel_update_ms=%.3f palette_updates=%d palette_update_ms=%.3f "
            "upload_bytes=%llu rgba_fallbacks=%d largest_create=%dx%d",
            kind, (unsigned long long)uwp_render_trace_frame, bg_w.stage, bg_w.area,
            plw[0].player_number, plw[1].player_number, bg_w.bgw[1].wxy[0].disp.pos,
            plw[0].wu.xyz[0].disp.pos, plw[1].wu.xyz[0].disp.pos,
            stats->cache_hits, stats->retained_on_unlock, stats->snapshot_invalidations,
            stats->resize_recreates, stats->cache_palette_invalidations, stats->cache_pixel_invalidations,
            stats->cache_release_invalidations, stats->unlock_palette_calls, stats->unlock_texture_calls,
            stats->indexed_texture_updates, stats->indexed_texture_update_pixels,
            stats->indexed_texture_update_ms, stats->indexed_palette_updates, stats->indexed_palette_update_ms,
            (unsigned long long)stats->upload_bytes, stats->indexed_texture_rgba_fallbacks,
            stats->largest_create_w, stats->largest_create_h);
}

static void record_uwp_frame_perf(const SDLAppFrameTiming* timing, double work_ms) {
    static SDLGameRendererStats cache_window;
#define UWP_ADD_CACHE_COUNTER(field) cache_window.field += timing->render_stats.field
    UWP_ADD_CACHE_COUNTER(cache_hits);
    UWP_ADD_CACHE_COUNTER(retained_on_unlock);
    UWP_ADD_CACHE_COUNTER(snapshot_invalidations);
    UWP_ADD_CACHE_COUNTER(resize_recreates);
    UWP_ADD_CACHE_COUNTER(cache_palette_invalidations);
    UWP_ADD_CACHE_COUNTER(cache_pixel_invalidations);
    UWP_ADD_CACHE_COUNTER(cache_release_invalidations);
    UWP_ADD_CACHE_COUNTER(unlock_palette_calls);
    UWP_ADD_CACHE_COUNTER(unlock_texture_calls);
    UWP_ADD_CACHE_COUNTER(indexed_texture_updates);
    UWP_ADD_CACHE_COUNTER(indexed_texture_update_pixels);
    UWP_ADD_CACHE_COUNTER(indexed_palette_updates);
    UWP_ADD_CACHE_COUNTER(indexed_texture_rgba_fallbacks);
    UWP_ADD_CACHE_COUNTER(upload_bytes);
#undef UWP_ADD_CACHE_COUNTER
    cache_window.indexed_texture_update_ms = SDL_max(cache_window.indexed_texture_update_ms,
                                                    timing->render_stats.indexed_texture_update_ms);
    cache_window.indexed_palette_update_ms = SDL_max(cache_window.indexed_palette_update_ms,
                                                    timing->render_stats.indexed_palette_update_ms);
    if ((Uint64)timing->render_stats.largest_create_w * timing->render_stats.largest_create_h >
        (Uint64)cache_window.largest_create_w * cache_window.largest_create_h) {
        cache_window.largest_create_w = timing->render_stats.largest_create_w;
        cache_window.largest_create_h = timing->render_stats.largest_create_h;
    }
    static unsigned int frames, over_20, over_33, slow_details, texture_creates, texture_failures;
    static unsigned int intervals, intervals_over_20, intervals_over_33;
    static double total_interval_ms;
    static double total_ms, max_frame_ms, max_interval_ms, max_work_ms, max_pre_render_ms;
    static double max_adx_ms, max_geometry_ms, max_copy_ms, max_present_ms, max_cleanup_ms;
    static double max_create_ms, max_sleep_overrun_ms;
    const double frame_ms = (double)(SDL_GetTicksNS() - uwp_frame_start_ns) / 1e6;
    frames++;
    total_ms += frame_ms;
    if (uwp_frame_interval_ms > 0.0) {
        intervals++;
        total_interval_ms += uwp_frame_interval_ms;
        intervals_over_20 += uwp_frame_interval_ms > 20.0;
        intervals_over_33 += uwp_frame_interval_ms > 33.333;
    }
    over_20 += frame_ms > 20.0;
    over_33 += frame_ms > 33.333;
    max_frame_ms = SDL_max(max_frame_ms, frame_ms);
    max_interval_ms = SDL_max(max_interval_ms, uwp_frame_interval_ms);
    max_work_ms = SDL_max(max_work_ms, work_ms);
    max_pre_render_ms = SDL_max(max_pre_render_ms, uwp_pre_render_ms);
    max_adx_ms = SDL_max(max_adx_ms, timing->adx_process_ms);
    max_geometry_ms = SDL_max(max_geometry_ms, timing->game_renderer_render_ms);
    max_copy_ms = SDL_max(max_copy_ms, timing->screen_copy_ms);
    max_present_ms = SDL_max(max_present_ms, timing->present_ms);
    max_cleanup_ms = SDL_max(max_cleanup_ms, timing->cleanup_ms);
    max_create_ms = SDL_max(max_create_ms, timing->render_stats.texture_create_ms);
    max_sleep_overrun_ms = SDL_max(max_sleep_overrun_ms, timing->sleep_overrun_ms);
    texture_creates += timing->render_stats.texture_create_calls;
    texture_failures += timing->render_stats.texture_create_failures;
    if (frame_ms > 20.0 || uwp_frame_interval_ms > 20.0) {
        slow_details++;
        if (slow_details <= 8) {
            SDL_Log("[UWP_PERF] slow frame=%llu G=%u,%u,%u,%u D=%u,%u,%u,%u "
                    "frame_ms=%.3f previous_begin_interval_ms=%.3f work_ms=%.3f pre_render_ms=%.3f "
                    "adx_ms=%.3f geometry_ms=%.3f copy_ms=%.3f present_ms=%.3f cleanup_ms=%.3f "
                    "sleep_ms=%.3f sleep_overrun_ms=%.3f screenshot_ms=%.3f "
                    "tasks=%d creates=%d create_ms=%.3f create_failures=%d audio_queued_bytes=%d audio_files=%d",
                    (unsigned long long)uwp_render_trace_frame,
                    G_No[0], G_No[1], G_No[2], G_No[3], D_No[0], D_No[1], D_No[2], D_No[3],
                    frame_ms, uwp_frame_interval_ms, work_ms, uwp_pre_render_ms,
                    timing->adx_process_ms, timing->game_renderer_render_ms, timing->screen_copy_ms,
                    timing->present_ms, timing->cleanup_ms, timing->sleep_ms, timing->sleep_overrun_ms,
                    timing->screenshot_ms, timing->render_stats.render_tasks,
                    timing->render_stats.texture_create_calls, timing->render_stats.texture_create_ms,
                    timing->render_stats.texture_create_failures, ADX_GetQueuedBytes(), ADX_GetNumFiles());
            log_uwp_cache_stats("slow", &timing->render_stats);
        }
    }
    if (frames == 120) {
        SDL_Log("[UWP_PERF] window end_frame=%llu frames=%u target_ms=%.3f avg_frame_ms=%.3f "
                "max_frame_ms=%.3f over_20ms=%u over_33ms=%u slow_details_omitted=%u "
                "avg_begin_interval_ms=%.3f max_begin_interval_ms=%.3f begin_intervals=%u "
                "begin_over_20ms=%u begin_over_33ms=%u max_work_ms=%.3f max_pre_render_ms=%.3f "
                "max_adx_ms=%.3f max_geometry_ms=%.3f max_copy_ms=%.3f max_present_ms=%.3f "
                "max_cleanup_ms=%.3f max_create_ms=%.3f max_sleep_overrun_ms=%.3f "
                "creates=%u create_failures=%u audio_queued_bytes=%d audio_files=%d legacy_debug_log=%d",
                (unsigned long long)uwp_render_trace_frame, frames, (double)target_frame_time_ns / 1e6,
                total_ms / frames, max_frame_ms, over_20, over_33, slow_details > 8 ? slow_details - 8 : 0,
                intervals ? total_interval_ms / intervals : 0.0, max_interval_ms, intervals,
                intervals_over_20, intervals_over_33, max_work_ms, max_pre_render_ms,
                max_adx_ms, max_geometry_ms, max_copy_ms,
                max_present_ms, max_cleanup_ms, max_create_ms, max_sleep_overrun_ms,
                texture_creates, texture_failures, ADX_GetQueuedBytes(), ADX_GetNumFiles(), DebugLog_IsEnabled());
        if (uwp_render_trace_frame % 600 == 0) {
            log_uwp_cache_stats("window", &cache_window);
        }
        SDL_zero(cache_window);
        frames = over_20 = over_33 = slow_details = texture_creates = texture_failures = 0;
        total_ms = max_frame_ms = max_interval_ms = max_work_ms = max_pre_render_ms = 0.0;
        max_adx_ms = max_geometry_ms = max_copy_ms = max_present_ms = max_cleanup_ms = 0.0;
        max_create_ms = max_sleep_overrun_ms = 0.0;
        intervals = intervals_over_20 = intervals_over_33 = 0;
        total_interval_ms = 0.0;
    }
}

static void log_uwp_game_profile(const char* kind, const DebugFrameTiming* frame,
                                 const DebugStepStats* steps, const DebugTaskStats* tasks) {
    int top_task = 0;
    for (int i = 1; i < DEBUG_TASK_STATS_COUNT; i++) {
        if (tasks->task_ms[i] > tasks->task_ms[top_task]) {
            top_task = i;
        }
    }
    SDL_Log("[UWP_PERF] game kind=%s frame=%llu stage=%d p1=%d p2=%d "
            "total_ms=%.3f poll_ms=%.3f begin_ms=%.3f game0_ms=%.3f end_work_ms=%.3f game1_ms=%.3f sleep_ms=%.3f",
            kind, (unsigned long long)uwp_render_trace_frame, bg_w.stage,
            plw[0].player_number, plw[1].player_number, frame->total_ms, frame->poll_ms,
            frame->begin_ms, frame->game0_ms, frame->end_ms, frame->game1_ms, frame->sleep_ms);
    SDL_Log("[UWP_PERF] steps kind=%s frame=%llu afs_ms=%.3f priority_ms=%.3f "
            "pad_ms=%.3f key_ms=%.3f input_ms=%.3f nj_user_ms=%.3f "
            "seq_before_ms=%.3f draw_ms=%.3f seq_after_ms=%.3f game_main_ms=%.3f "
            "knj_ms=%.3f effects_ms=%.3f flip_ms=%.3f scrn_ms=%.3f "
            "irl_family_ms=%.3f irl_scrn_ms=%.3f bgm_ms=%.3f test_prologue_ms=%.3f",
            kind, (unsigned long long)uwp_render_trace_frame, steps->afs_run_server_ms,
            steps->setup_temp_priority_ms, steps->pad_get_all_ms, steps->key_convert_ms,
            steps->input_copy_ms, steps->nj_user_main_ms, steps->seqs_before_process_ms,
            steps->njdp2d_draw_ms, steps->seqs_after_process_ms, steps->game_main_ms,
            steps->knj_flush_ms, steps->disp_effect_work_ms, steps->fl_flip_ms,
            steps->scrn_renew_ms, steps->irl_family_ms, steps->irl_scrn_ms,
            steps->bgm_server_ms, steps->test_prologue_ms);
    SDL_Log("[UWP_PERF] tasks kind=%s frame=%llu top_task=%d top_ms=%.3f "
            "condition=%d r=%d,%d,%d,%d peak_task_frame=%llu "
            "task_ms=%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f",
            kind, (unsigned long long)uwp_render_trace_frame, top_task, tasks->task_ms[top_task],
            tasks->condition[top_task], tasks->r_no[top_task][0], tasks->r_no[top_task][1],
            tasks->r_no[top_task][2], tasks->r_no[top_task][3], (unsigned long long)tasks->frame,
            tasks->task_ms[0], tasks->task_ms[1], tasks->task_ms[2], tasks->task_ms[3],
            tasks->task_ms[4], tasks->task_ms[5], tasks->task_ms[6], tasks->task_ms[7],
            tasks->task_ms[8], tasks->task_ms[9], tasks->task_ms[10]);
}

void SDLApp_RecordUWPGameTiming(const DebugFrameTiming* frame, const DebugStepStats* steps,
                               const DebugTaskStats* tasks) {
    static DebugFrameTiming window_frame;
    static DebugStepStats window_steps;
    static DebugTaskStats window_tasks;
    static unsigned int frames, slow_frames;
    static double peak_task_ms;
    if (uwp_trace_thread == NULL) {
        /* Keep per-frame sampling bounded while no output worker is running. */
        uwp_combat_count = uwp_combat_dropped = 0;
        uwp_combat_events = uwp_combat_timings = 0;
        return;
    }
    for (unsigned int i = 0; i < uwp_combat_count; i++) {
        SDL_Log("[UWP_COMBAT] frame=%llu kind=%s subject=%d detail=%d detail_hex=0x%08X value=%d at_ns=%llu elapsed_ms=%.3f event=%d",
                (unsigned long long)uwp_render_trace_frame, uwp_combat_details[i].kind,
                uwp_combat_details[i].subject, uwp_combat_details[i].detail,
                (unsigned int)uwp_combat_details[i].detail, uwp_combat_details[i].value,
                (unsigned long long)uwp_combat_details[i].at_ns,
                (double)uwp_combat_details[i].elapsed_ns / 1e6, uwp_combat_details[i].elapsed_ns == 0);
    }
    uwp_combat_count = 0;
    if (uwp_render_trace_frame % 120 == 0 && uwp_combat_dropped != 0) {
        SDL_Log("[UWP_COMBAT] limits frame=%llu omitted=%u event_limit=512 timing_limit=1024 frame_capacity=64",
                (unsigned long long)uwp_render_trace_frame, uwp_combat_dropped);
        uwp_combat_dropped = 0;
    }
    frames++;
#define UWP_FRAME_MAX(field) window_frame.field = SDL_max(window_frame.field, frame->field)
    UWP_FRAME_MAX(total_ms);
    UWP_FRAME_MAX(poll_ms);
    UWP_FRAME_MAX(begin_ms);
    UWP_FRAME_MAX(game0_ms);
    UWP_FRAME_MAX(end_ms);
    UWP_FRAME_MAX(game1_ms);
    UWP_FRAME_MAX(sleep_ms);
#undef UWP_FRAME_MAX
#define UWP_STEP_MAX(field) window_steps.field = SDL_max(window_steps.field, steps->field)
    UWP_STEP_MAX(afs_run_server_ms);
    UWP_STEP_MAX(setup_temp_priority_ms);
    UWP_STEP_MAX(pad_get_all_ms);
    UWP_STEP_MAX(key_convert_ms);
    UWP_STEP_MAX(test_prologue_ms);
    UWP_STEP_MAX(input_copy_ms);
    UWP_STEP_MAX(game_main_ms);
    UWP_STEP_MAX(nj_user_main_ms);
    UWP_STEP_MAX(seqs_before_process_ms);
    UWP_STEP_MAX(njdp2d_draw_ms);
    UWP_STEP_MAX(seqs_after_process_ms);
    UWP_STEP_MAX(knj_flush_ms);
    UWP_STEP_MAX(disp_effect_work_ms);
    UWP_STEP_MAX(fl_flip_ms);
    UWP_STEP_MAX(scrn_renew_ms);
    UWP_STEP_MAX(irl_family_ms);
    UWP_STEP_MAX(irl_scrn_ms);
    UWP_STEP_MAX(bgm_server_ms);
#undef UWP_STEP_MAX
    for (int i = 0; i < DEBUG_TASK_STATS_COUNT; i++) {
        if (tasks->task_ms[i] > window_tasks.task_ms[i]) {
            window_tasks.task_ms[i] = tasks->task_ms[i];
            window_tasks.condition[i] = tasks->condition[i];
            SDL_memcpy(window_tasks.r_no[i], tasks->r_no[i], sizeof(window_tasks.r_no[i]));
        }
        if (tasks->task_ms[i] > peak_task_ms) {
            peak_task_ms = tasks->task_ms[i];
            window_tasks.frame = uwp_render_trace_frame;
        }
    }
    if (frame->total_ms > 20.0 && slow_frames++ < 8) {
        DebugTaskStats current_tasks = *tasks;
        current_tasks.frame = uwp_render_trace_frame;
        log_uwp_game_profile("slow", frame, steps, &current_tasks);
        for (unsigned int i = 0; i < uwp_game_phase_count; i++) {
            if (uwp_game_phases[i].ms >= 1.0) {
                SDL_Log("[UWP_PERF] game_phase frame=%llu name=%s calls=%u elapsed_ms=%.3f",
                        (unsigned long long)uwp_render_trace_frame, uwp_game_phases[i].name,
                        uwp_game_phases[i].calls, uwp_game_phases[i].ms);
            }
        }
    }
    if (frames == 120) {
        if (uwp_render_trace_frame % 600 == 0) {
            log_uwp_game_profile("window", &window_frame, &window_steps, &window_tasks);
        }
        SDL_zero(window_frame);
        SDL_zero(window_steps);
        SDL_zero(window_tasks);
        frames = slow_frames = 0;
        peak_task_ms = 0.0;
    }
}

static void SDLCALL capture_uwp_video_log(void* userdata,
                                          int category,
                                          SDL_LogPriority priority,
                                          const char* message) {
    UWPRenderLogCapture* capture = (UWPRenderLogCapture*)userdata;

    if (capture != NULL &&
        (category == SDL_LOG_CATEGORY_VIDEO || category == SDL_LOG_CATEGORY_RENDER) &&
        priority >= SDL_LOG_PRIORITY_WARN) {
        const size_t used = SDL_strlen(capture->messages);
        if (used < sizeof(capture->messages) - 1) {
            SDL_snprintf(capture->messages + used,
                         sizeof(capture->messages) - used,
                         "[category=%d priority=%d] %s\n",
                         category,
                         (int)priority,
                         message != NULL ? message : "(empty SDL log message)");
        }
    }

    if (capture != NULL && capture->previous_output != NULL) {
        capture->previous_output(capture->previous_userdata, category, priority, message);
    }
}
#else
#define UWP_RENDER_CALL(call) (call)
#endif

static bool pause_requested = false;
static bool has_input_focus = true;
static bool config_initialized = false;
static bool pads_initialized = false;
static bool screenshots_initialized = false;
static bool bezel_initialized = false;
static bool bezel_render_error_logged = false;
static bool scanlines_initialized = false;
static bool scanlines_render_error_logged = false;
static Uint64 last_mouse_motion_time = 0;
static const int mouse_hide_delay_ms = 2000; // 2 seconds

static const char* scale_mode_name() {
    switch (scale_mode) {
    case SCALEMODE_NEAREST:
        return "nearest";

    case SCALEMODE_LINEAR:
        return "linear";

    case SCALEMODE_SOFT_LINEAR:
        return "soft-linear";

    case SCALEMODE_SQUARE_PIXELS:
        return "square-pixels";

    }

    return "unknown";
}

static SDL_ScaleMode screen_texture_scale_mode() {
    switch (scale_mode) {
    case SCALEMODE_LINEAR:
    case SCALEMODE_SOFT_LINEAR:
        return SDL_SCALEMODE_LINEAR;

    case SCALEMODE_NEAREST:
    case SCALEMODE_SQUARE_PIXELS:
        return SDL_SCALEMODE_NEAREST;
    }
}

static bool screen_texture_size(SDL_Point* size) {
    if (!SDL_GetRenderOutputSize(renderer, &size->x, &size->y)) {
        return false;
    }

    if (size->x <= 0 || size->y <= 0) {
        SDL_SetError("Invalid renderer output size: %dx%d", size->x, size->y);
        return false;
    }

    if (scale_mode == SCALEMODE_SOFT_LINEAR) {
        if (size->x > INT_MAX / 2 || size->y > INT_MAX / 2) {
            SDL_SetError("Renderer output is too large for soft linear scaling: %dx%d", size->x, size->y);
            return false;
        }

        size->x *= 2;
        size->y *= 2;
    }

    return true;
}

static bool create_screen_texture() {
    SDL_Point size;

    if (!screen_texture_size(&size)) {
        return false;
    }

    SDL_Texture* new_texture =
        SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB32, SDL_TEXTUREACCESS_TARGET, size.x, size.y);

    if (new_texture == NULL) {
        return false;
    }

    if (!SDL_SetTextureScaleMode(new_texture, screen_texture_scale_mode())) {
        char texture_error[512];
        SDL_strlcpy(texture_error, SDL_GetError(), sizeof(texture_error));
        SDL_DestroyTexture(new_texture);
        SDL_SetError("%s", texture_error);
        return false;
    }

    if (screen_texture != NULL) {
        SDL_DestroyTexture(screen_texture);
    }
    screen_texture = new_texture;
    return true;
}

static void init_scalemode() {
#if defined(SF3_UWP)
    scale_mode = SCALEMODE_NEAREST;
    return;
#endif
    const char* raw_scalemode = Config_GetString(CFG_KEY_SCALEMODE);

    if (raw_scalemode == NULL) {
        return;
    }

    if (SDL_strcmp(raw_scalemode, "nearest") == 0) {
        scale_mode = SCALEMODE_NEAREST;
    } else if (SDL_strcmp(raw_scalemode, "linear") == 0) {
        scale_mode = SCALEMODE_LINEAR;
    } else if (SDL_strcmp(raw_scalemode, "soft-linear") == 0) {
        scale_mode = SCALEMODE_SOFT_LINEAR;
    } else if (SDL_strcmp(raw_scalemode, "square-pixels") == 0) {
        scale_mode = SCALEMODE_SQUARE_PIXELS;
    } else if (SDL_strcmp(raw_scalemode, "integer") == 0 || SDL_strcmp(raw_scalemode, "arcade-integer") == 0) {
        // Compatibility with configurations created by previous releases.
        scale_mode = SCALEMODE_SQUARE_PIXELS;
    }
}

static void init_aspect_ratio() {
#if defined(SF3_UWP)
    stretch_aspect_ratio = false;
    return;
#endif
    const char* raw_aspect_ratio = Config_GetString(CFG_KEY_ASPECT_RATIO);
    stretch_aspect_ratio = raw_aspect_ratio != NULL && SDL_strcmp(raw_aspect_ratio, "stretch") == 0;
}

static void init_frame_timing() {
    const char* raw_frame_timing = Config_GetString(CFG_KEY_FRAME_TIMING);
    ps2_frame_timing = raw_frame_timing != NULL && SDL_strcmp(raw_frame_timing, "ps2") == 0;
    target_frame_rate = ps2_frame_timing ? frame_rate_ps2 : frame_rate_arcade;
    target_frame_time_ns = (Uint64)(1000000000.0 / target_frame_rate);
}

static bool init_window() {
    SDL_WindowFlags window_flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY;

    if (Config_GetBool(CFG_KEY_FULLSCREEN)) {
        window_flags |= SDL_WINDOW_FULLSCREEN;
    }

    int window_width = Config_GetInt(CFG_KEY_WINDOW_WIDTH);

    if (window_width < window_min_width) {
        window_width = window_min_width;
    }

    int window_height = Config_GetInt(CFG_KEY_WINDOW_HEIGHT);

    if (window_height < window_min_height) {
        window_height = window_min_height;
    }

#if defined(SF3_UWP)
    SDL_Log("[UWP_RENDER] saved_presentation window=%dx%d fullscreen=%d scale_mode=%s aspect_ratio=%s "
            "bezel=%d policy_override=1",
            window_width, window_height, Config_GetBool(CFG_KEY_FULLSCREEN),
            Config_GetString(CFG_KEY_SCALEMODE) ? Config_GetString(CFG_KEY_SCALEMODE) : "(default)",
            Config_GetString(CFG_KEY_ASPECT_RATIO) ? Config_GetString(CFG_KEY_ASPECT_RATIO) : "(default)",
            Config_GetBool(CFG_KEY_BEZEL));
    /* Use the console's reported display size, never a saved desktop window size
     * or a hard-coded 1080p target. Fullscreen lets WinRT occupy the whole view. */
    window_flags = SDL_WINDOW_FULLSCREEN | SDL_WINDOW_HIGH_PIXEL_DENSITY;
    const SDL_DisplayMode* console_mode = SDL_GetDesktopDisplayMode(SDL_GetPrimaryDisplay());
    if (console_mode != NULL && console_mode->w > 0 && console_mode->h > 0) {
        window_width = console_mode->w;
        window_height = console_mode->h;
    }
    SDL_zero(uwp_render_log_capture);
    SDL_GetLogOutputFunction(&uwp_render_log_capture.previous_output, &uwp_render_log_capture.previous_userdata);
    SDL_SetLogOutputFunction(capture_uwp_video_log, &uwp_render_log_capture);

    const bool window_and_renderer_created =
        SDL_CreateWindowAndRenderer(app_name, window_width, window_height, window_flags, &window, &renderer);
    if (!window_and_renderer_created) {
        const char* error = SDL_GetError();
        SDL_strlcpy(uwp_window_renderer_error,
                    error != NULL && error[0] != '\0' ? error : "(SDL returned no error details)",
                    sizeof(uwp_window_renderer_error));
    }

    SDL_SetLogOutputFunction(uwp_render_log_capture.previous_output,
                             uwp_render_log_capture.previous_userdata);

    if (!window_and_renderer_created) {
#else
    if (!SDL_CreateWindowAndRenderer(app_name, window_width, window_height, window_flags, &window, &renderer)) {
#endif
        SDL_Log("Couldn't create window/renderer: %s", SDL_GetError());
        return false;
    }

#if defined(SF3_UWP)
    const int requested_vsync = SDL_GetHintBoolean("SF3_UWP_VSYNC", true) ? 1 : 0;
    const bool vsync_set = SDL_SetRenderVSync(renderer, requested_vsync);
    char vsync_error[256];
    SDL_strlcpy(vsync_error, vsync_set ? "none" : SDL_GetError(), sizeof(vsync_error));
    int effective_vsync = -99;
    const bool vsync_query = SDL_GetRenderVSync(renderer, &effective_vsync);
    const SDL_DisplayID display = SDL_GetDisplayForWindow(window);
    const SDL_DisplayMode* mode = SDL_GetCurrentDisplayMode(display);
    SDL_Log("[UWP_RENDER] vsync requested=%d set_ok=%d query_ok=%d effective=%d error=%s "
            "display=%u mode=%dx%d refresh_hz=%.6f refresh_fraction=%d/%d target_fps=%.6f",
            requested_vsync, vsync_set, vsync_query, effective_vsync, vsync_error, (unsigned int)display,
            mode ? mode->w : 0, mode ? mode->h : 0, mode ? mode->refresh_rate : 0.0,
            mode ? mode->refresh_rate_numerator : 0, mode ? mode->refresh_rate_denominator : 0,
            target_frame_rate);
#else
    if (!SDL_SetRenderVSync(renderer, 0)) {
        SDL_Log("Failed to disable renderer VSync: %s", SDL_GetError());
    }
#endif

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

#if defined(SF3_UWP)
    // Present once so UWP can dismiss the splash screen.
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
    SDL_RenderClear(renderer);
    UWP_RENDER_CALL(SDL_RenderPresent(renderer));
#endif
    return true;
}

bool SDLApp_PreInit() {
    SDL_SetAppMetadata(app_name, "0.1", NULL);
    SDL_SetHint(SDL_HINT_VIDEO_WAYLAND_PREFER_LIBDECOR, "1");
    SDL_SetHint(SDL_HINT_NO_SIGNAL_HANDLERS, "1");
#if defined(SF3_UWP)
    if (!SDL_SetHintWithPriority(SDL_HINT_RENDER_DRIVER, "direct3d11", SDL_HINT_OVERRIDE)) {
        critical_error("Couldn't select the Direct3D 11 renderer: %s", SDL_GetError());
        return false;
    }
#endif

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        critical_error("Couldn't initialize SDL video: %s", SDL_GetError());
        return false;
    }

    return true;
}

static void init_audio() {
    if (!SDL_Init(SDL_INIT_AUDIO)) {
        SDL_Log("Couldn't initialize the SDL audio subsystem; the game will continue without audio: %s", SDL_GetError());
    }
}

bool SDLApp_FullInit() {
    Config_Init();
    config_initialized = true;
#if defined(SF3_UWP)
    init_uwp_render_trace();
#endif
    Keymap_Init();
    init_scalemode();
    init_aspect_ratio();
    init_frame_timing();
#if defined(SF3_UWP)
    // Use one provider so a physical Xbox controller cannot occupy both slots
    // through separate XInput and WGI instance IDs.
    const bool wgi_selected = SDL_SetHintWithPriority(SDL_HINT_JOYSTICK_WGI, "1", SDL_HINT_OVERRIDE);
    const bool xinput_disabled = SDL_SetHintWithPriority(SDL_HINT_XINPUT_ENABLED, "0", SDL_HINT_OVERRIDE);
    SDL_Log("[UWP_PAD] providers wgi_selected=%d xinput_disabled=%d", wgi_selected, xinput_disabled);
#endif
    if (!SDL_Init(SDL_INIT_GAMEPAD)) {
        SDL_Log("Couldn't initialize the SDL gamepad subsystem; keyboard input remains available: %s", SDL_GetError());
    }

#if !defined(SF3_UWP)
    init_audio();
#endif

    if (!init_window()) {
#if defined(SF3_UWP)
        const char* diagnostics = uwp_render_log_capture.messages[0] != '\0'
                                      ? uwp_render_log_capture.messages
                                      : "(SDL emitted no video/render warnings during initialization.)";
        log_error("SDL video/render initialization diagnostics:\n%s", diagnostics);
        critical_error("Couldn't initialize the SDL window and renderer: %s", uwp_window_renderer_error);
#else
        critical_error("Couldn't initialize the SDL window and renderer: %s", SDL_GetError());
#endif
        return false;
    }

#if defined(SF3_UWP)
    init_audio();
#endif

    // Initialize rendering subsystems
    if (!SDLMessageRenderer_Initialize(renderer)) {
        critical_error("Couldn't initialize the message renderer: %s", SDL_GetError());
        return false;
    }

    if (!SDLGameRenderer_Init(renderer)) {
        critical_error("Couldn't initialize the game renderer: %s", SDL_GetError());
        return false;
    }

#if DEBUG
    SDLDebugText_Initialize(renderer);
#endif

    // Initialize screen texture
    if (!create_screen_texture()) {
        critical_error("Couldn't create the screen texture: %s", SDL_GetError());
        return false;
    }

#if !defined(SF3_UWP)
    if (Config_GetBool(CFG_KEY_BEZEL)) {
        const char* bezel_image = scale_mode == SCALEMODE_SQUARE_PIXELS ? "bezel-pixel-perfect.png" : "bezel.png";
        bezel_initialized = SDLBezel_Init(renderer, bezel_image);
    }
#endif

    if (Config_GetBool(CFG_KEY_SCANLINES)) {
        scanlines_initialized = SDLScanlines_Init(renderer, Config_GetInt(CFG_KEY_SCANLINE_OPACITY));
    }

    screenshots_initialized = SDLScreenshot_Init();

    if (!screenshots_initialized) {
        log_error("JPEG screenshots are unavailable because the screenshot worker could not be initialized.");
    }

    // Initialize pads
    SDLPad_Init();
    pads_initialized = true;

    return true;
}

void SDLApp_WriteDebugSessionInfo() {
    if (!DebugLog_IsEnabled()) {
        return;
    }

    if (window == NULL || renderer == NULL) {
        return;
    }

    int window_width = 0;
    int window_height = 0;
    int output_width = 0;
    int output_height = 0;

    SDL_GetWindowSize(window, &window_width, &window_height);
    SDL_GetRenderOutputSize(renderer, &output_width, &output_height);

    DebugLog_PrintSession("scale_mode=%s\n", scale_mode_name());
    DebugLog_PrintSession("aspect_ratio=%s\n", stretch_aspect_ratio ? "stretch" : "preserve");
    DebugLog_PrintSession("frame_timing=%s\n", SDLApp_GetFrameTimingName());
    DebugLog_PrintSession("target_frame_rate=%.6f\n", SDLApp_GetTargetFrameRate());
    DebugLog_PrintSession("target_frame_ms=%.3f\n", 1000.0 / SDLApp_GetTargetFrameRate());
    DebugLog_PrintSession("late_frame_threshold_ms=%.3f\n", (1000.0 / SDLApp_GetTargetFrameRate()) + 1.0);
    DebugLog_PrintSession("window_width=%d\n", window_width);
    DebugLog_PrintSession("window_height=%d\n", window_height);
    DebugLog_PrintSession("render_output_width=%d\n", output_width);
    DebugLog_PrintSession("render_output_height=%d\n", output_height);
}

void SDLApp_Quit() {
    if (screenshots_initialized) {
        SDLScreenshot_Quit();
        screenshots_initialized = false;
    }

    if (pads_initialized) {
        SDLPad_Quit();
        pads_initialized = false;
    }

    if (scanlines_initialized) {
        SDLScanlines_Quit();
        scanlines_initialized = false;
    }

    if (bezel_initialized) {
        SDLBezel_Quit();
        bezel_initialized = false;
    }

    if (config_initialized) {
        Config_Destroy();
        config_initialized = false;
    }

    if (screen_texture != NULL) {
        SDL_DestroyTexture(screen_texture);
    }
    screen_texture = NULL;
    SDLMessageRenderer_Quit();
    SDLGameRenderer_Quit();

    if (renderer != NULL) {
        SDL_DestroyRenderer(renderer);
        renderer = NULL;
    }

    if (window != NULL) {
        SDL_DestroyWindow(window);
        window = NULL;
    }

#if defined(SF3_UWP)
    if (uwp_render_trace_file != NULL) {
        SDL_Log("[UWP_RENDER] session_end frames=%llu samples=%u",
                (unsigned long long)uwp_render_trace_frame, uwp_render_trace_samples);
        SDL_SetLogOutputFunction(uwp_trace_previous_output, uwp_trace_previous_userdata);
        SDL_SetAtomicInt(&uwp_trace_stop, 1);
        SDL_SignalSemaphore(uwp_trace_wakeup);
        SDL_WaitThread(uwp_trace_thread, NULL);
        uwp_trace_thread = NULL;
        SDL_CloseIO(uwp_render_trace_file);
        uwp_render_trace_file = NULL;
        SDL_DestroySemaphore(uwp_trace_wakeup);
        uwp_trace_wakeup = NULL;
    }
#endif
    SDL_Quit();
}

static void set_screenshot_flag_if_needed(SDL_KeyboardEvent* event) {
    if ((event->key == SDLK_F12) && event->down && !event->repeat) {
        should_save_screenshot = true;
        SDLPad_SuppressKeyboardScancodeUntilReleased(event->scancode);
    }
}

static void handle_fullscreen_toggle(SDL_KeyboardEvent* event) {
#if defined(SF3_UWP)
    (void)event;
    return;
#endif
    const bool is_alt_enter = (event->key == SDLK_RETURN) && (event->mod & SDL_KMOD_ALT);
    const bool is_f11 = (event->key == SDLK_F11);
    const bool correct_key = (is_alt_enter || is_f11);

    if (!correct_key || !event->down || event->repeat) {
        return;
    }

    SDLPad_SuppressKeyboardScancodeUntilReleased(event->scancode);
    pause_requested = true;

    const SDL_WindowFlags flags = SDL_GetWindowFlags(window);

    if (flags & SDL_WINDOW_FULLSCREEN) {
        SDL_SetWindowFullscreen(window, false);
    } else {
        SDL_SetWindowFullscreen(window, true);
    }
}

static void handle_mouse_motion() {
    last_mouse_motion_time = SDL_GetTicks();
    SDL_ShowCursor();
}

static void hide_cursor_if_needed() {
    const Uint64 now = SDL_GetTicks();

    if ((last_mouse_motion_time > 0) && ((now - last_mouse_motion_time) > mouse_hide_delay_ms)) {
        SDL_HideCursor();
    }
}

bool SDLApp_PollEvents() {
    SDL_Event event;
    bool continue_running = true;

    while (SDL_PollEvent(&event)) {
        switch (event.type) {
        case SDL_EVENT_GAMEPAD_ADDED:
        case SDL_EVENT_GAMEPAD_REMOVED:
            SDLPad_HandleGamepadDeviceEvent(&event.gdevice);
            break;

        case SDL_EVENT_KEY_DOWN:
        case SDL_EVENT_KEY_UP:
            set_screenshot_flag_if_needed(&event.key);
            handle_fullscreen_toggle(&event.key);
            break;

        case SDL_EVENT_MOUSE_MOTION:
            handle_mouse_motion();
            break;

        case SDL_EVENT_WINDOW_RESIZED:
#if defined(SF3_UWP)
            SDL_Log("[UWP_RENDER] window_resized data=%d,%d", event.window.data1, event.window.data2);
#endif
            if (!create_screen_texture()) {
                SDL_Log("Couldn't resize the screen texture; keeping the previous texture: %s", SDL_GetError());
            }
            break;

#if defined(SF3_UWP)
        case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
            SDL_Log("[UWP_RENDER] window_pixel_size_changed data=%d,%d", event.window.data1, event.window.data2);
            UWP_RENDER_CALL(create_screen_texture());
            break;
#endif

        case SDL_EVENT_WINDOW_FOCUS_LOST:
            has_input_focus = false;
            SDLPad_SetInputEnabled(false);
            SDL_ResetKeyboard();
            pause_requested = true;
            break;

        case SDL_EVENT_WINDOW_FOCUS_GAINED:
#if defined(SF3_UWP)
            UWP_RENDER_CALL(SDL_SetWindowFullscreen(window, true));
#endif
            has_input_focus = true;
            SDLPad_SetInputEnabled(true);
            break;

        case SDL_EVENT_QUIT:
            continue_running = false;
            break;
        }
    }

    return continue_running;
}

bool SDLApp_ConsumePauseRequest() {
    const bool requested = pause_requested;
    pause_requested = false;
    return requested;
}

bool SDLApp_HasInputFocus() {
    return has_input_focus;
}

double SDLApp_GetTargetFrameRate() {
    return target_frame_rate;
}

const char* SDLApp_GetFrameTimingName() {
    return ps2_frame_timing ? "ps2" : "arcade";
}

void SDLApp_BeginFrame() {
#if defined(SF3_UWP)
    uwp_game_phase_count = 0;
    uwp_combat_count = 0;
    if (uwp_trace_thread != NULL) {
        uwp_frame_start_ns = SDL_GetTicksNS();
        uwp_frame_interval_ms = uwp_previous_frame_start_ns != 0
            ? (double)(uwp_frame_start_ns - uwp_previous_frame_start_ns) / 1e6 : 0.0;
        uwp_previous_frame_start_ns = uwp_frame_start_ns;
    }
#endif
    // Clear window
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
    UWP_RENDER_CALL(SDL_SetRenderTarget(renderer, NULL));
    UWP_RENDER_CALL(SDL_RenderClear(renderer));

    SDLMessageRenderer_BeginFrame();
    SDLGameRenderer_BeginFrame();
}

static void center_rect(SDL_FRect* rect, int win_w, int win_h) {
    rect->x = (win_w - rect->w) / 2;
    rect->y = (win_h - rect->h) / 2;
}

static SDL_FRect fit_4_by_3_rect(int win_w, int win_h) {
    SDL_FRect rect;
    rect.w = win_w;
    rect.h = win_w / display_target_ratio;

    if (rect.h > win_h) {
        rect.h = win_h;
        rect.w = win_h * display_target_ratio;
    }

    center_rect(&rect, win_w, win_h);
    return rect;
}

static SDL_FRect fit_square_pixels_rect(int win_w, int win_h) {
    /*
     * The 384x224 CPS3 canvas is presented as 4:3 by the original game.
     * A 300x224 presentation grid keeps that intended aspect closely while
     * producing a 1200x896 game area at 1920x1080 for the Capcom bezel.
     */
    const int scale_w = win_w / 300;
    const int scale_h = win_h / 224;
    int scale = (scale_h < scale_w) ? scale_h : scale_w;

    if (scale < 1) {
        scale = 1;
    }

    SDL_FRect rect = {
        .w = scale * 300,
        .h = scale * 224,
    };
    center_rect(&rect, win_w, win_h);
    return rect;
}

static SDL_FRect get_letterbox_rect(int win_w, int win_h) {
#if defined(SF3_UWP)
    const SDL_FRect rect = fit_4_by_3_rect(win_w, win_h);
    static int previous_w, previous_h;
    if (win_w != previous_w || win_h != previous_h) {
        previous_w = win_w;
        previous_h = win_h;
        int pixel_w = 0, pixel_h = 0;
        int logical_w = 0, logical_h = 0;
        int output_w = 0, output_h = 0;
        SDL_GetWindowSizeInPixels(window, &pixel_w, &pixel_h);
        SDL_GetWindowSize(window, &logical_w, &logical_h);
        SDL_GetRenderOutputSize(renderer, &output_w, &output_h);
        SDL_Log("[UWP_RENDER] presentation policy=fullscreen_4_3 bezel=0 fullscreen=%d "
                "window_logical=%dx%d window_pixels=%dx%d output=%dx%d canvas=%dx%d "
                "rect=%.1f,%.1f,%.1f,%.1f",
                (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN) != 0,
                logical_w, logical_h, pixel_w, pixel_h, output_w, output_h,
                win_w, win_h, rect.x, rect.y, rect.w, rect.h);
    }
    return rect;
#endif
    if (stretch_aspect_ratio) {
        return (SDL_FRect){ .x = 0.0f, .y = 0.0f, .w = (float)win_w, .h = (float)win_h };
    }

    switch (scale_mode) {
    case SCALEMODE_NEAREST:
    case SCALEMODE_LINEAR:
    case SCALEMODE_SOFT_LINEAR:
        return fit_4_by_3_rect(win_w, win_h);

    case SCALEMODE_SQUARE_PIXELS:
        return fit_square_pixels_rect(win_w, win_h);
    }
}

static bool output_is_16_by_9(int width, int height) {
    if (width <= 0 || height <= 0) {
        return false;
    }

    Sint64 difference = ((Sint64)width * 9) - ((Sint64)height * 16);

    if (difference < 0) {
        difference = -difference;
    }

    // Accept common near-16:9 modes such as 1366x768 without accepting 16:10.
    return difference <= ((Sint64)height / 10);
}

static bool should_render_bezel() {
    if (!bezel_initialized || stretch_aspect_ratio || window == NULL || renderer == NULL) {
        return false;
    }

    if (!(SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN)) {
        return false;
    }

    int output_width = 0;
    int output_height = 0;

    if (!SDL_GetRenderOutputSize(renderer, &output_width, &output_height)) {
        return false;
    }

    return output_is_16_by_9(output_width, output_height);
}

static bool queue_texture_screenshot(SDL_Texture* texture) {
    SDL_Texture* previous_target = SDL_GetRenderTarget(renderer);

    if (!SDL_SetRenderTarget(renderer, texture)) {
        log_error("Couldn't select the screenshot render target: %s", SDL_GetError());
        return false;
    }

    SDL_Surface* rendered_surface = SDL_RenderReadPixels(renderer, NULL);

    if (rendered_surface == NULL) {
        log_error("Couldn't read the screenshot pixels: %s", SDL_GetError());
    }

    const bool target_restored = SDL_SetRenderTarget(renderer, previous_target);

    if (!target_restored) {
        log_error("Couldn't restore the render target after capturing a screenshot: %s", SDL_GetError());
    }

    if (rendered_surface == NULL) {
        return false;
    }

    if (!target_restored || !SDLScreenshot_Queue(rendered_surface)) {
        SDL_DestroySurface(rendered_surface);
        return false;
    }

    return true;
}

void SDLApp_EndFrame(SDLAppFrameTiming* timing) {
#if defined(SF3_UWP)
    SDLAppFrameTiming uwp_timing;
    if (uwp_trace_thread != NULL) {
        uwp_pre_render_ms = (double)(SDL_GetTicksNS() - uwp_frame_start_ns) / 1e6;
        if (timing == NULL) {
            timing = &uwp_timing;
        }
    }
#endif
    if (timing != NULL) {
        SDL_zero(*timing);
    }

    if (SDLPad_TakeScreenshotRequested()) {
        should_save_screenshot = true;
    }

    // Run sound processing
    const Uint64 adx_start_ns = timing != NULL ? SDL_GetTicksNS() : 0;
    ADX_ProcessTracks();
    if (timing != NULL) {
        timing->adx_process_ms = (double)(SDL_GetTicksNS() - adx_start_ns) / 1e6;
    }
    DebugLog_RecordAdxProcess(
        timing != NULL ? timing->adx_process_ms : 0.0, ADX_GetQueuedBytes(), ADX_GetNumFiles());

    // Render

#if defined(SF3_UWP)
    begin_uwp_render_sample();
#endif

    const Uint64 game_renderer_start_ns = timing != NULL ? SDL_GetTicksNS() : 0;
    SDLGameRenderer_RenderFrame(timing != NULL ? &timing->render_stats : NULL);
#if defined(SF3_UWP)
    trace_uwp_target("game_geometry_queued", false);
#endif
    if (timing != NULL) {
        timing->game_renderer_render_ms = (double)(SDL_GetTicksNS() - game_renderer_start_ns) / 1e6;
    }

    const Uint64 screen_copy_start_ns = timing != NULL ? SDL_GetTicksNS() : 0;
    UWP_RENDER_CALL(SDL_SetRenderTarget(renderer, screen_texture));
#if defined(SF3_UWP)
    trace_uwp_target("screen_composition", false);
#endif

    // Render window background
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255); // black bars
    UWP_RENDER_CALL(SDL_RenderClear(renderer));

    // Render content
    const SDL_FRect dst_rect = get_letterbox_rect(screen_texture->w, screen_texture->h);
#if defined(SF3_UWP)
    if (uwp_render_trace_sample) {
        SDL_Log("[UWP_RENDER] frame=%llu letterbox=%g,%g,%g,%g cps3=%dx%d messages=%dx%d",
                (unsigned long long)uwp_render_trace_frame, dst_rect.x, dst_rect.y, dst_rect.w, dst_rect.h,
                cps3_canvas->w, cps3_canvas->h, message_canvas->w, message_canvas->h);
    }
#endif
    UWP_RENDER_CALL(SDL_RenderTexture(renderer, cps3_canvas, NULL, &dst_rect));

    if (scanlines_initialized && !SDLScanlines_Render(renderer, &dst_rect) && !scanlines_render_error_logged) {
        log_error("Couldn't render the scanline texture: %s", SDL_GetError());
        scanlines_render_error_logged = true;
    }

    UWP_RENDER_CALL(SDL_RenderTexture(renderer, message_canvas, NULL, &dst_rect));

    if (should_render_bezel() && !SDLBezel_Render(renderer, screen_texture->w, screen_texture->h) &&
        !bezel_render_error_logged) {
        log_error("Couldn't render the bezel texture: %s", SDL_GetError());
        bezel_render_error_logged = true;
    }

    // Render screen texture to screen
    UWP_RENDER_CALL(SDL_SetRenderTarget(renderer, NULL));
    UWP_RENDER_CALL(SDL_RenderTexture(renderer, screen_texture, NULL, NULL));
#if defined(SF3_UWP)
    static int previous_presentation_w, previous_presentation_h;
    const bool presentation_size_changed = screen_texture->w != previous_presentation_w ||
                                           screen_texture->h != previous_presentation_h;
    trace_uwp_target("window_presentation", presentation_size_changed);
    previous_presentation_w = screen_texture->w;
    previous_presentation_h = screen_texture->h;
#endif
    if (timing != NULL) {
        timing->screen_copy_ms = (double)(SDL_GetTicksNS() - screen_copy_start_ns) / 1e6;
    }

    const Uint64 screenshot_screen_start_ns = timing != NULL ? SDL_GetTicksNS() : 0;
    if (should_save_screenshot && screenshots_initialized) {
        queue_texture_screenshot(screen_texture);
    }
    if (timing != NULL) {
        timing->screenshot_ms += (double)(SDL_GetTicksNS() - screenshot_screen_start_ns) / 1e6;
    }

#if DEBUG
    const Uint64 debug_text_start_ns = timing != NULL ? SDL_GetTicksNS() : 0;

    // Render debug text
    SDLDebugText_Render();

    // Render metrics
    // int window_width;
    // SDL_GetRenderOutputSize(renderer, &window_width, NULL);
    // SDL_SetRenderDrawColor(renderer, 255, 255, 255, SDL_ALPHA_OPAQUE);
    // SDL_SetRenderScale(renderer, 2, 2);
    // SDL_SetRenderScale(renderer, 1, 1);
    if (timing != NULL) {
        timing->debug_text_ms = (double)(SDL_GetTicksNS() - debug_text_start_ns) / 1e6;
    }
#endif

    const Uint64 present_start_ns = timing != NULL ? SDL_GetTicksNS() : 0;
    UWP_RENDER_CALL(SDL_RenderPresent(renderer));
    if (timing != NULL) {
        timing->present_ms = (double)(SDL_GetTicksNS() - present_start_ns) / 1e6;
    }

    // Cleanup
    const Uint64 cleanup_start_ns = timing != NULL ? SDL_GetTicksNS() : 0;
    SDLGameRenderer_EndFrame();
    should_save_screenshot = false;
    if (timing != NULL) {
        timing->cleanup_ms = (double)(SDL_GetTicksNS() - cleanup_start_ns) / 1e6;
    }

    // Handle cursor hiding
    const Uint64 cursor_start_ns = timing != NULL ? SDL_GetTicksNS() : 0;
    hide_cursor_if_needed();
    if (timing != NULL) {
        timing->cursor_ms = (double)(SDL_GetTicksNS() - cursor_start_ns) / 1e6;
    }

    // Do frame pacing
#if defined(SF3_UWP)
    const double uwp_work_ms = uwp_trace_thread != NULL
        ? (double)(SDL_GetTicksNS() - uwp_frame_start_ns) / 1e6 : 0.0;
#endif
    const Uint64 pacing_start_ns = timing != NULL ? SDL_GetTicksNS() : 0;
    Uint64 now = SDL_GetTicksNS();

    if (frame_deadline == 0) {
        frame_deadline = now + target_frame_time_ns;
    }

    if (now < frame_deadline) {
        Uint64 sleep_time = frame_deadline - now;
        SDL_DelayNS(sleep_time);
        const Uint64 wake_time = SDL_GetTicksNS();

        if (timing != NULL) {
            const double requested_sleep_ms = (double)sleep_time / 1e6;
            timing->sleep_ms = (double)(wake_time - now) / 1e6;
            timing->sleep_overrun_ms = timing->sleep_ms - requested_sleep_ms;

            if (timing->sleep_overrun_ms < 0.0) {
                timing->sleep_overrun_ms = 0.0;
            }
        }

        now = wake_time;
    }

    frame_deadline += target_frame_time_ns;

    // If we fell behind by more than one frame, resync to avoid spiraling
    if (now > frame_deadline + target_frame_time_ns) {
        frame_deadline = now + target_frame_time_ns;
    }

    if (timing != NULL) {
        timing->pacing_ms = (double)(SDL_GetTicksNS() - pacing_start_ns) / 1e6;
        timing->pacing_overhead_ms = timing->pacing_ms - timing->sleep_ms;

        if (timing->pacing_overhead_ms < 0.0) {
            timing->pacing_overhead_ms = 0.0;
        }
    }
#if defined(SF3_UWP)
    if (uwp_trace_thread != NULL) {
        record_uwp_frame_perf(timing, uwp_work_ms);
    }
#endif
}

void SDLApp_Exit() {
    SDL_Event quit_event;
    quit_event.type = SDL_EVENT_QUIT;
    SDL_PushEvent(&quit_event);
}
