#include "port/sdl/sdl_screenshot.h"
#include "port/paths.h"
#include "port/utils.h"

#include <libavcodec/avcodec.h>
#include <libavutil/error.h>
#include <libavutil/frame.h>
#include <libavutil/pixfmt.h>

#define SCREENSHOT_QUEUE_CAPACITY 2

typedef struct ScreenshotJob {
    SDL_Surface* surface;
    char* filename;
} ScreenshotJob;

static SDL_Thread* worker_thread = NULL;
static SDL_Mutex* queue_mutex = NULL;
static SDL_Condition* queue_condition = NULL;
static ScreenshotJob screenshot_queue[SCREENSHOT_QUEUE_CAPACITY];
static int queue_head = 0;
static int queue_count = 0;
static bool stop_requested = false;
static char* prints_path = NULL;

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

static char* make_screenshot_path() {
    SDL_Time current_time;
    SDL_DateTime date_time;

    if (!SDL_GetCurrentTime(&current_time) || !SDL_TimeToDateTime(current_time, &date_time, true)) {
        log_error("Couldn't obtain the current date and time for the screenshot filename: %s", SDL_GetError());
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

    return screenshot_path;
}

static int screenshot_worker(void* userdata) {
    (void)userdata;

    if (!SDL_SetCurrentThreadPriority(SDL_THREAD_PRIORITY_LOW)) {
        SDL_Log("Couldn't lower the screenshot worker priority: %s", SDL_GetError());
    }

    while (true) {
        SDL_LockMutex(queue_mutex);

        while (queue_count == 0 && !stop_requested) {
            SDL_WaitCondition(queue_condition, queue_mutex);
        }

        if (queue_count == 0 && stop_requested) {
            SDL_UnlockMutex(queue_mutex);
            break;
        }

        ScreenshotJob job = screenshot_queue[queue_head];
        SDL_zero(screenshot_queue[queue_head]);
        queue_head = (queue_head + 1) % SCREENSHOT_QUEUE_CAPACITY;
        queue_count -= 1;
        SDL_UnlockMutex(queue_mutex);

        save_surface_as_jpeg(job.surface, job.filename);
        SDL_DestroySurface(job.surface);
        SDL_free(job.filename);
    }

    return 0;
}

bool SDLScreenshot_Init(void) {
    if (worker_thread != NULL) {
        return true;
    }

    const char* base_path = Paths_GetBasePath();

    if (base_path == NULL) {
        log_error("Couldn't locate the game directory for screenshot storage: %s", SDL_GetError());
        return false;
    }

    if (SDL_asprintf(&prints_path, "%sprints", base_path) < 0 || prints_path == NULL) {
        log_error("Couldn't allocate the screenshot directory path.");
        return false;
    }

    if (!SDL_CreateDirectory(prints_path)) {
        log_error("Couldn't create the screenshot directory %s: %s", prints_path, SDL_GetError());
        SDL_free(prints_path);
        prints_path = NULL;
        return false;
    }

    queue_mutex = SDL_CreateMutex();
    queue_condition = SDL_CreateCondition();

    if (queue_mutex == NULL || queue_condition == NULL) {
        log_error("Couldn't initialize the screenshot queue: %s", SDL_GetError());
        SDLScreenshot_Quit();
        return false;
    }

    stop_requested = false;
    queue_head = 0;
    queue_count = 0;
    SDL_zeroa(screenshot_queue);
    worker_thread = SDL_CreateThread(screenshot_worker, "JPEG Screenshot Worker", NULL);

    if (worker_thread == NULL) {
        log_error("Couldn't start the screenshot worker: %s", SDL_GetError());
        SDLScreenshot_Quit();
        return false;
    }

    return true;
}

void SDLScreenshot_Quit(void) {
    if (worker_thread != NULL) {
        SDL_LockMutex(queue_mutex);
        stop_requested = true;
        SDL_SignalCondition(queue_condition);
        SDL_UnlockMutex(queue_mutex);
        SDL_WaitThread(worker_thread, NULL);
        worker_thread = NULL;
    }

    for (int i = 0; i < SCREENSHOT_QUEUE_CAPACITY; i++) {
        SDL_DestroySurface(screenshot_queue[i].surface);
        SDL_free(screenshot_queue[i].filename);
        SDL_zero(screenshot_queue[i]);
    }

    if (queue_condition != NULL) {
        SDL_DestroyCondition(queue_condition);
        queue_condition = NULL;
    }

    if (queue_mutex != NULL) {
        SDL_DestroyMutex(queue_mutex);
        queue_mutex = NULL;
    }
    SDL_free(prints_path);
    prints_path = NULL;
    stop_requested = false;
    queue_head = 0;
    queue_count = 0;
}

bool SDLScreenshot_Queue(SDL_Surface* surface) {
    if (surface == NULL || worker_thread == NULL) {
        return false;
    }

    char* filename = make_screenshot_path();

    if (filename == NULL) {
        return false;
    }

    SDL_LockMutex(queue_mutex);

    if (stop_requested || queue_count == SCREENSHOT_QUEUE_CAPACITY) {
        SDL_UnlockMutex(queue_mutex);
        SDL_free(filename);
        SDL_Log("Screenshot queue is full; the new capture was discarded.");
        return false;
    }

    const int queue_tail = (queue_head + queue_count) % SCREENSHOT_QUEUE_CAPACITY;
    screenshot_queue[queue_tail] = (ScreenshotJob){
        .surface = surface,
        .filename = filename,
    };
    queue_count += 1;
    SDL_SignalCondition(queue_condition);
    SDL_UnlockMutex(queue_mutex);
    return true;
}
