#include "port/sound/adx.h"
#include "port/debug/debug_log.h"
#include "port/io/afs.h"
#include "port/utils.h"
#include "sf33rd/Source/Game/io/gd3rd.h"

#include <SDL3/SDL.h>

#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libavutil/frame.h>
#include <libavutil/intreadwrite.h>
#include <libswresample/swresample.h>

#include <math.h>
#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#define SAMPLE_RATE 48000
#define N_CHANNELS 2
#define BYTES_PER_SAMPLE 2
#define MIN_QUEUED_DATA_MS 400
#define MIN_QUEUED_DATA (int)((float)SAMPLE_RATE * MIN_QUEUED_DATA_MS / 1000 * N_CHANNELS * BYTES_PER_SAMPLE)
#define TRACKS_MAX 10
#define RETIRED_BUFFERS_MAX 100

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

typedef struct ADXDecoderPipeline {
    AVCodecContext* context;
    AVCodecParserContext* parser_context;
    SwrContext* swr;
    AVPacket* packet;
    AVFrame* frame;
} ADXDecoderPipeline;

typedef struct ADXLoopInfo {
    bool looping_enabled;
    int start_sample;
    int end_sample;
    uint8_t* data;
    int data_size;
    int position;
} ADXLoopInfo;

typedef struct ADXTrack {
    bool failed;
    int size;
    size_t data_capacity;
    uint8_t* data;
    bool should_free_data_after_use;
    int used_bytes;
    int processed_samples;
    ADXLoopInfo loop_info;
    ADXDecoderPipeline pipeline;
} ADXTrack;

typedef struct ADXPendingTrack {
    bool initialized;
    int file_id;
    int size;
    size_t data_capacity;
    int sectors;
    uint8_t* data;
    AFSHandle handle;
    bool looping_allowed;
    Uint64 request_start_ns;
} ADXPendingTrack;

typedef struct ADXBufferPoolItem {
    uint8_t* data;
    size_t capacity;
} ADXBufferPoolItem;

typedef struct ADXRetiredBuffer {
    AFSHandle handle;
    uint8_t* data;
    size_t capacity;
} ADXRetiredBuffer;

static SDL_AudioStream* stream = NULL;
static ADXTrack tracks[TRACKS_MAX] = { 0 };
static ADXPendingTrack pending_tracks[TRACKS_MAX] = { 0 };
static ADXBufferPoolItem buffer_pool[TRACKS_MAX] = { 0 };
static ADXRetiredBuffer retired_buffers[RETIRED_BUFFERS_MAX] = { 0 };
static int num_tracks = 0;
static int first_track_index = 0;
static int num_pending_tracks = 0;
static int first_pending_track_index = 0;
static int buffer_pool_count = 0;
static int retired_buffer_count = 0;
static bool has_tracks = false;

static void pipeline_destroy(ADXDecoderPipeline* pipeline);
static void print_av_error(int errnum);

static int stream_data_needed() {
    return MIN_QUEUED_DATA - SDL_GetAudioStreamQueued(stream);
}

static bool stream_needs_data() {
    return stream_data_needed() > 0;
}

static bool stream_is_empty() {
    return SDL_GetAudioStreamQueued(stream) <= 0;
}

int ADX_GetQueuedBytes() {
    if (stream == NULL) {
        return 0;
    }

    return SDL_GetAudioStreamQueued(stream);
}

static bool pipeline_init(ADXDecoderPipeline* pipeline) {
    SDL_zerop(pipeline);
    const AVCodec* codec = avcodec_find_decoder(AV_CODEC_ID_ADPCM_ADX);

    if (codec == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_AUDIO, "ADX decoder is unavailable in FFmpeg.");
        return false;
    }

    pipeline->context = avcodec_alloc_context3(codec);

    if (pipeline->context == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_AUDIO, "Couldn't allocate the ADX decoder context.");
        return false;
    }

    int result = avcodec_open2(pipeline->context, codec, NULL);
    if (result < 0) {
        print_av_error(result);
        pipeline_destroy(pipeline);
        return false;
    }

    pipeline->parser_context = av_parser_init(codec->id);

    if (pipeline->parser_context == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_AUDIO, "Couldn't allocate the ADX parser.");
        pipeline_destroy(pipeline);
        return false;
    }

    const AVChannelLayout ch_layout = AV_CHANNEL_LAYOUT_STEREO;
    result = swr_alloc_set_opts2(&pipeline->swr,
                                 &ch_layout,
                                 AV_SAMPLE_FMT_S16,
                                 SAMPLE_RATE,
                                 &ch_layout,
                                 AV_SAMPLE_FMT_S16P,
                                 SAMPLE_RATE,
                                 0,
                                 NULL);
    if (result < 0 || pipeline->swr == NULL) {
        print_av_error(result);
        pipeline_destroy(pipeline);
        return false;
    }

    result = swr_init(pipeline->swr);
    if (result < 0) {
        print_av_error(result);
        pipeline_destroy(pipeline);
        return false;
    }

    pipeline->packet = av_packet_alloc();
    pipeline->frame = av_frame_alloc();

    if (pipeline->packet == NULL || pipeline->frame == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_AUDIO, "Couldn't allocate ADX decoder buffers.");
        pipeline_destroy(pipeline);
        return false;
    }

    return true;
}

static void pipeline_destroy(ADXDecoderPipeline* pipeline) {
    av_packet_free(&pipeline->packet);
    av_frame_free(&pipeline->frame);
    swr_free(&pipeline->swr);
    avcodec_free_context(&pipeline->context);
    if (pipeline->parser_context != NULL) {
        av_parser_close(pipeline->parser_context);
        pipeline->parser_context = NULL;
    }
}

static uint8_t* acquire_buffer(size_t size, size_t* capacity) {
    int best_index = -1;
    size_t best_capacity = 0;

    for (int i = 0; i < buffer_pool_count; i++) {
        const size_t candidate_capacity = buffer_pool[i].capacity;

        if (candidate_capacity < size) {
            continue;
        }

        if (best_index == -1 || candidate_capacity < best_capacity) {
            best_index = i;
            best_capacity = candidate_capacity;
        }
    }

    if (best_index >= 0) {
        uint8_t* data = buffer_pool[best_index].data;
        *capacity = buffer_pool[best_index].capacity;

        for (int i = best_index; i < buffer_pool_count - 1; i++) {
            buffer_pool[i] = buffer_pool[i + 1];
        }

        buffer_pool_count -= 1;
        SDL_zero(buffer_pool[buffer_pool_count]);
        return data;
    }

    *capacity = size;
    return malloc(size);
}

static void release_buffer(uint8_t* data, size_t capacity) {
    if (data == NULL) {
        return;
    }

    if (buffer_pool_count >= SDL_arraysize(buffer_pool)) {
        free(data);
        return;
    }

    buffer_pool[buffer_pool_count].data = data;
    buffer_pool[buffer_pool_count].capacity = capacity;
    buffer_pool_count += 1;
}

static void retire_buffer(AFSHandle handle, uint8_t* data, size_t capacity) {
    if (data == NULL) {
        return;
    }

    if (!AFS_IsOpen(handle)) {
        release_buffer(data, capacity);
        return;
    }

    if (retired_buffer_count >= SDL_arraysize(retired_buffers)) {
        SDL_LogError(SDL_LOG_CATEGORY_AUDIO, "ADX retired buffer queue is full; preserving the buffer until process exit.");
        return;
    }

    retired_buffers[retired_buffer_count].handle = handle;
    retired_buffers[retired_buffer_count].data = data;
    retired_buffers[retired_buffer_count].capacity = capacity;
    retired_buffer_count += 1;
}

static void release_retired_buffers(bool wait_for_all) {
    const Uint64 timeout_at = SDL_GetTicks() + 10000;

    do {
        AFS_RunServer();

        for (int i = 0; i < retired_buffer_count;) {
            ADXRetiredBuffer* retired = &retired_buffers[i];

            if (AFS_IsOpen(retired->handle)) {
                i += 1;
                continue;
            }

            release_buffer(retired->data, retired->capacity);
            retired_buffers[i] = retired_buffers[retired_buffer_count - 1];
            retired_buffer_count -= 1;
            SDL_zero(retired_buffers[retired_buffer_count]);
        }

        if (!wait_for_all || retired_buffer_count == 0) {
            break;
        }

        SDL_Delay(1);
    } while (SDL_GetTicks() < timeout_at);

    if (wait_for_all && retired_buffer_count > 0) {
        SDL_LogError(SDL_LOG_CATEGORY_AUDIO,
                     "Timed out waiting for %d canceled ADX reads during shutdown.",
                     retired_buffer_count);
    }
}

static void free_buffer_pool() {
    for (int i = 0; i < buffer_pool_count; i++) {
        free(buffer_pool[i].data);
    }

    buffer_pool_count = 0;
    SDL_zeroa(buffer_pool);
}

static void* load_file(int file_id, int* size) {
    const Uint64 load_start_ns = DebugLog_IsEnabled() ? SDL_GetTicksNS() : 0;
    // FIXME: Remove dependency on GD3rd.h
    const unsigned int file_size = fsGetFileSize(file_id);
    *size = file_size;

    if (file_size == 0) {
        SDL_LogError(SDL_LOG_CATEGORY_AUDIO, "ADX resource %d is empty or unavailable.", file_id);
        return NULL;
    }

    const size_t buff_size = (file_size + 2048 - 1) & ~(2048 - 1); // AFS reads data in 2048-byte chunks
    void* buff = malloc(buff_size);
    const int sectors = fsCalSectorSize(file_size);

    if (buff == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_AUDIO, "Couldn't allocate %zu bytes for ADX resource %d.", buff_size, file_id);
        return NULL;
    }

    AFSHandle handle = AFS_Open(file_id);

    if (handle == AFS_NONE) {
        free(buff);
        return NULL;
    }

    const Uint64 afs_start_ns = DebugLog_IsEnabled() ? SDL_GetTicksNS() : 0;
    AFS_ReadSync(handle, sectors, buff);
    const AFSReadState read_state = AFS_GetState(handle);
    if (DebugLog_IsEnabled()) {
        DebugLog_RecordAudioAfsSyncRead(
            file_id, sectors, (uint32_t)(sectors * 2048), (double)(SDL_GetTicksNS() - afs_start_ns) / 1e6);
    }
    AFS_Close(handle);

    if (read_state != AFS_READ_STATE_FINISHED) {
        SDL_LogError(SDL_LOG_CATEGORY_AUDIO, "Couldn't read ADX resource %d from the AFS archive.", file_id);
        free(buff);
        *size = 0;
        return NULL;
    }

    if (DebugLog_IsEnabled()) {
        DebugLog_RecordAdxLoadFile(file_id, file_size, (double)(SDL_GetTicksNS() - load_start_ns) / 1e6);
    }

    return buff;
}

static void print_av_error(int errnum) {
    char errbuf[AV_ERROR_MAX_STRING_SIZE] = { 0 };
    av_strerror(errnum, errbuf, sizeof(errbuf));
    fprintf(stderr, "FFmpeg error: %s\n", errbuf);
}

static bool track_reached_eof(ADXTrack* track) {
    return (track->size - track->used_bytes) <= 0;
}

static bool track_loop_filled(ADXTrack* track) {
    if (track->loop_info.looping_enabled) {
        return track->processed_samples >= track->loop_info.end_sample;
    } else {
        return false;
    }
}

static bool track_needs_decoding(ADXTrack* track) {
    if (track->loop_info.looping_enabled) {
        return !track_loop_filled(track);
    } else {
        return !track_reached_eof(track);
    }
}

static bool track_exhausted(ADXTrack* track) {
    if (track->failed) {
        return true;
    }

    if (track->loop_info.looping_enabled) {
        return false; // Track is never exhausted, because it can be looped infinitely
    } else {
        return track_reached_eof(track);
    }
}

static int track_add_samples_to_loop(ADXTrack* track, uint8_t* buf, int num_samples) {
    ADXLoopInfo* loop_info = &track->loop_info;

    if (!loop_info->looping_enabled) {
        return 0; // No need to add samples if looping is not enabled
    }

    const int buf_sample_start = MAX(loop_info->start_sample - track->processed_samples, 0);
    const int buf_sample_end = MIN(loop_info->end_sample - track->processed_samples, num_samples);

    if (buf_sample_end > buf_sample_start) {
        const int buf_start = buf_sample_start * N_CHANNELS * BYTES_PER_SAMPLE;
        const int buf_end = buf_sample_end * N_CHANNELS * BYTES_PER_SAMPLE;
        const int buf_len = buf_end - buf_start;
        memcpy(loop_info->data + loop_info->position, buf + buf_start, buf_len);
        loop_info->position += buf_len;

        if (loop_info->position == loop_info->data_size) {
            loop_info->position = 0;
        }
    }

    const int overflow = MAX(track->processed_samples + num_samples - loop_info->end_sample, 0);
    track->processed_samples += num_samples;
    return overflow;
}

static bool loop_info_init(ADXLoopInfo* info, const uint8_t* data, size_t data_size) {
    if (data == NULL || data_size < 0x34) {
        return false;
    }

    const uint8_t version = data[0x12];

    switch (version) {
    case 3:
        const Uint16 loop_enabled_16 = AV_RB16(data + 0x16);

        if (loop_enabled_16 == 1) {
            const Uint32 start_sample = AV_RB32(data + 0x1C);
            const Uint32 end_sample = AV_RB32(data + 0x24);

            if (start_sample > (Uint32)INT_MAX || end_sample > (Uint32)INT_MAX) {
                return false;
            }

            info->looping_enabled = true;
            info->start_sample = (int)start_sample;
            info->end_sample = (int)end_sample;
        }

        break;

    case 4:
        const Uint32 loop_enabled_32 = AV_RB32(data + 0x24);

        if (loop_enabled_32 == 1) {
            const Uint32 start_sample = AV_RB32(data + 0x28);
            const Uint32 end_sample = AV_RB32(data + 0x30);

            if (start_sample > (Uint32)INT_MAX || end_sample > (Uint32)INT_MAX) {
                return false;
            }

            info->looping_enabled = true;
            info->start_sample = (int)start_sample;
            info->end_sample = (int)end_sample;
        }

        break;

    default:
        SDL_LogError(SDL_LOG_CATEGORY_AUDIO, "Unsupported ADX version: %d.", version);
        return false;
    }

    if (info->looping_enabled) {
        if (info->end_sample <= info->start_sample ||
            (info->end_sample - info->start_sample) > (INT_MAX / (BYTES_PER_SAMPLE * N_CHANNELS))) {
            SDL_LogError(SDL_LOG_CATEGORY_AUDIO, "Invalid ADX loop boundaries.");
            return false;
        }

        info->data_size = (info->end_sample - info->start_sample) * BYTES_PER_SAMPLE * N_CHANNELS;
        info->data = malloc(info->data_size);

        if (info->data == NULL) {
            SDL_LogError(SDL_LOG_CATEGORY_AUDIO, "Couldn't allocate the ADX loop buffer.");
            SDL_zerop(info);
            return false;
        }

        info->position = 0;
    }

    return true;
}

static void loop_info_destroy(ADXLoopInfo* info) {
    if (info->looping_enabled) {
        free(info->data);
    }

    SDL_zerop(info);
}

static void fail_track(ADXTrack* track, const char* operation, int ffmpeg_error) {
    if (ffmpeg_error < 0) {
        print_av_error(ffmpeg_error);
    }

    SDL_LogError(SDL_LOG_CATEGORY_AUDIO, "ADX playback failed while %s.", operation);
    track->failed = true;
}

static void process_track(ADXTrack* track) {
    if (track->failed) {
        return;
    }

    ADXDecoderPipeline* pipeline = &track->pipeline;

    // Decode samples and queue them for playback
    while (stream_needs_data() && track_needs_decoding(track)) {
        int ret = av_parser_parse2(pipeline->parser_context,
                                   pipeline->context,
                                   &pipeline->packet->data,
                                   &pipeline->packet->size,
                                   track->data + track->used_bytes,
                                   track->size - track->used_bytes,
                                   AV_NOPTS_VALUE,
                                   AV_NOPTS_VALUE,
                                   0);

        if (ret < 0) {
            fail_track(track, "parsing the stream", ret);
            return;
        }

        track->used_bytes += ret;

        if (ret == 0 && pipeline->packet->size == 0) {
            fail_track(track, "advancing the stream parser", 0);
            return;
        }

        if (pipeline->packet->size > 0) {
            // Send parsed packet to decoder
            ret = avcodec_send_packet(pipeline->context, pipeline->packet);

            if (ret < 0) {
                fail_track(track, "sending a packet to the decoder", ret);
                return;
            }

            // Receive all available frames
            while (ret >= 0) {
                ret = avcodec_receive_frame(pipeline->context, pipeline->frame);

                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                    break;
                } else if (ret < 0) {
                    fail_track(track, "receiving a decoded frame", ret);
                    return;
                }

                const int out_channels = N_CHANNELS;
                const int out_samples = pipeline->frame->nb_samples;

                if (out_samples <= 0) {
                    fail_track(track, "validating a decoded frame", 0);
                    return;
                }

                // Allocate buffer for interleaved samples
                uint8_t* out_buf = NULL;
                int out_linesize = 0;

                ret = av_samples_alloc(&out_buf, &out_linesize, out_channels, out_samples, AV_SAMPLE_FMT_S16, 0);

                if (ret < 0 || out_buf == NULL) {
                    av_freep(&out_buf);
                    fail_track(track, "allocating converted samples", ret);
                    return;
                }

                // Convert planar → interleaved
                const int samples_converted = swr_convert(
                    pipeline->swr, &out_buf, out_samples, (const uint8_t**)pipeline->frame->data, out_samples);

                if (samples_converted < 0) {
                    av_freep(&out_buf);
                    fail_track(track, "converting decoded samples", samples_converted);
                    return;
                }

                const int overflow = track_add_samples_to_loop(track, out_buf, samples_converted);
                const int samples_to_queue = samples_converted - overflow;

                const int out_size =
                    av_samples_get_buffer_size(&out_linesize, out_channels, samples_to_queue, AV_SAMPLE_FMT_S16, 1);

                if (out_size < 0 || (out_size > 0 && !SDL_PutAudioStreamData(stream, out_buf, out_size))) {
                    av_freep(&out_buf);
                    fail_track(track, "queueing decoded audio", 0);
                    return;
                }

                av_freep(&out_buf);
            }
        }
    }

    // Queue looped samples (if needed)
    while (track_loop_filled(track) && stream_needs_data()) {
        const int available_data = track->loop_info.data_size - track->loop_info.position;
        const int data_to_queue = MIN(stream_data_needed(), available_data);
        if (data_to_queue <= 0 ||
            !SDL_PutAudioStreamData(stream, track->loop_info.data + track->loop_info.position, data_to_queue)) {
            fail_track(track, "queueing looped audio", 0);
            return;
        }
        track->loop_info.position += data_to_queue;

        if (track->loop_info.position == track->loop_info.data_size) {
            track->loop_info.position = 0;
        }
    }
}

static void track_init_from_data(ADXTrack* track,
                                 void* buf,
                                 size_t buf_size,
                                 size_t data_capacity,
    bool should_free_data_after_use,
                                 bool looping_allowed) {
    track->data = buf;
    track->data_capacity = data_capacity;
    track->should_free_data_after_use = should_free_data_after_use;
    track->used_bytes = 0;

    if (buf_size > (size_t)INT_MAX || track->data == NULL || buf_size < 0x34) {
        fail_track(track, "validating the ADX data", 0);
        return;
    }

    track->size = (int)buf_size;

    if (!pipeline_init(&track->pipeline)) {
        fail_track(track, "initializing the decoder", 0);
        return;
    }

    if (looping_allowed && !loop_info_init(&track->loop_info, track->data, track->size)) {
        fail_track(track, "reading loop metadata", 0);
        return;
    }

    process_track(track); // Feed first batch of data to the stream
}

static void track_init(ADXTrack* track, int file_id, void* buf, size_t buf_size, bool looping_allowed) {
    if (file_id == -1 && buf == NULL) {
        fail_track(track, "validating the track source", 0);
        return;
    }

    if (file_id != -1) {
        int size = 0;
        void* data = load_file(file_id, &size);
        track_init_from_data(track, data, size, size, true, looping_allowed);
    } else {
        track_init_from_data(track, buf, buf_size, buf_size, false, looping_allowed);
    }
}

static void track_destroy(ADXTrack* track) {
    pipeline_destroy(&track->pipeline);
    loop_info_destroy(&track->loop_info);

    if (track->should_free_data_after_use) {
        release_buffer(track->data, track->data_capacity);
    }

    SDL_zerop(track);
}

static int used_track_slots() {
    return num_tracks + num_pending_tracks;
}

static ADXTrack* alloc_track() {
    if (used_track_slots() >= TRACKS_MAX) {
        return NULL;
    }

    const int index = (first_track_index + num_tracks) % TRACKS_MAX;
    num_tracks += 1;
    has_tracks = true;
    return &tracks[index];
}

static ADXPendingTrack* alloc_pending_track() {
    if (used_track_slots() >= TRACKS_MAX) {
        return NULL;
    }

    const int index = (first_pending_track_index + num_pending_tracks) % TRACKS_MAX;
    ADXPendingTrack* pending_track = &pending_tracks[index];
    SDL_zerop(pending_track);
    pending_track->handle = AFS_NONE;
    pending_track->initialized = true;
    num_pending_tracks += 1;
    return pending_track;
}

static ADXPendingTrack* first_pending_track() {
    if (num_pending_tracks <= 0) {
        return NULL;
    }

    return &pending_tracks[first_pending_track_index];
}

static void remove_first_pending_track() {
    ADXPendingTrack* pending_track = first_pending_track();

    if (pending_track == NULL) {
        return;
    }

    SDL_zerop(pending_track);
    num_pending_tracks -= 1;

    if (num_pending_tracks > 0) {
        first_pending_track_index += 1;
        first_pending_track_index %= TRACKS_MAX;
    } else {
        first_pending_track_index = 0;
    }
}

static void pending_track_destroy(ADXPendingTrack* pending_track) {
    bool read_in_progress = false;
    AFSHandle handle = pending_track->handle;

    if (pending_track->handle != AFS_NONE) {
        read_in_progress = AFS_GetState(pending_track->handle) == AFS_READ_STATE_READING;
        AFS_Close(pending_track->handle);
    }

    if (read_in_progress) {
        retire_buffer(handle, pending_track->data, pending_track->data_capacity);
    } else {
        release_buffer(pending_track->data, pending_track->data_capacity);
    }

    SDL_zerop(pending_track);
}

static void cancel_pending_tracks() {
    for (int i = 0; i < num_pending_tracks; i++) {
        const int j = (first_pending_track_index + i) % TRACKS_MAX;
        pending_track_destroy(&pending_tracks[j]);
    }

    num_pending_tracks = 0;
    first_pending_track_index = 0;
}

static void queue_afs_track(int file_id, bool looping_allowed) {
    if (used_track_slots() >= TRACKS_MAX) {
        return;
    }

    const unsigned int file_size = fsGetFileSize(file_id);

    if (file_size == 0) {
        SDL_LogError(SDL_LOG_CATEGORY_AUDIO, "ADX resource %d is empty or unavailable.", file_id);
        return;
    }

    const size_t buff_size = (file_size + 2048 - 1) & ~(2048 - 1);
    size_t data_capacity = 0;
    uint8_t* data = acquire_buffer(buff_size, &data_capacity);

    if (data == NULL) {
        return;
    }

    AFSHandle handle = AFS_Open(file_id);

    if (handle == AFS_NONE) {
        release_buffer(data, data_capacity);
        return;
    }

    ADXPendingTrack* pending_track = alloc_pending_track();

    if (pending_track == NULL) {
        AFS_Close(handle);
        release_buffer(data, data_capacity);
        return;
    }

    pending_track->file_id = file_id;
    pending_track->size = file_size;
    pending_track->data_capacity = data_capacity;
    pending_track->sectors = fsCalSectorSize(file_size);
    pending_track->data = data;
    pending_track->looping_allowed = looping_allowed;
    pending_track->request_start_ns = SDL_GetTicksNS();
    pending_track->handle = handle;

    AFS_Read(pending_track->handle, pending_track->sectors, pending_track->data);
}

static bool finish_pending_track(ADXPendingTrack* pending_track) {
    uint8_t* data = pending_track->data;
    const int size = pending_track->size;
    const size_t data_capacity = pending_track->data_capacity;
    const bool looping_allowed = pending_track->looping_allowed;

    pending_track->data = NULL;
    AFS_Close(pending_track->handle);
    pending_track->handle = AFS_NONE;
    remove_first_pending_track();

    ADXTrack* track = alloc_track();

    if (track == NULL) {
        release_buffer(data, data_capacity);
        return false;
    }

    track_init_from_data(track, data, size, data_capacity, true, looping_allowed);
    return true;
}

static void process_pending_tracks() {
    while (num_pending_tracks > 0) {
        ADXPendingTrack* pending_track = first_pending_track();
        const AFSReadState state = AFS_GetState(pending_track->handle);

        if (state == AFS_READ_STATE_READING || state == AFS_READ_STATE_IDLE) {
            break;
        }

        if (state == AFS_READ_STATE_ERROR) {
            pending_track_destroy(pending_track);
            remove_first_pending_track();
            continue;
        }

        if (!finish_pending_track(pending_track)) {
            break;
        }
    }
}

void ADX_ProcessTracks() {
    if (stream == NULL) {
        return;
    }

    release_retired_buffers(false);
    process_pending_tracks();

    const int first_track_index_old = first_track_index;
    const int num_tracks_old = num_tracks;

    for (int i = 0; i < num_tracks_old; i++) {
        const int j = (first_track_index_old + i) % TRACKS_MAX;
        ADXTrack* track = &tracks[j];
        process_track(track);

        if (!track_exhausted(track)) {
            // No need to continue if the current track is not exhausted yet
            break;
        }

        track_destroy(track);
        num_tracks -= 1;

        if (num_tracks > 0) {
            first_track_index += 1;
        } else {
            first_track_index = 0;
        }
    }
}

bool ADX_Init() {
    const SDL_AudioSpec spec = { .format = SDL_AUDIO_S16, .channels = N_CHANNELS, .freq = SAMPLE_RATE };
    stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, NULL, NULL);

    if (stream == NULL) {
        SDL_Log("Couldn't create the ADX music stream; music will be disabled: %s", SDL_GetError());
        return false;
    }

    return true;
}

void ADX_Exit() {
    ADX_Stop();
    release_retired_buffers(true);

    if (stream != NULL) {
        SDL_DestroyAudioStream(stream);
        stream = NULL;
    }

    free_buffer_pool();
}

void ADX_Stop() {
    if (stream != NULL) {
        ADX_Pause(true);
        SDL_ClearAudioStream(stream);
    }

    cancel_pending_tracks();

    for (int i = 0; i < num_tracks; i++) {
        const int j = (first_track_index + i) % TRACKS_MAX;
        track_destroy(&tracks[j]);
    }

    num_tracks = 0;
    first_track_index = 0;
    has_tracks = false;
}

int ADX_IsPaused() {
    if (stream == NULL) {
        return 1;
    }

    return SDL_AudioStreamDevicePaused(stream);
}

void ADX_Pause(int pause) {
    if (stream == NULL) {
        return;
    }

    if (pause) {
        SDL_PauseAudioStreamDevice(stream);
    } else {
        SDL_ResumeAudioStreamDevice(stream);
    }
}

void ADX_StartMem(void* buf, size_t size) {
    if (stream == NULL) {
        return;
    }

    const Uint64 start_ns = DebugLog_IsEnabled() ? SDL_GetTicksNS() : 0;
    ADX_Stop();

    ADXTrack* track = alloc_track();

    if (track == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_AUDIO, "No ADX track slot is available.");
        return;
    }

    track_init(track, -1, buf, size, true);

    if (DebugLog_IsEnabled()) {
        DebugLog_RecordAdxStartMem(size, (double)(SDL_GetTicksNS() - start_ns) / 1e6);
    }
}

int ADX_GetNumFiles() {
    return num_tracks + num_pending_tracks;
}

void ADX_EntryAfs(int file_id) {
    if (stream == NULL) {
        return;
    }

    const Uint64 start_ns = DebugLog_IsEnabled() ? SDL_GetTicksNS() : 0;
    queue_afs_track(file_id, false);

    if (DebugLog_IsEnabled()) {
        DebugLog_RecordAdxEntryAfs(file_id, (double)(SDL_GetTicksNS() - start_ns) / 1e6);
    }
}

void ADX_StartSeamless() {
    ADX_Pause(false);
}

void ADX_ResetEntry() {
    // ResetEntry is always called after Stop, so we don't need to do anything here
}

void ADX_StartAfs(int file_id) {
    if (stream == NULL) {
        return;
    }

    const Uint64 start_ns = DebugLog_IsEnabled() ? SDL_GetTicksNS() : 0;
    ADX_Stop();

    queue_afs_track(file_id, true);

    if (DebugLog_IsEnabled()) {
        DebugLog_RecordAdxStartAfs(file_id, (double)(SDL_GetTicksNS() - start_ns) / 1e6);
    }
}

void ADX_SetOutVol(int volume) {
    if (stream == NULL) {
        return;
    }

    // Convert volume (dB * 10) to linear gain
    const float gain = powf(10.0f, volume / 200.0f);
    SDL_SetAudioStreamGain(stream, gain);
}

void ADX_SetMono(bool mono) {
    // FIXME: Do we really need this?
}

ADXState ADX_GetState() {
    if (stream == NULL || !has_tracks) {
        return ADX_STATE_STOP;
    }

    if (stream_is_empty()) {
        return ADX_STATE_PLAYEND;
    } else {
        if (ADX_IsPaused()) {
            return ADX_STATE_STOP;
        } else {
            return ADX_STATE_PLAYING;
        }
    }
}
