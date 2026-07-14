#include "font.h"

#include <SDL3/SDL.h>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

#define FIRST_GLYPH 32
#define LAST_GLYPH 126
#define GLYPH_COUNT ((LAST_GLYPH - FIRST_GLYPH) + 1)
#define FALLBACK_FONT_SCALE 1.5f

typedef struct FontGlyph {
    SDL_FRect source;
    float advance;
} FontGlyph;

static SDL_Texture* font_texture = NULL;
static FontGlyph glyphs[GLYPH_COUNT];
static float font_line_height = SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE * FALLBACK_FONT_SCALE;

#if defined(_WIN32)

#define FONT_ATLAS_WIDTH 1024
#define FONT_ATLAS_HEIGHT 256
#define FONT_PIXEL_HEIGHT 18
#define GLYPH_PADDING 2

static void release_gdi_resources(HDC device_context,
                                  HBITMAP bitmap,
                                  HFONT font,
                                  HGDIOBJ previous_bitmap,
                                  HGDIOBJ previous_font) {
    if (device_context != NULL) {
        if (previous_font != NULL) {
            SelectObject(device_context, previous_font);
        }

        if (previous_bitmap != NULL) {
            SelectObject(device_context, previous_bitmap);
        }
    }

    if (font != NULL) {
        DeleteObject(font);
    }

    if (bitmap != NULL) {
        DeleteObject(bitmap);
    }

    if (device_context != NULL) {
        DeleteDC(device_context);
    }
}

static bool copy_gdi_bitmap_to_surface(const Uint8* bitmap_pixels, SDL_Surface* surface) {
    if (!SDL_LockSurface(surface)) {
        return false;
    }

    const size_t source_pitch = FONT_ATLAS_WIDTH * 4;

    for (int y = 0; y < FONT_ATLAS_HEIGHT; y++) {
        const Uint8* source_row = bitmap_pixels + ((size_t)y * source_pitch);
        Uint8* destination_row = (Uint8*)surface->pixels + ((size_t)y * (size_t)surface->pitch);

        for (int x = 0; x < FONT_ATLAS_WIDTH; x++) {
            const Uint8 blue = source_row[(x * 4) + 0];
            const Uint8 green = source_row[(x * 4) + 1];
            const Uint8 red = source_row[(x * 4) + 2];
            Uint8 coverage = red;

            if (green > coverage) {
                coverage = green;
            }

            if (blue > coverage) {
                coverage = blue;
            }

            destination_row[(x * 4) + 0] = 255;
            destination_row[(x * 4) + 1] = 255;
            destination_row[(x * 4) + 2] = 255;
            destination_row[(x * 4) + 3] = coverage;
        }
    }

    SDL_UnlockSurface(surface);
    return true;
}

static SDL_Texture* create_windows_font_texture(SDL_Renderer* renderer) {
    HDC device_context = CreateCompatibleDC(NULL);
    HBITMAP bitmap = NULL;
    HFONT font = NULL;
    HGDIOBJ previous_bitmap = NULL;
    HGDIOBJ previous_font = NULL;
    Uint8* bitmap_pixels = NULL;
    SDL_Surface* surface = NULL;
    SDL_Texture* texture = NULL;

    if (device_context == NULL) {
        return NULL;
    }

    BITMAPINFO bitmap_info;
    SDL_zero(bitmap_info);
    bitmap_info.bmiHeader.biSize = sizeof(bitmap_info.bmiHeader);
    bitmap_info.bmiHeader.biWidth = FONT_ATLAS_WIDTH;
    bitmap_info.bmiHeader.biHeight = -FONT_ATLAS_HEIGHT;
    bitmap_info.bmiHeader.biPlanes = 1;
    bitmap_info.bmiHeader.biBitCount = 32;
    bitmap_info.bmiHeader.biCompression = BI_RGB;

    bitmap = CreateDIBSection(device_context,
                              &bitmap_info,
                              DIB_RGB_COLORS,
                              (void**)&bitmap_pixels,
                              NULL,
                              0);

    if (bitmap == NULL || bitmap_pixels == NULL) {
        release_gdi_resources(device_context, bitmap, font, previous_bitmap, previous_font);
        return NULL;
    }

    font = CreateFontW(-FONT_PIXEL_HEIGHT,
                       0,
                       0,
                       0,
                       FW_NORMAL,
                       FALSE,
                       FALSE,
                       FALSE,
                       DEFAULT_CHARSET,
                       OUT_DEFAULT_PRECIS,
                       CLIP_DEFAULT_PRECIS,
                       ANTIALIASED_QUALITY,
                       DEFAULT_PITCH | FF_DONTCARE,
                       L"Segoe UI");

    if (font == NULL) {
        release_gdi_resources(device_context, bitmap, font, previous_bitmap, previous_font);
        return NULL;
    }

    previous_bitmap = SelectObject(device_context, bitmap);

    if (previous_bitmap == NULL || previous_bitmap == HGDI_ERROR) {
        previous_bitmap = NULL;
        release_gdi_resources(device_context, bitmap, font, previous_bitmap, previous_font);
        return NULL;
    }

    previous_font = SelectObject(device_context, font);

    if (previous_font == NULL || previous_font == HGDI_ERROR) {
        previous_font = NULL;
        release_gdi_resources(device_context, bitmap, font, previous_bitmap, previous_font);
        return NULL;
    }

    SetBkColor(device_context, RGB(0, 0, 0));
    SetTextColor(device_context, RGB(255, 255, 255));
    SetBkMode(device_context, TRANSPARENT);
    SetTextAlign(device_context, TA_LEFT | TA_TOP);
    PatBlt(device_context, 0, 0, FONT_ATLAS_WIDTH, FONT_ATLAS_HEIGHT, BLACKNESS);

    TEXTMETRICW metrics;

    if (!GetTextMetricsW(device_context, &metrics)) {
        release_gdi_resources(device_context, bitmap, font, previous_bitmap, previous_font);
        return NULL;
    }

    const int row_height = metrics.tmHeight + (GLYPH_PADDING * 2);
    int atlas_x = GLYPH_PADDING;
    int atlas_y = GLYPH_PADDING;

    for (int character = FIRST_GLYPH; character <= LAST_GLYPH; character++) {
        const wchar_t wide_character = (wchar_t)character;
        SIZE extent;

        if (!GetTextExtentPoint32W(device_context, &wide_character, 1, &extent)) {
            release_gdi_resources(device_context, bitmap, font, previous_bitmap, previous_font);
            return NULL;
        }

        const int advance = extent.cx > 0 ? extent.cx : 1;
        const int cell_width = advance + (GLYPH_PADDING * 2);

        if (atlas_x + cell_width >= FONT_ATLAS_WIDTH) {
            atlas_x = GLYPH_PADDING;
            atlas_y += row_height;
        }

        if (atlas_y + row_height >= FONT_ATLAS_HEIGHT) {
            release_gdi_resources(device_context, bitmap, font, previous_bitmap, previous_font);
            return NULL;
        }

        if (character != ' ' && !TextOutW(device_context, atlas_x + GLYPH_PADDING, atlas_y, &wide_character, 1)) {
            release_gdi_resources(device_context, bitmap, font, previous_bitmap, previous_font);
            return NULL;
        }

        FontGlyph* glyph = &glyphs[character - FIRST_GLYPH];
        glyph->source.x = (float)atlas_x;
        glyph->source.y = (float)atlas_y;
        glyph->source.w = (float)cell_width;
        glyph->source.h = (float)metrics.tmHeight;
        glyph->advance = (float)advance;
        atlas_x += cell_width;
    }

    surface = SDL_CreateSurface(FONT_ATLAS_WIDTH, FONT_ATLAS_HEIGHT, SDL_PIXELFORMAT_RGBA32);

    if (surface == NULL || !copy_gdi_bitmap_to_surface(bitmap_pixels, surface)) {
        SDL_DestroySurface(surface);
        release_gdi_resources(device_context, bitmap, font, previous_bitmap, previous_font);
        return NULL;
    }

    texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_DestroySurface(surface);
    release_gdi_resources(device_context, bitmap, font, previous_bitmap, previous_font);

    if (texture == NULL) {
        return NULL;
    }

    if (!SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND) ||
        !SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_LINEAR)) {
        SDL_DestroyTexture(texture);
        return NULL;
    }

    font_line_height = (float)metrics.tmHeight;
    return texture;
}

#endif

bool AppFont_Init(SDL_Renderer* renderer) {
    AppFont_Quit();

#if defined(_WIN32)
    if (renderer != NULL) {
        font_texture = create_windows_font_texture(renderer);
    }
#else
    (void)renderer;
#endif

    // SDL's built-in font remains available as a portable fallback.
    return true;
}

void AppFont_Quit(void) {
    if (font_texture != NULL) {
        SDL_DestroyTexture(font_texture);
        font_texture = NULL;
    }

    SDL_zeroa(glyphs);
    font_line_height = SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE * FALLBACK_FONT_SCALE;
}

static int glyph_index(unsigned char character) {
    if (character < FIRST_GLYPH || character > LAST_GLYPH) {
        return '?' - FIRST_GLYPH;
    }

    return character - FIRST_GLYPH;
}

float AppFont_MeasureText(const char* text, float scale) {
    if (text == NULL || scale <= 0.0f) {
        return 0.0f;
    }

    if (font_texture == NULL) {
        return (float)SDL_strlen(text) * SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE * FALLBACK_FONT_SCALE * scale;
    }

    float width = 0.0f;

    for (const unsigned char* character = (const unsigned char*)text; *character != '\0'; character++) {
        width += glyphs[glyph_index(*character)].advance * scale;
    }

    return width;
}

float AppFont_LineHeight(float scale) {
    return font_line_height * scale;
}

bool AppFont_Draw(SDL_Renderer* renderer,
                  float x,
                  float y,
                  const char* text,
                  SDL_Color color,
                  float scale) {
    if (renderer == NULL || text == NULL || scale <= 0.0f) {
        return false;
    }

    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);

    if (font_texture == NULL) {
        const float fallback_scale = scale * FALLBACK_FONT_SCALE;
        SDL_SetRenderScale(renderer, fallback_scale, fallback_scale);
        const bool rendered = SDL_RenderDebugText(renderer, x / fallback_scale, y / fallback_scale, text);
        SDL_SetRenderScale(renderer, 1.0f, 1.0f);
        return rendered;
    }

    if (!SDL_SetTextureColorMod(font_texture, color.r, color.g, color.b) ||
        !SDL_SetTextureAlphaMod(font_texture, color.a)) {
        return false;
    }

    float destination_x = x;

    for (const unsigned char* character = (const unsigned char*)text; *character != '\0'; character++) {
        const FontGlyph* glyph = &glyphs[glyph_index(*character)];
        const SDL_FRect destination = {
            .x = destination_x,
            .y = y,
            .w = glyph->source.w * scale,
            .h = glyph->source.h * scale,
        };

        if (!SDL_RenderTexture(renderer, font_texture, &glyph->source, &destination)) {
            return false;
        }

        destination_x += glyph->advance * scale;
    }

    return true;
}
