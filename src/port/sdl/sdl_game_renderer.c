#include "port/sdl/sdl_game_renderer.h"
#include "common.h"
#include "port/debug/debug_log.h"
#include "port/utils.h"
#include "sf33rd/AcrSDK/ps2/flps2etc.h"
#include "sf33rd/AcrSDK/ps2/flps2render.h"
#include "sf33rd/AcrSDK/ps2/foundaps2.h"
#include "sf33rd/Source/Game/system/work_sys.h"

#include <libgraph.h>

#include <SDL3/SDL.h>

#include <stdio.h>
#include <stdlib.h>

#define RENDER_TASK_MAX 1024
#define TEXTURE_DESTROY_QUEUE_INITIAL_CAPACITY 1024

typedef struct RenderTask {
    SDL_Texture* texture;
    SDL_Vertex vertices[4];
    float z;
    int index;
} RenderTask;

typedef enum TextureCacheInvalidationReason {
    TEXTURE_CACHE_INVALIDATION_NONE,
    TEXTURE_CACHE_INVALIDATION_PALETTE_UNLOCK,
    TEXTURE_CACHE_INVALIDATION_TEXTURE_UNLOCK,
    TEXTURE_CACHE_INVALIDATION_RELEASE,
} TextureCacheInvalidationReason;

typedef enum TextureCacheMode {
    TEXTURE_CACHE_MODE_DEFAULT,
    TEXTURE_CACHE_MODE_RGBA_STREAMING,
    TEXTURE_CACHE_MODE_PALETTIZED,
} TextureCacheMode;

typedef struct TexturePaletteHandleStats {
    Uint32 set_texture_calls;
    Uint32 cache_hits;
    Uint32 cache_misses;
    Uint32 miss_first_use;
    Uint32 miss_after_palette_unlock;
    Uint32 miss_after_texture_unlock;
    Uint32 miss_after_release;
    Uint32 miss_unknown;
    Uint32 invalidated_by_palette_unlock;
    Uint32 invalidated_by_texture_unlock;
    Uint32 invalidated_by_release;
    Uint32 rgba_fallbacks;
} TexturePaletteHandleStats;

typedef struct TextureHandleStats {
    Uint32 set_texture_calls;
    Uint32 cache_hits;
    Uint32 cache_misses;
    Uint32 miss_first_use;
    Uint32 miss_after_palette_unlock;
    Uint32 miss_after_texture_unlock;
    Uint32 miss_after_release;
    Uint32 miss_unknown;
    Uint32 texture_unlocks;
    Uint32 invalidated_by_palette_unlock;
    Uint32 invalidated_by_texture_unlock;
    Uint32 invalidated_by_release;
    Uint32 rgba_fallbacks;
} TextureHandleStats;

typedef struct PaletteHandleStats {
    Uint32 set_texture_calls;
    Uint32 cache_hits;
    Uint32 cache_misses;
    Uint32 miss_first_use;
    Uint32 miss_after_palette_unlock;
    Uint32 miss_after_texture_unlock;
    Uint32 miss_after_release;
    Uint32 miss_unknown;
    Uint32 palette_unlocks;
    Uint32 invalidated_by_palette_unlock;
    Uint32 invalidated_by_texture_unlock;
    Uint32 invalidated_by_release;
    Uint32 rgba_fallbacks;
} PaletteHandleStats;

SDL_Texture* cps3_canvas = NULL;

static const int cps3_width = 384;
static const int cps3_height = 224;

static SDL_Renderer* _renderer = NULL;
static SDL_Surface* surfaces[FL_TEXTURE_MAX] = { NULL };
static SDL_Palette* palettes[FL_PALETTE_MAX] = { NULL };
static SDL_Texture* textures[FL_PALETTE_MAX] = { NULL };
static int texture_count = 0;
static SDL_Texture* texture_cache[FL_TEXTURE_MAX][FL_PALETTE_MAX + 1] = { { NULL } };
static TextureCacheMode texture_cache_mode[FL_TEXTURE_MAX][FL_PALETTE_MAX + 1] = { { TEXTURE_CACHE_MODE_DEFAULT } };
static bool texture_cache_pixels_dirty[FL_TEXTURE_MAX][FL_PALETTE_MAX + 1] = { { false } };
static bool texture_cache_palette_dirty[FL_TEXTURE_MAX][FL_PALETTE_MAX + 1] = { { false } };
static bool texture_cache_used_this_frame[FL_TEXTURE_MAX][FL_PALETTE_MAX + 1] = { { false } };
static bool texture_cache_has_been_created[FL_TEXTURE_MAX][FL_PALETTE_MAX + 1] = { { false } };
static TextureCacheInvalidationReason texture_cache_last_invalidation[FL_TEXTURE_MAX][FL_PALETTE_MAX + 1] = { { 0 } };
static SDL_Texture** textures_to_destroy = NULL;
static int textures_to_destroy_count = 0;
static int textures_to_destroy_capacity = 0;
static RenderTask render_tasks[RENDER_TASK_MAX] = { 0 };
static int render_task_count = 0;
static int texture_cache_miss_count = 0;
static int texture_cache_miss_first_use_count = 0;
static int texture_cache_miss_after_palette_unlock_count = 0;
static int texture_cache_miss_after_texture_unlock_count = 0;
static int texture_cache_miss_after_release_count = 0;
static int texture_cache_miss_unknown_count = 0;
static int palette_unlock_count = 0;
static int texture_unlock_count = 0;
static int palette_cache_invalidated_texture_count = 0;
static int texture_cache_invalidated_texture_count = 0;
static int release_cache_invalidated_texture_count = 0;
static int indexed_texture_update_count = 0;
static int indexed_texture_update_pixel_count = 0;
static double indexed_texture_update_ms = 0.0;
static int indexed_palette_update_count = 0;
static double indexed_palette_update_ms = 0.0;
static int indexed_texture_rgba_fallback_count = 0;
static bool debug_indexed_texture_path_enabled = false;
static TextureCacheInvalidationReason active_texture_invalidation_reason = TEXTURE_CACHE_INVALIDATION_RELEASE;
static TextureCacheInvalidationReason active_palette_invalidation_reason = TEXTURE_CACHE_INVALIDATION_RELEASE;
static TexturePaletteHandleStats* texture_palette_handle_stats = NULL;
static TextureHandleStats texture_handle_stats[FL_TEXTURE_MAX + 1] = { 0 };
static PaletteHandleStats palette_handle_stats[FL_PALETTE_MAX + 1] = { 0 };
static Uint8* indexed_texture_rgba_buffer = NULL;
static size_t indexed_texture_rgba_buffer_size = 0;
static Uint8* indexed_texture_index8_buffer = NULL;
static size_t indexed_texture_index8_buffer_size = 0;

// Debugging

static bool draw_rect_borders = false;
static bool dump_textures = false;

static int texture_index = 0;

static void save_texture(const SDL_Surface* surface, const SDL_Palette* palette) {
    char filename[128];
    sprintf(filename, "textures/%d.tga", texture_index);

    const Uint8* pixels = surface->pixels;
    const int width = surface->w;
    const int height = surface->h;

    FILE* f = fopen(filename, "wb");

    if (!f) {
        return;
    }

    uint8_t header[18] = { 0 };
    header[2] = 2; // uncompressed RGB
    header[12] = width & 0xFF;
    header[13] = (width >> 8) & 0xFF;
    header[14] = height & 0xFF;
    header[15] = (height >> 8) & 0xFF;
    header[16] = 32;   // bits per pixel
    header[17] = 0x20; // top-left origin

    fwrite(header, 1, 18, f);

    // Write pixels in BGRA format
    for (int i = 0; i < width * height; ++i) {
        Uint8 index = pixels[i];

        switch (palette->ncolors) {
        case 16:
            if (i & 1) {
                index >>= 4;
            } else {
                index &= 0xF;
            }

            break;

        case 256:
            break;
        }

        const SDL_Color* color = &palette->colors[index];
        const Uint8 bgr[] = { color->b, color->g, color->r, color->a };
        fwrite(bgr, 1, 4, f);
    }

    fclose(f);
    texture_index += 1;
}

// Textures

static bool is_valid_texture_handle(int texture_handle) {
    return texture_handle > 0 && texture_handle <= FL_TEXTURE_MAX;
}

static bool is_valid_palette_handle(int palette_handle) {
    return palette_handle > 0 && palette_handle <= FL_PALETTE_MAX;
}

static void push_texture(SDL_Texture* texture) {
    if (texture_count >= FL_PALETTE_MAX) {
        fatal_error("Texture stack overflow");
    }

    textures[texture_count] = texture;
    texture_count += 1;
}

static SDL_Texture* get_texture() {
    if (texture_count == 0) {
        fatal_error("No textures to get");
    }

    return textures[texture_count - 1];
}

static bool reserve_textures_to_destroy(int required_capacity) {
    if (textures_to_destroy_capacity >= required_capacity) {
        return true;
    }

    int new_capacity = textures_to_destroy_capacity;

    if (new_capacity == 0) {
        new_capacity = TEXTURE_DESTROY_QUEUE_INITIAL_CAPACITY;
    }

    while (new_capacity < required_capacity) {
        new_capacity *= 2;
    }

    SDL_Texture** new_textures_to_destroy =
        SDL_realloc(textures_to_destroy, (size_t)new_capacity * sizeof(SDL_Texture*));

    if (new_textures_to_destroy == NULL) {
        return false;
    }

    textures_to_destroy = new_textures_to_destroy;
    textures_to_destroy_capacity = new_capacity;
    return true;
}

static void push_texture_to_destroy(SDL_Texture* texture) {
    if (texture == NULL) {
        return;
    }

    if (!reserve_textures_to_destroy(textures_to_destroy_count + 1)) {
        fatal_error("Failed to grow texture destroy queue");
    }

    textures_to_destroy[textures_to_destroy_count] = texture;
    textures_to_destroy_count += 1;
}

static void destroy_textures() {
    for (int i = 0; i < texture_count; i++) {
        textures[i] = NULL;
    }

    texture_count = 0;

    for (int i = 0; i < textures_to_destroy_count; i++) {
        SDL_DestroyTexture(textures_to_destroy[i]);
    }

    textures_to_destroy_count = 0;
}

static void push_render_task(RenderTask* task) {
    if (No_Trans) {
        printf("⚠️ Requesting a render task when no rendering is allowed is a programmer error!\n");
    }

    if (render_task_count >= RENDER_TASK_MAX) {
        fatal_error("Render task queue overflow");
    }

    memcpy(&render_tasks[render_task_count], task, sizeof(RenderTask));
    render_task_count += 1;
}

static void clear_render_tasks() {
    SDL_zeroa(render_tasks);
    render_task_count = 0;
}

static int compare_render_tasks(const RenderTask* a, const RenderTask* b) {
    if (a->z < b->z) {
        return -1;
    } else if (a->z > b->z) {
        return 1;
    } else {
        // This eliminates z-fighting
        if (a->index < b->index) {
            return 1;
        } else if (a->index > b->index) {
            return -1;
        } else {
            return 0;
        }
    }
}

static double elapsed_ms(Uint64 start_ns, Uint64 end_ns) {
    return (double)(end_ns - start_ns) / 1e6;
}

static bool is_indexed_surface(const SDL_Surface* surface) {
    return surface != NULL && (surface->format == SDL_PIXELFORMAT_INDEX8 || surface->format == SDL_PIXELFORMAT_INDEX4LSB);
}

static bool should_record_texture_diagnostics() {
    return DebugLog_IsEnabled() && debug_indexed_texture_path_enabled;
}

static bool ensure_texture_handle_stats() {
    if (!debug_indexed_texture_path_enabled) {
        return false;
    }

    if (texture_palette_handle_stats != NULL) {
        return true;
    }

    const size_t stats_count = FL_TEXTURE_MAX * (FL_PALETTE_MAX + 1);
    texture_palette_handle_stats = SDL_calloc(stats_count, sizeof(TexturePaletteHandleStats));

    if (texture_palette_handle_stats == NULL) {
        SDL_Log("Failed to allocate texture handle debug stats.");
        return false;
    }

    SDL_zeroa(texture_handle_stats);
    SDL_zeroa(palette_handle_stats);
    return true;
}

static size_t texture_palette_stats_index(int texture_index, int palette_handle) {
    return ((size_t)texture_index * (FL_PALETTE_MAX + 1)) + (size_t)palette_handle;
}

static TexturePaletteHandleStats* get_texture_palette_handle_stats(int texture_index, int palette_handle) {
    if (!should_record_texture_diagnostics() || !ensure_texture_handle_stats()) {
        return NULL;
    }

    return &texture_palette_handle_stats[texture_palette_stats_index(texture_index, palette_handle)];
}

static void reset_texture_diagnostics() {
    texture_cache_miss_first_use_count = 0;
    texture_cache_miss_after_palette_unlock_count = 0;
    texture_cache_miss_after_texture_unlock_count = 0;
    texture_cache_miss_after_release_count = 0;
    texture_cache_miss_unknown_count = 0;
    palette_unlock_count = 0;
    texture_unlock_count = 0;
    palette_cache_invalidated_texture_count = 0;
    texture_cache_invalidated_texture_count = 0;
    release_cache_invalidated_texture_count = 0;
    indexed_texture_update_count = 0;
    indexed_texture_update_pixel_count = 0;
    indexed_texture_update_ms = 0.0;
    indexed_palette_update_count = 0;
    indexed_palette_update_ms = 0.0;
    indexed_texture_rgba_fallback_count = 0;
}

static void record_texture_cache_invalidation(int texture_index, int palette_handle, TextureCacheInvalidationReason reason) {
    if (!should_record_texture_diagnostics()) {
        return;
    }

    TexturePaletteHandleStats* pair_stats = get_texture_palette_handle_stats(texture_index, palette_handle);
    TextureHandleStats* texture_stats = &texture_handle_stats[texture_index + 1];
    PaletteHandleStats* palette_stats = palette_handle > 0 ? &palette_handle_stats[palette_handle] : NULL;

    switch (reason) {
    case TEXTURE_CACHE_INVALIDATION_PALETTE_UNLOCK:
        palette_cache_invalidated_texture_count += 1;
        texture_stats->invalidated_by_palette_unlock += 1;

        if (palette_stats != NULL) {
            palette_stats->invalidated_by_palette_unlock += 1;
        }

        if (pair_stats != NULL) {
            pair_stats->invalidated_by_palette_unlock += 1;
        }

        break;

    case TEXTURE_CACHE_INVALIDATION_TEXTURE_UNLOCK:
        texture_cache_invalidated_texture_count += 1;
        texture_stats->invalidated_by_texture_unlock += 1;

        if (palette_stats != NULL) {
            palette_stats->invalidated_by_texture_unlock += 1;
        }

        if (pair_stats != NULL) {
            pair_stats->invalidated_by_texture_unlock += 1;
        }

        break;

    case TEXTURE_CACHE_INVALIDATION_RELEASE:
        release_cache_invalidated_texture_count += 1;
        texture_stats->invalidated_by_release += 1;

        if (palette_stats != NULL) {
            palette_stats->invalidated_by_release += 1;
        }

        if (pair_stats != NULL) {
            pair_stats->invalidated_by_release += 1;
        }

        break;

    case TEXTURE_CACHE_INVALIDATION_NONE:
    default:
        break;
    }
}

static void record_texture_cache_access(int texture_index, int palette_handle, bool cache_hit) {
    if (!should_record_texture_diagnostics()) {
        return;
    }

    TexturePaletteHandleStats* pair_stats = get_texture_palette_handle_stats(texture_index, palette_handle);
    TextureHandleStats* texture_stats = &texture_handle_stats[texture_index + 1];
    PaletteHandleStats* palette_stats = palette_handle > 0 ? &palette_handle_stats[palette_handle] : NULL;

    texture_stats->set_texture_calls += 1;

    if (palette_stats != NULL) {
        palette_stats->set_texture_calls += 1;
    }

    if (pair_stats != NULL) {
        pair_stats->set_texture_calls += 1;
    }

    if (cache_hit) {
        texture_stats->cache_hits += 1;

        if (palette_stats != NULL) {
            palette_stats->cache_hits += 1;
        }

        if (pair_stats != NULL) {
            pair_stats->cache_hits += 1;
        }
    }
}

static void record_texture_handle_miss(int texture_index,
                                       int palette_handle,
                                       bool first_use,
                                       TextureCacheInvalidationReason reason) {
    TexturePaletteHandleStats* pair_stats = get_texture_palette_handle_stats(texture_index, palette_handle);
    TextureHandleStats* texture_stats = &texture_handle_stats[texture_index + 1];
    PaletteHandleStats* palette_stats = palette_handle > 0 ? &palette_handle_stats[palette_handle] : NULL;

    texture_stats->cache_misses += 1;

    if (palette_stats != NULL) {
        palette_stats->cache_misses += 1;
    }

    if (pair_stats != NULL) {
        pair_stats->cache_misses += 1;
    }

#define ADD_MISS_FIELD(field)               \
    do {                                    \
        texture_stats->field += 1;          \
        if (palette_stats != NULL) {        \
            palette_stats->field += 1;      \
        }                                   \
        if (pair_stats != NULL) {           \
            pair_stats->field += 1;         \
        }                                   \
    } while (0)

    if (first_use) {
        ADD_MISS_FIELD(miss_first_use);
    } else {
        switch (reason) {
        case TEXTURE_CACHE_INVALIDATION_PALETTE_UNLOCK:
            ADD_MISS_FIELD(miss_after_palette_unlock);
            break;

        case TEXTURE_CACHE_INVALIDATION_TEXTURE_UNLOCK:
            ADD_MISS_FIELD(miss_after_texture_unlock);
            break;

        case TEXTURE_CACHE_INVALIDATION_RELEASE:
            ADD_MISS_FIELD(miss_after_release);
            break;

        case TEXTURE_CACHE_INVALIDATION_NONE:
        default:
            ADD_MISS_FIELD(miss_unknown);
            break;
        }
    }

#undef ADD_MISS_FIELD
}

static void record_texture_cache_miss(int texture_index, int palette_handle) {
    if (!DebugLog_IsEnabled()) {
        return;
    }

    texture_cache_miss_count += 1;

    if (!debug_indexed_texture_path_enabled) {
        return;
    }

    const bool first_use = !texture_cache_has_been_created[texture_index][palette_handle];
    const TextureCacheInvalidationReason reason = texture_cache_last_invalidation[texture_index][palette_handle];

    if (first_use) {
        texture_cache_miss_first_use_count += 1;
    } else {
        switch (reason) {
        case TEXTURE_CACHE_INVALIDATION_PALETTE_UNLOCK:
            texture_cache_miss_after_palette_unlock_count += 1;
            break;

        case TEXTURE_CACHE_INVALIDATION_TEXTURE_UNLOCK:
            texture_cache_miss_after_texture_unlock_count += 1;
            break;

        case TEXTURE_CACHE_INVALIDATION_RELEASE:
            texture_cache_miss_after_release_count += 1;
            break;

        case TEXTURE_CACHE_INVALIDATION_NONE:
        default:
            texture_cache_miss_unknown_count += 1;
            break;
        }
    }

    record_texture_handle_miss(texture_index, palette_handle, first_use, reason);
    texture_cache_has_been_created[texture_index][palette_handle] = true;
    texture_cache_last_invalidation[texture_index][palette_handle] = TEXTURE_CACHE_INVALIDATION_NONE;
}

static void record_indexed_texture_rgba_fallback(int texture_index, int palette_handle) {
    indexed_texture_rgba_fallback_count += 1;

    if (!should_record_texture_diagnostics()) {
        return;
    }

    TexturePaletteHandleStats* pair_stats = get_texture_palette_handle_stats(texture_index, palette_handle);
    TextureHandleStats* texture_stats = &texture_handle_stats[texture_index + 1];
    PaletteHandleStats* palette_stats = palette_handle > 0 ? &palette_handle_stats[palette_handle] : NULL;

    texture_stats->rgba_fallbacks += 1;

    if (palette_stats != NULL) {
        palette_stats->rgba_fallbacks += 1;
    }

    if (pair_stats != NULL) {
        pair_stats->rgba_fallbacks += 1;
    }
}

static bool pair_stats_has_data(const TexturePaletteHandleStats* stats) {
    return stats->set_texture_calls > 0 || stats->cache_misses > 0 || stats->invalidated_by_palette_unlock > 0 ||
           stats->invalidated_by_texture_unlock > 0 || stats->invalidated_by_release > 0 || stats->rgba_fallbacks > 0;
}

static bool texture_stats_has_data(const TextureHandleStats* stats) {
    return stats->set_texture_calls > 0 || stats->cache_misses > 0 || stats->texture_unlocks > 0 ||
           stats->invalidated_by_palette_unlock > 0 || stats->invalidated_by_texture_unlock > 0 ||
           stats->invalidated_by_release > 0 || stats->rgba_fallbacks > 0;
}

static bool palette_stats_has_data(const PaletteHandleStats* stats) {
    return stats->set_texture_calls > 0 || stats->cache_misses > 0 || stats->palette_unlocks > 0 ||
           stats->invalidated_by_palette_unlock > 0 || stats->invalidated_by_texture_unlock > 0 ||
           stats->invalidated_by_release > 0 || stats->rgba_fallbacks > 0;
}

static bool ensure_indexed_texture_rgba_buffer(int width, int height) {
    const size_t required_size = (size_t)width * (size_t)height * 4;

    if (indexed_texture_rgba_buffer_size >= required_size) {
        return true;
    }

    Uint8* new_buffer = SDL_realloc(indexed_texture_rgba_buffer, required_size);

    if (new_buffer == NULL) {
        SDL_Log("Failed to allocate indexed texture RGBA buffer.");
        return false;
    }

    indexed_texture_rgba_buffer = new_buffer;
    indexed_texture_rgba_buffer_size = required_size;
    return true;
}

static bool ensure_indexed_texture_index8_buffer(int width, int height) {
    const size_t required_size = (size_t)width * (size_t)height;

    if (indexed_texture_index8_buffer_size >= required_size) {
        return true;
    }

    Uint8* new_buffer = SDL_realloc(indexed_texture_index8_buffer, required_size);

    if (new_buffer == NULL) {
        SDL_Log("Failed to allocate indexed texture INDEX8 buffer.");
        return false;
    }

    indexed_texture_index8_buffer = new_buffer;
    indexed_texture_index8_buffer_size = required_size;
    return true;
}

static bool write_indexed_texture_rgba_pixels(const SDL_Surface* surface, const SDL_Palette* palette) {
    if (!is_indexed_surface(surface) || palette == NULL || !ensure_indexed_texture_rgba_buffer(surface->w, surface->h)) {
        return false;
    }

    Uint8* dst = indexed_texture_rgba_buffer;

    for (int y = 0; y < surface->h; y++) {
        const Uint8* src = (const Uint8*)surface->pixels + ((size_t)y * (size_t)surface->pitch);

        for (int x = 0; x < surface->w; x++) {
            Uint8 color_index = 0;

            if (surface->format == SDL_PIXELFORMAT_INDEX8) {
                color_index = src[x];
            } else {
                const Uint8 packed_indices = src[x / 2];
                color_index = (x & 1) ? (packed_indices >> 4) : (packed_indices & 0xF);
            }

            if (color_index >= palette->ncolors) {
                color_index = 0;
            }

            const SDL_Color* color = &palette->colors[color_index];
            *dst++ = color->r;
            *dst++ = color->g;
            *dst++ = color->b;
            *dst++ = color->a;
        }
    }

    return true;
}

static bool write_index4_texture_index8_pixels(const SDL_Surface* surface) {
    if (surface == NULL || surface->format != SDL_PIXELFORMAT_INDEX4LSB ||
        !ensure_indexed_texture_index8_buffer(surface->w, surface->h)) {
        return false;
    }

    Uint8* dst = indexed_texture_index8_buffer;

    for (int y = 0; y < surface->h; y++) {
        const Uint8* src = (const Uint8*)surface->pixels + ((size_t)y * (size_t)surface->pitch);

        for (int x = 0; x < surface->w; x++) {
            const Uint8 packed_indices = src[x / 2];
            *dst++ = (x & 1) ? (packed_indices >> 4) : (packed_indices & 0xF);
        }
    }

    return true;
}

static bool update_indexed_streaming_texture(SDL_Texture* texture, const SDL_Surface* surface, const SDL_Palette* palette) {
    if (texture == NULL) {
        return false;
    }

    const Uint64 update_start_ns = SDL_GetTicksNS();

    if (!write_indexed_texture_rgba_pixels(surface, palette)) {
        return false;
    }

    const bool updated = SDL_UpdateTexture(texture, NULL, indexed_texture_rgba_buffer, surface->w * 4);
    const Uint64 update_end_ns = SDL_GetTicksNS();

    if (!updated) {
        SDL_Log("Failed to update indexed texture: %s", SDL_GetError());
        return false;
    }

    indexed_texture_update_count += 1;
    indexed_texture_update_pixel_count += surface->w * surface->h;
    indexed_texture_update_ms += elapsed_ms(update_start_ns, update_end_ns);
    return true;
}

static bool update_indexed_texture_pixels(SDL_Texture* texture, const SDL_Surface* surface) {
    if (texture == NULL || surface == NULL) {
        return false;
    }

    const Uint64 update_start_ns = SDL_GetTicksNS();
    bool updated = false;

    if (surface->format == SDL_PIXELFORMAT_INDEX4LSB) {
        if (!write_index4_texture_index8_pixels(surface)) {
            return false;
        }

        updated = SDL_UpdateTexture(texture, NULL, indexed_texture_index8_buffer, surface->w);
    } else {
        updated = SDL_UpdateTexture(texture, NULL, surface->pixels, surface->pitch);
    }

    const Uint64 update_end_ns = SDL_GetTicksNS();

    if (!updated) {
        SDL_Log("Failed to update indexed texture pixels: %s", SDL_GetError());
        return false;
    }

    indexed_texture_update_count += 1;
    indexed_texture_update_pixel_count += surface->w * surface->h;
    indexed_texture_update_ms += elapsed_ms(update_start_ns, update_end_ns);
    return true;
}

static SDL_PixelFormat indexed_texture_upload_format(const SDL_Surface* surface) {
    if (surface->format == SDL_PIXELFORMAT_INDEX4LSB) {
        return SDL_PIXELFORMAT_INDEX8;
    }

    return surface->format;
}

static bool update_indexed_texture_palette(SDL_Texture* texture, const SDL_Palette* palette) {
    if (texture == NULL || palette == NULL) {
        return false;
    }

    const Uint64 update_start_ns = SDL_GetTicksNS();
    const bool updated = SDL_SetTexturePalette(texture, (SDL_Palette*)palette);
    const Uint64 update_end_ns = SDL_GetTicksNS();

    if (!updated) {
        SDL_Log("Failed to update indexed texture palette: %s", SDL_GetError());
        return false;
    }

    indexed_palette_update_count += 1;
    indexed_palette_update_ms += elapsed_ms(update_start_ns, update_end_ns);
    return true;
}

static SDL_Texture* create_indexed_rgba_streaming_texture(const SDL_Surface* surface, const SDL_Palette* palette) {
    SDL_Texture* texture =
        SDL_CreateTexture(_renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING, surface->w, surface->h);

    if (texture == NULL) {
        SDL_Log("Failed to create indexed streaming texture: %s", SDL_GetError());
        return NULL;
    }

    SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);
    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);

    if (!update_indexed_streaming_texture(texture, surface, palette)) {
        SDL_DestroyTexture(texture);
        return NULL;
    }

    return texture;
}

static SDL_Texture* create_indexed_palettized_texture(const SDL_Surface* surface, const SDL_Palette* palette) {
    SDL_Texture* texture = SDL_CreateTexture(
        _renderer, indexed_texture_upload_format(surface), SDL_TEXTUREACCESS_STATIC, surface->w, surface->h);

    if (texture == NULL) {
        SDL_Log("Failed to create indexed palettized texture: %s", SDL_GetError());
        return NULL;
    }

    SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);
    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);

    if (!update_indexed_texture_palette(texture, palette) || !update_indexed_texture_pixels(texture, surface)) {
        SDL_DestroyTexture(texture);
        return NULL;
    }

    return texture;
}

static SDL_Texture* create_experimental_indexed_texture(int texture_index,
                                                        int palette_handle,
                                                        const SDL_Surface* surface,
                                                        const SDL_Palette* palette,
                                                        TextureCacheMode* texture_mode) {
    SDL_Texture* texture = create_indexed_palettized_texture(surface, palette);

    if (texture != NULL) {
        *texture_mode = TEXTURE_CACHE_MODE_PALETTIZED;
        return texture;
    }

    record_indexed_texture_rgba_fallback(texture_index, palette_handle);
    texture = create_indexed_rgba_streaming_texture(surface, palette);

    if (texture != NULL) {
        *texture_mode = TEXTURE_CACHE_MODE_RGBA_STREAMING;
    }

    return texture;
}

static bool should_use_experimental_indexed_texture(const SDL_Surface* surface, const SDL_Palette* palette) {
    return should_record_texture_diagnostics() && is_indexed_surface(surface) && palette != NULL;
}

static void mark_experimental_textures_dirty_for_palette(int palette_handle) {
    if (!should_record_texture_diagnostics() || !is_valid_palette_handle(palette_handle)) {
        return;
    }

    for (int texture_index = 0; texture_index < FL_TEXTURE_MAX; texture_index++) {
        SDL_Texture* texture = texture_cache[texture_index][palette_handle];

        if (texture == NULL || texture_cache_mode[texture_index][palette_handle] == TEXTURE_CACHE_MODE_DEFAULT) {
            continue;
        }

        if (texture_cache_mode[texture_index][palette_handle] == TEXTURE_CACHE_MODE_PALETTIZED) {
            texture_cache_palette_dirty[texture_index][palette_handle] = true;
        } else {
            texture_cache_pixels_dirty[texture_index][palette_handle] = true;
        }
    }
}

static void mark_experimental_textures_dirty_for_texture(int texture_handle) {
    if (!should_record_texture_diagnostics() || !is_valid_texture_handle(texture_handle)) {
        return;
    }

    const int texture_index = texture_handle - 1;

    for (int palette_handle = 1; palette_handle < FL_PALETTE_MAX + 1; palette_handle++) {
        SDL_Texture* texture = texture_cache[texture_index][palette_handle];

        if (texture == NULL || texture_cache_mode[texture_index][palette_handle] == TEXTURE_CACHE_MODE_DEFAULT) {
            continue;
        }

        texture_cache_pixels_dirty[texture_index][palette_handle] = true;
    }
}

static void reset_texture_cache_entry(int texture_index, int palette_handle) {
    texture_cache[texture_index][palette_handle] = NULL;
    texture_cache_mode[texture_index][palette_handle] = TEXTURE_CACHE_MODE_DEFAULT;
    texture_cache_pixels_dirty[texture_index][palette_handle] = false;
    texture_cache_palette_dirty[texture_index][palette_handle] = false;
}

static bool update_cached_experimental_indexed_texture(SDL_Texture* texture,
                                                       const SDL_Surface* surface,
                                                       const SDL_Palette* palette,
                                                       int texture_index,
                                                       int palette_handle) {
    const TextureCacheMode mode = texture_cache_mode[texture_index][palette_handle];

    if (mode == TEXTURE_CACHE_MODE_PALETTIZED) {
        if (texture_cache_pixels_dirty[texture_index][palette_handle] && !update_indexed_texture_pixels(texture, surface)) {
            return false;
        }

        if (texture_cache_palette_dirty[texture_index][palette_handle] &&
            !update_indexed_texture_palette(texture, palette)) {
            return false;
        }
    } else if (mode == TEXTURE_CACHE_MODE_RGBA_STREAMING) {
        if ((texture_cache_pixels_dirty[texture_index][palette_handle] ||
             texture_cache_palette_dirty[texture_index][palette_handle]) &&
            !update_indexed_streaming_texture(texture, surface, palette)) {
            return false;
        }
    }

    texture_cache_pixels_dirty[texture_index][palette_handle] = false;
    texture_cache_palette_dirty[texture_index][palette_handle] = false;
    return true;
}

static SDL_Texture* create_texture_for_cache(const SDL_Surface* surface,
                                             int texture_index,
                                             int palette_handle,
                                             const SDL_Palette* palette,
                                             TextureCacheMode* texture_mode) {
    SDL_Texture* texture = NULL;
    *texture_mode = TEXTURE_CACHE_MODE_DEFAULT;

    if (should_use_experimental_indexed_texture(surface, palette)) {
        texture = create_experimental_indexed_texture(texture_index, palette_handle, surface, palette, texture_mode);
    } else {
        texture = SDL_CreateTextureFromSurface(_renderer, surface);
    }

    if (texture != NULL && *texture_mode == TEXTURE_CACHE_MODE_DEFAULT) {
        SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);
        SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
    }

    return texture;
}

// Colors

#define clut_shuf(x) (((x) & ~0x18) | ((((x) & 0x08) << 1) | (((x) & 0x10) >> 1)))

static void read_rgba32_color(Uint32 pixel, SDL_Color* color) {
    color->b = pixel & 0xFF;
    color->g = (pixel >> 8) & 0xFF;
    color->r = (pixel >> 16) & 0xFF;
    color->a = (pixel >> 24) & 0xFF;
}

static void read_rgba32_fcolor(Uint32 pixel, SDL_FColor* fcolor) {
    SDL_Color color;

    read_rgba32_color(pixel, &color);
    fcolor->r = (float)color.r / 255;
    fcolor->g = (float)color.g / 255;
    fcolor->b = (float)color.b / 255;
    fcolor->a = (float)color.a / 255;
}

static void read_rgba16_color(Uint16 pixel, SDL_Color* color) {
    color->r = (pixel & 0x1F) * 255 / 31;
    color->g = ((pixel >> 5) & 0x1F) * 255 / 31;
    color->b = ((pixel >> 10) & 0x1F) * 255 / 31;
    color->a = (pixel & 0x8000) ? 255 : 0;
}

static void read_color(void* pixels, int index, size_t color_size, SDL_Color* color) {
    switch (color_size) {
    case 2: {
        const Uint16* rgba16_colors = (Uint16*)pixels;
        read_rgba16_color(rgba16_colors[index], color);
        break;
    }

    case 4: {
        const Uint32* rgba32_colors = (Uint32*)pixels;
        read_rgba32_color(rgba32_colors[index], color);
        break;
    }
    }
}

#define LERP_FLOAT(a, b, x) ((a) * (1 - (x)) + (b) * (x))

static void lerp_fcolors(SDL_FColor* dest, const SDL_FColor* a, const SDL_FColor* b, float x) {
    dest->r = LERP_FLOAT(a->r, b->r, x);
    dest->g = LERP_FLOAT(a->g, b->g, x);
    dest->b = LERP_FLOAT(a->b, b->b, x);
    dest->a = LERP_FLOAT(a->a, b->a, x);
}

// Lifecycle

void SDLGameRenderer_Init(SDL_Renderer* renderer) {
    _renderer = renderer;
    cps3_canvas =
        SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, cps3_width, cps3_height);
    SDL_SetTextureScaleMode(cps3_canvas, SDL_SCALEMODE_NEAREST);
}

void SDLGameRenderer_SetDebugIndexedTexturePathEnabled(bool enabled) {
    debug_indexed_texture_path_enabled = enabled;

    if (enabled) {
        ensure_texture_handle_stats();
    }
}

void SDLGameRenderer_WriteDebugTextureHandleStats() {
    if (!should_record_texture_diagnostics() || texture_palette_handle_stats == NULL) {
        return;
    }

    DebugLog_Write("texture_palette_handle_stats.csv",
                   "texture_handle,palette_handle,set_texture_calls,cache_hits,cache_misses,miss_first_use,"
                   "miss_after_palette_unlock,miss_after_texture_unlock,miss_after_release,miss_unknown,"
                   "invalidated_by_palette_unlock,invalidated_by_texture_unlock,invalidated_by_release,"
                   "rgba_fallbacks\n");

    for (int texture_index = 0; texture_index < FL_TEXTURE_MAX; texture_index++) {
        for (int palette_handle = 0; palette_handle < FL_PALETTE_MAX + 1; palette_handle++) {
            const TexturePaletteHandleStats* stats =
                &texture_palette_handle_stats[texture_palette_stats_index(texture_index, palette_handle)];

            if (!pair_stats_has_data(stats)) {
                continue;
            }

            DebugLog_Printf("texture_palette_handle_stats.csv",
                            "%d,%d,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u\n",
                            texture_index + 1,
                            palette_handle,
                            stats->set_texture_calls,
                            stats->cache_hits,
                            stats->cache_misses,
                            stats->miss_first_use,
                            stats->miss_after_palette_unlock,
                            stats->miss_after_texture_unlock,
                            stats->miss_after_release,
                            stats->miss_unknown,
                            stats->invalidated_by_palette_unlock,
                            stats->invalidated_by_texture_unlock,
                            stats->invalidated_by_release,
                            stats->rgba_fallbacks);
        }
    }

    DebugLog_Write("texture_handle_stats.csv",
                   "texture_handle,set_texture_calls,cache_hits,cache_misses,miss_first_use,"
                   "miss_after_palette_unlock,miss_after_texture_unlock,miss_after_release,miss_unknown,"
                   "texture_unlocks,invalidated_by_palette_unlock,invalidated_by_texture_unlock,"
                   "invalidated_by_release,rgba_fallbacks\n");

    for (int texture_handle = 1; texture_handle < FL_TEXTURE_MAX + 1; texture_handle++) {
        const TextureHandleStats* stats = &texture_handle_stats[texture_handle];

        if (!texture_stats_has_data(stats)) {
            continue;
        }

        DebugLog_Printf("texture_handle_stats.csv",
                        "%d,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u\n",
                        texture_handle,
                        stats->set_texture_calls,
                        stats->cache_hits,
                        stats->cache_misses,
                        stats->miss_first_use,
                        stats->miss_after_palette_unlock,
                        stats->miss_after_texture_unlock,
                        stats->miss_after_release,
                        stats->miss_unknown,
                        stats->texture_unlocks,
                        stats->invalidated_by_palette_unlock,
                        stats->invalidated_by_texture_unlock,
                        stats->invalidated_by_release,
                        stats->rgba_fallbacks);
    }

    DebugLog_Write("palette_handle_stats.csv",
                   "palette_handle,set_texture_calls,cache_hits,cache_misses,miss_first_use,"
                   "miss_after_palette_unlock,miss_after_texture_unlock,miss_after_release,miss_unknown,"
                   "palette_unlocks,invalidated_by_palette_unlock,invalidated_by_texture_unlock,"
                   "invalidated_by_release,rgba_fallbacks\n");

    for (int palette_handle = 1; palette_handle < FL_PALETTE_MAX + 1; palette_handle++) {
        const PaletteHandleStats* stats = &palette_handle_stats[palette_handle];

        if (!palette_stats_has_data(stats)) {
            continue;
        }

        DebugLog_Printf("palette_handle_stats.csv",
                        "%d,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u\n",
                        palette_handle,
                        stats->set_texture_calls,
                        stats->cache_hits,
                        stats->cache_misses,
                        stats->miss_first_use,
                        stats->miss_after_palette_unlock,
                        stats->miss_after_texture_unlock,
                        stats->miss_after_release,
                        stats->miss_unknown,
                        stats->palette_unlocks,
                        stats->invalidated_by_palette_unlock,
                        stats->invalidated_by_texture_unlock,
                        stats->invalidated_by_release,
                        stats->rgba_fallbacks);
    }

    SDL_free(texture_palette_handle_stats);
    texture_palette_handle_stats = NULL;
}

void SDLGameRenderer_BeginFrame() {
    if (DebugLog_IsEnabled()) {
        texture_cache_miss_count = 0;
        reset_texture_diagnostics();
    }

    if (debug_indexed_texture_path_enabled) {
        SDL_zeroa(texture_cache_used_this_frame);
    }

    // Clear canvas
    const Uint8 r = (flPs2State.FrameClearColor >> 16) & 0xFF;
    const Uint8 g = (flPs2State.FrameClearColor >> 8) & 0xFF;
    const Uint8 b = flPs2State.FrameClearColor & 0xFF;
    const Uint8 a = flPs2State.FrameClearColor >> 24;

    if (a != SDL_ALPHA_TRANSPARENT) {
        SDL_SetRenderDrawColor(_renderer, r, g, b, a);
    } else {
        SDL_SetRenderDrawColor(_renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
    }

    SDL_SetRenderTarget(_renderer, cps3_canvas);
    SDL_RenderClear(_renderer);
}

void SDLGameRenderer_RenderFrame(SDLGameRendererStats* stats) {
    SDL_SetRenderTarget(_renderer, cps3_canvas);

    if (stats != NULL) {
        SDL_zero(*stats);
        stats->render_tasks = render_task_count;
        stats->texture_cache_misses = texture_cache_miss_count;
        stats->texture_cache_misses_first_use = texture_cache_miss_first_use_count;
        stats->texture_cache_misses_after_palette_unlock = texture_cache_miss_after_palette_unlock_count;
        stats->texture_cache_misses_after_texture_unlock = texture_cache_miss_after_texture_unlock_count;
        stats->texture_cache_misses_after_release = texture_cache_miss_after_release_count;
        stats->texture_cache_misses_unknown = texture_cache_miss_unknown_count;
        stats->palette_unlocks = palette_unlock_count;
        stats->texture_unlocks = texture_unlock_count;
        stats->palette_cache_invalidated_textures = palette_cache_invalidated_texture_count;
        stats->texture_cache_invalidated_textures = texture_cache_invalidated_texture_count;
        stats->release_cache_invalidated_textures = release_cache_invalidated_texture_count;
        stats->indexed_texture_updates = indexed_texture_update_count;
        stats->indexed_texture_update_pixels = indexed_texture_update_pixel_count;
        stats->indexed_texture_update_ms = indexed_texture_update_ms;
        stats->indexed_palette_updates = indexed_palette_update_count;
        stats->indexed_palette_update_ms = indexed_palette_update_ms;
        stats->indexed_texture_rgba_fallbacks = indexed_texture_rgba_fallback_count;
    }

    const Uint64 sort_start_ns = stats != NULL ? SDL_GetTicksNS() : 0;
    qsort(render_tasks, render_task_count, sizeof(RenderTask), compare_render_tasks);

    if (stats != NULL) {
        const Uint64 sort_end_ns = SDL_GetTicksNS();
        stats->render_sort_ms = elapsed_ms(sort_start_ns, sort_end_ns);
    }

    const Uint64 geometry_start_ns = stats != NULL ? SDL_GetTicksNS() : 0;
    for (int i = 0; i < render_task_count; i++) {
        const RenderTask* task = &render_tasks[i];
        const int indices[] = { 0, 1, 2, 1, 2, 3 };
        SDL_RenderGeometry(_renderer, task->texture, task->vertices, 4, indices, 6);

        if (stats != NULL) {
            stats->geometry_calls += 1;
        }
    }

    if (stats != NULL) {
        const Uint64 geometry_end_ns = SDL_GetTicksNS();
        stats->render_geometry_ms = elapsed_ms(geometry_start_ns, geometry_end_ns);
    }

    if (draw_rect_borders) {
        const SDL_FColor red = { .r = 1, .g = 0, .b = 0, .a = SDL_ALPHA_OPAQUE_FLOAT };
        const SDL_FColor green = { .r = 0, .g = 1, .b = 0, .a = SDL_ALPHA_OPAQUE_FLOAT };
        SDL_FColor border_color;

        for (int i = 0; i < render_task_count; i++) {
            const RenderTask* task = &render_tasks[i];
            const float x0 = task->vertices[0].position.x;
            const float y0 = task->vertices[0].position.y;
            const float x1 = task->vertices[3].position.x;
            const float y1 = task->vertices[3].position.y;
            const SDL_FRect border_rect = { .x = x0, .y = y0, .w = (x1 - x0), .h = (y1 - y0) };

            const float lerp_factor = (float)i / (float)(render_task_count - 1);
            lerp_fcolors(&border_color, &red, &green, lerp_factor);

            SDL_SetRenderDrawColorFloat(_renderer, border_color.r, border_color.g, border_color.b, border_color.a);
            SDL_RenderRect(_renderer, &border_rect);
        }
    }
}

void SDLGameRenderer_EndFrame() {
    destroy_textures();
    clear_render_tasks();
}

void SDLGameRenderer_UnlockPalette(unsigned int ph) {
    const int palette_handle = ph;

    if (is_valid_palette_handle(palette_handle)) {
        if (should_record_texture_diagnostics()) {
            palette_unlock_count += 1;
            ensure_texture_handle_stats();
            palette_handle_stats[palette_handle].palette_unlocks += 1;
        }

        active_palette_invalidation_reason = TEXTURE_CACHE_INVALIDATION_PALETTE_UNLOCK;
        SDLGameRenderer_DestroyPalette(palette_handle);
        active_palette_invalidation_reason = TEXTURE_CACHE_INVALIDATION_RELEASE;
        SDLGameRenderer_CreatePalette(ph << 16);
        mark_experimental_textures_dirty_for_palette(palette_handle);
    }
}

void SDLGameRenderer_UnlockTexture(unsigned int th) {
    const int texture_handle = th;

    if (is_valid_texture_handle(texture_handle)) {
        if (should_record_texture_diagnostics()) {
            texture_unlock_count += 1;
            ensure_texture_handle_stats();
            texture_handle_stats[texture_handle].texture_unlocks += 1;
        }

        active_texture_invalidation_reason = TEXTURE_CACHE_INVALIDATION_TEXTURE_UNLOCK;
        SDLGameRenderer_DestroyTexture(texture_handle);
        active_texture_invalidation_reason = TEXTURE_CACHE_INVALIDATION_RELEASE;
        SDLGameRenderer_CreateTexture(th);
        mark_experimental_textures_dirty_for_texture(texture_handle);
    }
}

void SDLGameRenderer_CreateTexture(unsigned int th) {
    const int texture_index = LO_16_BITS(th) - 1;
    const FLTexture* fl_texture = &flTexture[texture_index];
    const void* pixels = flPS2GetSystemBuffAdrs(fl_texture->mem_handle);
    SDL_PixelFormat pixel_format = SDL_PIXELFORMAT_UNKNOWN;
    int pitch = 0;

    if (surfaces[texture_index] != NULL) {
        fatal_error("Overwriting an existing texture");
    }

    switch (fl_texture->format) {
    case SCE_GS_PSMT8:
        pixel_format = SDL_PIXELFORMAT_INDEX8;
        pitch = fl_texture->width;
        break;

    case SCE_GS_PSMT4:
        pixel_format = SDL_PIXELFORMAT_INDEX4LSB;
        pitch = fl_texture->width / 2;
        break;

    case SCE_GS_PSMCT16:
        pixel_format = SDL_PIXELFORMAT_ABGR1555;
        pitch = fl_texture->width * 2;
        break;

    default:
        fatal_error("Unhandled pixel format: %d", fl_texture->format);
        break;
    }

    const SDL_Surface* surface =
        SDL_CreateSurfaceFrom(fl_texture->width, fl_texture->height, pixel_format, pixels, pitch);
    surfaces[texture_index] = surface;
}

void SDLGameRenderer_DestroyTexture(unsigned int texture_handle) {
    if (!is_valid_texture_handle(texture_handle)) {
        return;
    }

    const int texture_index = texture_handle - 1;

    for (int i = 0; i < FL_PALETTE_MAX + 1; i++) {
        SDL_Texture** texture_p = &texture_cache[texture_index][i];

        if (*texture_p == NULL) {
            continue;
        }

        texture_cache_last_invalidation[texture_index][i] = active_texture_invalidation_reason;
        record_texture_cache_invalidation(texture_index, i, active_texture_invalidation_reason);

        if (active_texture_invalidation_reason == TEXTURE_CACHE_INVALIDATION_TEXTURE_UNLOCK &&
            texture_cache_mode[texture_index][i] != TEXTURE_CACHE_MODE_DEFAULT &&
            !texture_cache_used_this_frame[texture_index][i]) {
            continue;
        }

        push_texture_to_destroy(*texture_p);
        *texture_p = NULL;
        texture_cache_mode[texture_index][i] = TEXTURE_CACHE_MODE_DEFAULT;
        texture_cache_pixels_dirty[texture_index][i] = false;
        texture_cache_palette_dirty[texture_index][i] = false;
    }

    SDL_DestroySurface(surfaces[texture_index]);
    surfaces[texture_index] = NULL;
}

void SDLGameRenderer_CreatePalette(unsigned int ph) {
    const int palette_index = HI_16_BITS(ph) - 1;
    const FLTexture* fl_palette = &flPalette[palette_index];
    const void* pixels = flPS2GetSystemBuffAdrs(fl_palette->mem_handle);
    const int color_count = fl_palette->width * fl_palette->height;
    SDL_Color colors[256];
    size_t color_size = 0;

    if (palettes[palette_index] != NULL) {
        fatal_error("Overwriting an existing palette");
    }

    switch (fl_palette->format) {
    case SCE_GS_PSMCT32:
        color_size = 4;
        break;

    case SCE_GS_PSMCT16:
        color_size = 2;
        break;

    default:
        fatal_error("Unhandled pixel format: %d", fl_palette->format);
        break;
    }

    switch (color_count) {
    case 16:
        for (int i = 0; i < 16; i++) {
            read_color(pixels, i, color_size, &colors[i]);
        }

        break;

    case 256:
        for (int i = 0; i < 256; i++) {
            const int color_index = clut_shuf(i);
            read_color(pixels, color_index, color_size, &colors[i]);
        }

        break;

    default:
        fatal_error("Unhandled palette dimensions: %dx%d", fl_palette->width, fl_palette->height);
        break;
    }

    SDL_Palette* palette = SDL_CreatePalette(color_count);
    SDL_SetPaletteColors(palette, colors, 0, color_count);
    palettes[palette_index] = palette;
}

void SDLGameRenderer_DestroyPalette(unsigned int palette_handle) {
    if (!is_valid_palette_handle(palette_handle)) {
        return;
    }

    const int palette_index = palette_handle - 1;

    for (int i = 0; i < FL_TEXTURE_MAX; i++) {
        SDL_Texture** texture_p = &texture_cache[i][palette_handle];

        if (*texture_p == NULL) {
            continue;
        }

        texture_cache_last_invalidation[i][palette_handle] = active_palette_invalidation_reason;
        record_texture_cache_invalidation(i, palette_handle, active_palette_invalidation_reason);

        if (active_palette_invalidation_reason == TEXTURE_CACHE_INVALIDATION_PALETTE_UNLOCK &&
            texture_cache_mode[i][palette_handle] != TEXTURE_CACHE_MODE_DEFAULT &&
            !texture_cache_used_this_frame[i][palette_handle]) {
            continue;
        }

        push_texture_to_destroy(*texture_p);
        *texture_p = NULL;
        texture_cache_mode[i][palette_handle] = TEXTURE_CACHE_MODE_DEFAULT;
        texture_cache_pixels_dirty[i][palette_handle] = false;
        texture_cache_palette_dirty[i][palette_handle] = false;
    }

    SDL_DestroyPalette(palettes[palette_index]);
    palettes[palette_index] = NULL;
}

void SDLGameRenderer_SetTexture(unsigned int th) {
    const int texture_handle = LO_16_BITS(th);
    const int texture_index = texture_handle - 1;
    const SDL_Surface* surface = surfaces[texture_handle - 1];
    const int palette_handle = HI_16_BITS(th);
    const SDL_Palette* palette = palette_handle != 0 ? palettes[palette_handle - 1] : NULL;

    if (dump_textures) {
        save_texture(surface, palette);
    }

    if (palette != NULL) {
        SDL_SetSurfacePalette(surface, palette);
    }

    SDL_Texture* texture = NULL;
    const SDL_Texture* cached_texture = texture_cache[texture_index][palette_handle];

    if (cached_texture != NULL) {
        texture = cached_texture;
        record_texture_cache_access(texture_index, palette_handle, true);

        if (texture_cache_mode[texture_index][palette_handle] != TEXTURE_CACHE_MODE_DEFAULT &&
            !update_cached_experimental_indexed_texture(texture, surface, palette, texture_index, palette_handle)) {
            push_texture_to_destroy(texture);
            texture = NULL;
            reset_texture_cache_entry(texture_index, palette_handle);
        }
    } else {
        TextureCacheMode texture_mode = TEXTURE_CACHE_MODE_DEFAULT;
        record_texture_cache_access(texture_index, palette_handle, false);
        texture = create_texture_for_cache(surface, texture_index, palette_handle, palette, &texture_mode);

        if (texture == NULL) {
            fatal_error("Failed to create texture");
        }

        texture_cache[texture_index][palette_handle] = texture;
        texture_cache_mode[texture_index][palette_handle] = texture_mode;
        texture_cache_pixels_dirty[texture_index][palette_handle] = false;
        texture_cache_palette_dirty[texture_index][palette_handle] = false;

        record_texture_cache_miss(texture_index, palette_handle);
    }

    if (texture == NULL) {
        TextureCacheMode texture_mode = TEXTURE_CACHE_MODE_DEFAULT;
        record_texture_cache_access(texture_index, palette_handle, false);
        texture = create_texture_for_cache(surface, texture_index, palette_handle, palette, &texture_mode);

        if (texture == NULL) {
            fatal_error("Failed to recreate dirty texture");
        }

        texture_cache[texture_index][palette_handle] = texture;
        texture_cache_mode[texture_index][palette_handle] = texture_mode;
        texture_cache_pixels_dirty[texture_index][palette_handle] = false;
        texture_cache_palette_dirty[texture_index][palette_handle] = false;
        record_texture_cache_miss(texture_index, palette_handle);
    }

    texture_cache_used_this_frame[texture_index][palette_handle] = true;
    push_texture(texture);
}

static void draw_quad(const SDLGameRenderer_Vertex* vertices, bool textured) {
    RenderTask task;
    task.index = render_task_count;
    task.texture = textured ? get_texture() : NULL;
    task.z = flPS2ConvScreenFZ(vertices[0].coord.z);

    SDL_zeroa(task.vertices);

    for (int i = 0; i < 4; i++) {
        task.vertices[i].position.x = vertices[i].coord.x;
        task.vertices[i].position.y = vertices[i].coord.y;

        if (textured) {
            task.vertices[i].tex_coord.x = vertices[i].tex_coord.s;
            task.vertices[i].tex_coord.y = vertices[i].tex_coord.t;
        }

        read_rgba32_fcolor(vertices[i].color, &task.vertices[i].color);
    }

    push_render_task(&task);
}

void SDLGameRenderer_DrawTexturedQuad(const Sprite* sprite, unsigned int color) {
    SDLGameRenderer_Vertex vertices[4];
    s32 i;

    for (i = 0; i < 4; i++) {
        vertices[i].coord.x = sprite->v[i].x;
        vertices[i].coord.y = sprite->v[i].y;
        vertices[i].coord.z = sprite->v[i].z;
        vertices[i].coord.w = 1.0f;
        vertices[i].color = color;
        vertices[i].tex_coord = sprite->t[i];
    }

    draw_quad(vertices, true);
}

void SDLGameRenderer_DrawSolidQuad(const Quad* sprite, unsigned int color) {
    SDLGameRenderer_Vertex vertices[4];
    s32 i;

    for (i = 0; i < 4; i++) {
        vertices[i].coord.x = sprite->v[i].x;
        vertices[i].coord.y = sprite->v[i].y;
        vertices[i].coord.z = sprite->v[i].z;
        vertices[i].coord.w = 1.0f;
        vertices[i].color = color;
    }

    draw_quad(vertices, false);
}

void SDLGameRenderer_DrawSprite(const Sprite* sprite, unsigned int color) {
    SDLGameRenderer_Vertex vertices[4];
    SDL_zeroa(vertices);

    for (int i = 0; i < 4; i++) {
        vertices[i].coord.z = sprite->v[0].z;
        vertices[i].color = color;
    }

    vertices[0].coord.x = sprite->v[0].x;
    vertices[0].coord.y = sprite->v[0].y;
    vertices[3].coord.x = sprite->v[3].x;
    vertices[3].coord.y = sprite->v[3].y;
    vertices[1].coord.x = vertices[3].coord.x;
    vertices[1].coord.y = vertices[0].coord.y;
    vertices[2].coord.x = vertices[0].coord.x;
    vertices[2].coord.y = vertices[3].coord.y;

    vertices[0].tex_coord = sprite->t[0];
    vertices[3].tex_coord = sprite->t[3];
    vertices[1].tex_coord.s = vertices[3].tex_coord.s;
    vertices[1].tex_coord.t = vertices[0].tex_coord.t;
    vertices[2].tex_coord.s = vertices[0].tex_coord.s;
    vertices[2].tex_coord.t = vertices[3].tex_coord.t;

    draw_quad(vertices, true);
}

void SDLGameRenderer_DrawSprite2(const Sprite2* sprite2) {
    Sprite sprite;
    SDL_zero(sprite);

    sprite.v[0] = sprite2->v[0];
    sprite.v[1].x = sprite2->v[1].x;
    sprite.v[1].y = sprite2->v[0].y;
    sprite.v[2].x = sprite2->v[0].x;
    sprite.v[2].y = sprite2->v[1].y;
    sprite.v[3] = sprite2->v[1];

    sprite.t[0] = sprite2->t[0];
    sprite.t[1].s = sprite2->t[1].s;
    sprite.t[1].t = sprite2->t[0].t;
    sprite.t[2].s = sprite2->t[0].s;
    sprite.t[2].t = sprite2->t[1].t;
    sprite.t[3] = sprite2->t[1];

    for (int i = 0; i < 4; i++) {
        sprite.v[i].z = sprite2->v[0].z;
    }

    SDLGameRenderer_DrawSprite(&sprite, sprite2->vertex_color);
}
