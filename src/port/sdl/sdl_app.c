#include "port/sdl/sdl_app.h"
#include "common.h"
#include "port/config/config.h"
#include "port/config/keymap.h"
#include "port/debug/debug_log.h"
#include "port/paths.h"
#include "port/sdl/sdl_debug_text.h"
#include "port/sdl/sdl_game_renderer.h"
#include "port/sdl/sdl_message_renderer.h"
#include "port/sdl/sdl_pad.h"
#include "port/sound/adx.h"
#include "port/utils.h"
#include "sf33rd/AcrSDK/ps2/foundaps2.h"

#include <SDL3/SDL.h>
#include <libavcodec/avcodec.h>
#include <libavutil/error.h>
#include <libavutil/frame.h>
#include <libavutil/pixfmt.h>
#include <limits.h>

#define FRAME_END_TIMES_MAX 30

typedef enum ScaleMode {
    SCALEMODE_NEAREST,
    SCALEMODE_LINEAR,
    SCALEMODE_SOFT_LINEAR,
    SCALEMODE_SQUARE_PIXELS,
    SCALEMODE_INTEGER,
} ScaleMode;

static const char* app_name = "Street Fighter III: 3rd Strike";
static const float display_target_ratio = 4.0 / 3.0;
static const int window_min_width = 384;
static const int window_min_height = (int)(window_min_width / display_target_ratio);
static const Uint64 target_frame_time_ns = 1000000000.0 / TARGET_FPS;

SDL_Window* window = NULL;
static SDL_Renderer* renderer = NULL;
static SDL_Texture* screen_texture = NULL;
static ScaleMode scale_mode = SCALEMODE_SOFT_LINEAR;

static Uint64 frame_deadline = 0;
static Uint64 frame_end_times[FRAME_END_TIMES_MAX];
static int frame_end_times_index = 0;
static bool frame_end_times_filled = false;
static double fps = 0;
static Uint64 frame_counter = 0;

static bool should_save_screenshot = false;
static bool pause_requested = false;
static bool has_input_focus = true;
static bool config_initialized = false;
static bool pads_initialized = false;
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

    case SCALEMODE_INTEGER:
        return "integer";
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
    case SCALEMODE_INTEGER:
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
    } else if (SDL_strcmp(raw_scalemode, "integer") == 0) {
        scale_mode = SCALEMODE_INTEGER;
    }
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

    if (!SDL_CreateWindowAndRenderer(app_name, window_width, window_height, window_flags, &window, &renderer)) {
        SDL_Log("Couldn't create window/renderer: %s", SDL_GetError());
        return false;
    }

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    return true;
}

bool SDLApp_PreInit() {
    SDL_SetAppMetadata(app_name, "0.1", NULL);
    SDL_SetHint(SDL_HINT_VIDEO_WAYLAND_PREFER_LIBDECOR, "1");
    SDL_SetHint(SDL_HINT_NO_SIGNAL_HANDLERS, "1");

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        critical_error("Couldn't initialize SDL video: %s", SDL_GetError());
        return false;
    }

    return true;
}

bool SDLApp_FullInit() {
    Config_Init();
    config_initialized = true;
    Keymap_Init();
    init_scalemode();

    if (!SDL_Init(SDL_INIT_GAMEPAD)) {
        SDL_Log("Couldn't initialize the SDL gamepad subsystem; keyboard input remains available: %s", SDL_GetError());
    }

    if (!SDL_Init(SDL_INIT_AUDIO)) {
        SDL_Log("Couldn't initialize the SDL audio subsystem; the game will continue without audio: %s", SDL_GetError());
    }

    if (!init_window()) {
        critical_error("Couldn't initialize the SDL window and renderer: %s", SDL_GetError());
        return false;
    }

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
    DebugLog_PrintSession("window_width=%d\n", window_width);
    DebugLog_PrintSession("window_height=%d\n", window_height);
    DebugLog_PrintSession("render_output_width=%d\n", output_width);
    DebugLog_PrintSession("render_output_height=%d\n", output_height);
}

void SDLApp_Quit() {
    if (pads_initialized) {
        SDLPad_Quit();
        pads_initialized = false;
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

    SDL_Quit();
}

static void set_screenshot_flag_if_needed(SDL_KeyboardEvent* event) {
    if ((event->key == SDLK_F12) && event->down && !event->repeat) {
        should_save_screenshot = true;
        SDLPad_SuppressKeyboardScancodeUntilReleased(event->scancode);
    }
}

static void handle_fullscreen_toggle(SDL_KeyboardEvent* event) {
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
            if (!create_screen_texture()) {
                SDL_Log("Couldn't resize the screen texture; keeping the previous texture: %s", SDL_GetError());
            }
            break;

        case SDL_EVENT_WINDOW_FOCUS_LOST:
            has_input_focus = false;
            SDLPad_SetInputEnabled(false);
            SDL_ResetKeyboard();
            pause_requested = true;
            break;

        case SDL_EVENT_WINDOW_FOCUS_GAINED:
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

void SDLApp_BeginFrame() {
    // Clear window
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
    SDL_SetRenderTarget(renderer, NULL);
    SDL_RenderClear(renderer);

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

static SDL_FRect fit_integer_rect(int win_w, int win_h, int pixel_w, int pixel_h) {
    const int virtual_w = win_w / pixel_w;
    const int virtual_h = win_h / pixel_h;
    const int scale_w = virtual_w / 384;
    const int scale_h = virtual_h / 224;
    int scale = (scale_h < scale_w) ? scale_h : scale_w;

    // Better to show a cropped image than nothing at all
    if (scale < 1) {
        scale = 1;
    }

    SDL_FRect rect;
    rect.w = scale * 384 * pixel_w;
    rect.h = scale * 224 * pixel_h;
    center_rect(&rect, win_w, win_h);
    return rect;
}

static SDL_FRect get_letterbox_rect(int win_w, int win_h) {
    switch (scale_mode) {
    case SCALEMODE_NEAREST:
    case SCALEMODE_LINEAR:
    case SCALEMODE_SOFT_LINEAR:
        return fit_4_by_3_rect(win_w, win_h);

    case SCALEMODE_INTEGER:
        // In order to scale a 384x224 buffer to 4:3 we need to stretch the image vertically by 9 / 7
        return fit_integer_rect(win_w, win_h, 7, 9);

    case SCALEMODE_SQUARE_PIXELS:
        return fit_integer_rect(win_w, win_h, 1, 1);
    }
}

static void note_frame_end_time() {
    frame_end_times[frame_end_times_index] = SDL_GetTicksNS();
    frame_end_times_index += 1;
    frame_end_times_index %= FRAME_END_TIMES_MAX;

    if (frame_end_times_index == 0) {
        frame_end_times_filled = true;
    }
}

static void update_fps() {
    if (!frame_end_times_filled) {
        return;
    }

    double total_frame_time_ms = 0;

    for (int i = 0; i < FRAME_END_TIMES_MAX - 1; i++) {
        const int cur = (frame_end_times_index + i) % FRAME_END_TIMES_MAX;
        const int next = (cur + 1) % FRAME_END_TIMES_MAX;
        total_frame_time_ms += (double)(frame_end_times[next] - frame_end_times[cur]) / 1e6;
    }

    double average_frame_time_ms = total_frame_time_ms / (FRAME_END_TIMES_MAX - 1);
    fps = 1000 / average_frame_time_ms;
}

static Uint8 clamp_color_component(int value) {
    if (value < 0) {
        return 0;
    }

    if (value > 255) {
        return 255;
    }

    return (Uint8)value;
}

static void log_jpeg_error(const char* operation, const char* filename, int error_code) {
    char error_text[AV_ERROR_MAX_STRING_SIZE];
    av_strerror(error_code, error_text, sizeof(error_text));
    log_error("Couldn't %s for screenshot %s: %s", operation, filename, error_text);
}

static bool save_surface_as_jpeg(SDL_Surface* surface, const char* filename) {
    bool saved = false;
    bool file_created = false;
    SDL_Surface* rgb_surface = SDL_ConvertSurface(surface, SDL_PIXELFORMAT_RGB24);
    AVCodecContext* codec_context = NULL;
    AVFrame* frame = NULL;
    AVPacket* packet = NULL;
    SDL_IOStream* output = NULL;

    if (rgb_surface == NULL) {
        log_error("Couldn't convert screenshot %s to RGB: %s", filename, SDL_GetError());
        goto cleanup;
    }

    const AVCodec* codec = avcodec_find_encoder(AV_CODEC_ID_MJPEG);

    if (codec == NULL) {
        log_error("Couldn't find the MJPEG encoder for screenshot %s.", filename);
        goto cleanup;
    }

    codec_context = avcodec_alloc_context3(codec);

    if (codec_context == NULL) {
        log_error("Couldn't allocate the JPEG encoder for screenshot %s.", filename);
        goto cleanup;
    }

    codec_context->width = rgb_surface->w;
    codec_context->height = rgb_surface->h;
    codec_context->pix_fmt = AV_PIX_FMT_YUVJ444P;
    codec_context->time_base = (AVRational){1, 1};
    codec_context->color_range = AVCOL_RANGE_JPEG;
    codec_context->colorspace = AVCOL_SPC_BT470BG;
    codec_context->flags |= AV_CODEC_FLAG_QSCALE;
    codec_context->global_quality = 2 * FF_QP2LAMBDA;

    int result = avcodec_open2(codec_context, codec, NULL);

    if (result < 0) {
        log_jpeg_error("initialize the JPEG encoder", filename, result);
        goto cleanup;
    }

    frame = av_frame_alloc();

    if (frame == NULL) {
        log_error("Couldn't allocate a JPEG frame for screenshot %s.", filename);
        goto cleanup;
    }

    frame->format = codec_context->pix_fmt;
    frame->width = codec_context->width;
    frame->height = codec_context->height;
    frame->color_range = codec_context->color_range;
    frame->colorspace = codec_context->colorspace;
    frame->quality = codec_context->global_quality;

    result = av_frame_get_buffer(frame, 32);

    if (result < 0) {
        log_jpeg_error("allocate the JPEG pixel buffer", filename, result);
        goto cleanup;
    }

    for (int y = 0; y < rgb_surface->h; y++) {
        const Uint8* source_row = (const Uint8*)rgb_surface->pixels + (y * rgb_surface->pitch);
        Uint8* y_row = frame->data[0] + (y * frame->linesize[0]);
        Uint8* u_row = frame->data[1] + (y * frame->linesize[1]);
        Uint8* v_row = frame->data[2] + (y * frame->linesize[2]);

        for (int x = 0; x < rgb_surface->w; x++) {
            const int red = source_row[(x * 3) + 0];
            const int green = source_row[(x * 3) + 1];
            const int blue = source_row[(x * 3) + 2];

            y_row[x] = clamp_color_component((77 * red + 150 * green + 29 * blue + 128) >> 8);
            u_row[x] = clamp_color_component(((-43 * red - 85 * green + 128 * blue + 128) >> 8) + 128);
            v_row[x] = clamp_color_component(((128 * red - 107 * green - 21 * blue + 128) >> 8) + 128);
        }
    }

    packet = av_packet_alloc();

    if (packet == NULL) {
        log_error("Couldn't allocate the encoded JPEG data for screenshot %s.", filename);
        goto cleanup;
    }

    result = avcodec_send_frame(codec_context, frame);

    if (result < 0) {
        log_jpeg_error("encode the JPEG frame", filename, result);
        goto cleanup;
    }

    result = avcodec_receive_packet(codec_context, packet);

    if (result < 0) {
        log_jpeg_error("receive the encoded JPEG data", filename, result);
        goto cleanup;
    }

    output = SDL_IOFromFile(filename, "wb");

    if (output == NULL) {
        log_error("Couldn't open screenshot %s for writing: %s", filename, SDL_GetError());
        goto cleanup;
    }

    file_created = true;
    const size_t packet_size = (size_t)packet->size;

    if (SDL_WriteIO(output, packet->data, packet_size) != packet_size) {
        log_error("Couldn't write screenshot %s: %s", filename, SDL_GetError());
        goto cleanup;
    }

    if (!SDL_CloseIO(output)) {
        output = NULL;
        log_error("Couldn't finish writing screenshot %s: %s", filename, SDL_GetError());
        goto cleanup;
    }

    output = NULL;
    saved = true;

cleanup:
    if (output != NULL && !SDL_CloseIO(output)) {
        log_error("Couldn't close the failed screenshot %s: %s", filename, SDL_GetError());
    }

    if (!saved && file_created && !SDL_RemovePath(filename)) {
        log_error("Couldn't remove the incomplete screenshot %s: %s", filename, SDL_GetError());
    }

    av_packet_free(&packet);
    av_frame_free(&frame);
    avcodec_free_context(&codec_context);
    SDL_DestroySurface(rgb_surface);
    return saved;
}

static bool save_texture_as_jpeg(SDL_Texture* texture, const char* filename) {
    SDL_Texture* previous_target = SDL_GetRenderTarget(renderer);

    if (!SDL_SetRenderTarget(renderer, texture)) {
        log_error("Couldn't select the screenshot render target for %s: %s", filename, SDL_GetError());
        return false;
    }

    SDL_Surface* rendered_surface = SDL_RenderReadPixels(renderer, NULL);

    if (rendered_surface == NULL) {
        log_error("Couldn't read the screenshot pixels for %s: %s", filename, SDL_GetError());
        if (!SDL_SetRenderTarget(renderer, previous_target)) {
            log_error("Couldn't restore the render target after a failed screenshot: %s", SDL_GetError());
        }
        return false;
    }

    const bool saved = save_surface_as_jpeg(rendered_surface, filename);
    SDL_DestroySurface(rendered_surface);
    const bool target_restored = SDL_SetRenderTarget(renderer, previous_target);

    if (!target_restored) {
        log_error("Couldn't restore the render target after saving screenshot %s: %s", filename, SDL_GetError());
    }

    if (!saved) {
        log_error("Couldn't save screenshot %s: %s", filename, SDL_GetError());
    }

    return saved && target_restored;
}

static char* make_screenshot_path() {
    const char* base_path = Paths_GetBasePath();

    if (base_path == NULL) {
        log_error("Couldn't locate the game directory for screenshot storage: %s", SDL_GetError());
        return NULL;
    }

    char* prints_path = NULL;

    if (SDL_asprintf(&prints_path, "%sprints", base_path) < 0 || prints_path == NULL) {
        log_error("Couldn't allocate the screenshot directory path.");
        return NULL;
    }

    if (!SDL_CreateDirectory(prints_path)) {
        log_error("Couldn't create the screenshot directory %s: %s", prints_path, SDL_GetError());
        SDL_free(prints_path);
        return NULL;
    }

    SDL_Time current_time;
    SDL_DateTime date_time;

    if (!SDL_GetCurrentTime(&current_time) || !SDL_TimeToDateTime(current_time, &date_time, true)) {
        log_error("Couldn't obtain the current date and time for the screenshot filename: %s", SDL_GetError());
        SDL_free(prints_path);
        return NULL;
    }

    char* screenshot_path = NULL;
    const int milliseconds = date_time.nanosecond / 1000000;

    if (SDL_asprintf(&screenshot_path,
                     "%s/sf3_%04d-%02d-%02d_%02d-%02d-%02d-%03d.jpg",
                     prints_path,
                     date_time.year,
                     date_time.month,
                     date_time.day,
                     date_time.hour,
                     date_time.minute,
                     date_time.second,
                     milliseconds) < 0 ||
        screenshot_path == NULL) {
        log_error("Couldn't allocate the screenshot filename.");
    }

    SDL_free(prints_path);
    return screenshot_path;
}

void SDLApp_EndFrame(SDLAppFrameTiming* timing) {
    if (timing != NULL) {
        SDL_zero(*timing);
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

    const Uint64 game_renderer_start_ns = timing != NULL ? SDL_GetTicksNS() : 0;
    SDLGameRenderer_RenderFrame(timing != NULL ? &timing->render_stats : NULL);
    if (timing != NULL) {
        timing->game_renderer_render_ms = (double)(SDL_GetTicksNS() - game_renderer_start_ns) / 1e6;
    }

    const Uint64 screen_copy_start_ns = timing != NULL ? SDL_GetTicksNS() : 0;
    SDL_SetRenderTarget(renderer, screen_texture);

    // Render window background
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255); // black bars
    SDL_RenderClear(renderer);

    // Render content
    const SDL_FRect dst_rect = get_letterbox_rect(screen_texture->w, screen_texture->h);
    SDL_RenderTexture(renderer, cps3_canvas, NULL, &dst_rect);
    SDL_RenderTexture(renderer, message_canvas, NULL, &dst_rect);

    // Render screen texture to screen
    SDL_SetRenderTarget(renderer, NULL);
    SDL_RenderTexture(renderer, screen_texture, NULL, NULL);
    if (timing != NULL) {
        timing->screen_copy_ms = (double)(SDL_GetTicksNS() - screen_copy_start_ns) / 1e6;
    }

    const Uint64 screenshot_screen_start_ns = timing != NULL ? SDL_GetTicksNS() : 0;
    if (should_save_screenshot) {
        char* screenshot_path = make_screenshot_path();

        if (screenshot_path != NULL) {
            save_texture_as_jpeg(screen_texture, screenshot_path);
            SDL_free(screenshot_path);
        }
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
    // SDL_RenderDebugTextFormat(renderer, (window_width / 2) - 88, 2, "FPS: %.3f", fps);
    // SDL_SetRenderScale(renderer, 1, 1);
    if (timing != NULL) {
        timing->debug_text_ms = (double)(SDL_GetTicksNS() - debug_text_start_ns) / 1e6;
    }
#endif

    const Uint64 present_start_ns = timing != NULL ? SDL_GetTicksNS() : 0;
    SDL_RenderPresent(renderer);
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

    // Measure
    frame_counter += 1;
    note_frame_end_time();
    update_fps();
    if (timing != NULL) {
        timing->pacing_ms = (double)(SDL_GetTicksNS() - pacing_start_ns) / 1e6;
        timing->pacing_overhead_ms = timing->pacing_ms - timing->sleep_ms;

        if (timing->pacing_overhead_ms < 0.0) {
            timing->pacing_overhead_ms = 0.0;
        }
    }
}

void SDLApp_Exit() {
    SDL_Event quit_event;
    quit_event.type = SDL_EVENT_QUIT;
    SDL_PushEvent(&quit_event);
}
