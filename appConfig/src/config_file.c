#include "config_file.h"
#include "localization.h"

#include <SDL3/SDL.h>

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CONFIG_LINE_LENGTH 1024
#define WINDOW_WIDTH_MIN 384
#define WINDOW_WIDTH_MAX 7680
#define WINDOW_HEIGHT_MIN 288
#define WINDOW_HEIGHT_MAX 4320

typedef enum SettingIndex {
    SETTING_FULLSCREEN,
    SETTING_WINDOW_WIDTH,
    SETTING_WINDOW_HEIGHT,
    SETTING_SCALE_MODE,
    SETTING_BEZEL,
    SETTING_SCANLINES,
    SETTING_SCANLINE_OPACITY,
    SETTING_DRAW_PLAYERS_ABOVE_HUD,
    SETTING_COUNT,
} SettingIndex;

static const char* setting_keys[SETTING_COUNT] = {
    "fullscreen",
    "window-width",
    "window-height",
    "scale-mode",
    "bezel",
    "scanlines",
    "scanline-opacity",
    "draw-players-above-hud",
};

static bool is_valid_scale_mode(const char* value) {
    static const char* valid_modes[] = {
        "nearest",
        "linear",
        "soft-linear",
        "integer",
        "square-pixels",
    };

    for (size_t i = 0; i < SDL_arraysize(valid_modes); i++) {
        if (SDL_strcmp(value, valid_modes[i]) == 0) {
            return true;
        }
    }

    return false;
}

static int clamp_int(int value, int minimum, int maximum) {
    if (value < minimum) {
        return minimum;
    }

    if (value > maximum) {
        return maximum;
    }

    return value;
}

void ConfigSettings_SetDefaults(ConfigSettings* settings) {
    if (settings == NULL) {
        return;
    }

    settings->fullscreen = true;
    settings->window_width = 640;
    settings->window_height = 480;
    SDL_strlcpy(settings->scale_mode, "nearest", sizeof(settings->scale_mode));
    settings->bezel = false;
    settings->scanlines = false;
    settings->scanline_opacity = 20;
    settings->draw_players_above_hud = false;
}

void ConfigSettings_Normalize(ConfigSettings* settings) {
    if (settings == NULL) {
        return;
    }

    settings->window_width = clamp_int(settings->window_width, WINDOW_WIDTH_MIN, WINDOW_WIDTH_MAX);
    settings->window_height = clamp_int(settings->window_height, WINDOW_HEIGHT_MIN, WINDOW_HEIGHT_MAX);
    settings->scanline_opacity = clamp_int(settings->scanline_opacity, 0, 100);

    if (!is_valid_scale_mode(settings->scale_mode)) {
        SDL_strlcpy(settings->scale_mode, "nearest", sizeof(settings->scale_mode));
    }
}

bool ConfigSettings_Equals(const ConfigSettings* left, const ConfigSettings* right) {
    if (left == NULL || right == NULL) {
        return false;
    }

    return left->fullscreen == right->fullscreen && left->window_width == right->window_width &&
           left->window_height == right->window_height && SDL_strcmp(left->scale_mode, right->scale_mode) == 0 &&
           left->bezel == right->bezel && left->scanlines == right->scanlines &&
           left->scanline_opacity == right->scanline_opacity &&
           left->draw_players_above_hud == right->draw_players_above_hud;
}

char* ConfigFile_CreatePath(char* error, size_t error_size) {
    const char* base_path = SDL_GetBasePath();

    if (base_path == NULL) {
        SDL_snprintf(error, error_size, Localize(TEXT_ERR_LOCATE_APP_FOLDER), SDL_GetError());
        return NULL;
    }

    char* data_path = NULL;

#if defined(__APPLE__)
    if (SDL_asprintf(&data_path, "%s../../../3SX.app/Contents/MacOS/data", base_path) < 0 || data_path == NULL) {
#else
    if (SDL_asprintf(&data_path, "%sdata", base_path) < 0 || data_path == NULL) {
#endif
        SDL_snprintf(error, error_size, "%s", Localize(TEXT_ERR_CREATE_CONFIG_PATH));
        return NULL;
    }

    if (!SDL_CreateDirectory(data_path)) {
        SDL_snprintf(error, error_size, Localize(TEXT_ERR_ACCESS_FOLDER), data_path, SDL_GetError());
        SDL_free(data_path);
        return NULL;
    }

    char* config_path = NULL;

    if (SDL_asprintf(&config_path, "%s/config", data_path) < 0 || config_path == NULL) {
        SDL_snprintf(error, error_size, "%s", Localize(TEXT_ERR_CREATE_CONFIG_FILE_PATH));
    }

    SDL_free(data_path);
    return config_path;
}

static char* trim(char* text) {
    while (SDL_isspace((unsigned char)*text)) {
        text++;
    }

    char* end = text + SDL_strlen(text);

    while (end > text && SDL_isspace((unsigned char)end[-1])) {
        end--;
    }

    *end = '\0';
    return text;
}

static bool parse_line(const char* line, char* key, size_t key_size, char* value, size_t value_size) {
    char copy[CONFIG_LINE_LENGTH];
    SDL_strlcpy(copy, line, sizeof(copy));

    char* content = trim(copy);

    if (*content == '\0' || *content == '#') {
        return false;
    }

    char* separator = SDL_strchr(content, '=');

    if (separator == NULL) {
        return false;
    }

    *separator = '\0';
    const char* parsed_key = trim(content);
    const char* parsed_value = trim(separator + 1);

    if (*parsed_key == '\0' || *parsed_value == '\0') {
        return false;
    }

    SDL_strlcpy(key, parsed_key, key_size);
    SDL_strlcpy(value, parsed_value, value_size);
    return true;
}

static bool parse_bool(const char* value, bool* result) {
    if (SDL_strcmp(value, "true") == 0) {
        *result = true;
        return true;
    }

    if (SDL_strcmp(value, "false") == 0) {
        *result = false;
        return true;
    }

    return false;
}

static bool parse_int(const char* value, int* result) {
    errno = 0;
    char* end = NULL;
    const long parsed = strtol(value, &end, 10);

    if (errno != 0 || end == value || *trim(end) != '\0' || parsed < INT_MIN || parsed > INT_MAX) {
        return false;
    }

    *result = (int)parsed;
    return true;
}

static void apply_setting(ConfigSettings* settings, const char* key, const char* value) {
    if (SDL_strcmp(key, "fullscreen") == 0) {
        parse_bool(value, &settings->fullscreen);
    } else if (SDL_strcmp(key, "window-width") == 0) {
        parse_int(value, &settings->window_width);
    } else if (SDL_strcmp(key, "window-height") == 0) {
        parse_int(value, &settings->window_height);
    } else if (SDL_strcmp(key, "scale-mode") == 0) {
        if (is_valid_scale_mode(value)) {
            SDL_strlcpy(settings->scale_mode, value, sizeof(settings->scale_mode));
        }
    } else if (SDL_strcmp(key, "bezel") == 0) {
        parse_bool(value, &settings->bezel);
    } else if (SDL_strcmp(key, "scanlines") == 0) {
        parse_bool(value, &settings->scanlines);
    } else if (SDL_strcmp(key, "scanline-opacity") == 0) {
        parse_int(value, &settings->scanline_opacity);
    } else if (SDL_strcmp(key, "draw-players-above-hud") == 0) {
        parse_bool(value, &settings->draw_players_above_hud);
    }
}

bool ConfigFile_Load(const char* path, ConfigSettings* settings, char* error, size_t error_size) {
    if (path == NULL || settings == NULL) {
        SDL_snprintf(error, error_size, "%s", Localize(TEXT_ERR_INVALID_READ_PARAMS));
        return false;
    }

    ConfigSettings_SetDefaults(settings);
    FILE* file = fopen(path, "rb");

    if (file == NULL) {
        if (errno == ENOENT) {
            return true;
        }

        SDL_snprintf(error, error_size, Localize(TEXT_ERR_OPEN_FILE), path, strerror(errno));
        return false;
    }

    char line[CONFIG_LINE_LENGTH];

    while (fgets(line, sizeof(line), file) != NULL) {
        char key[128];
        char value[256];

        if (parse_line(line, key, sizeof(key), value, sizeof(value))) {
            apply_setting(settings, key, value);
        }
    }

    const bool read_ok = !ferror(file);
    fclose(file);

    if (!read_ok) {
        SDL_snprintf(error, error_size, Localize(TEXT_ERR_READ_FILE), path);
        return false;
    }

    ConfigSettings_Normalize(settings);
    return true;
}

static int find_setting_index(const char* key) {
    for (int i = 0; i < SETTING_COUNT; i++) {
        if (SDL_strcmp(key, setting_keys[i]) == 0) {
            return i;
        }
    }

    return -1;
}

static bool write_setting(FILE* file, SettingIndex setting, const ConfigSettings* settings) {
    int result = -1;

    switch (setting) {
    case SETTING_FULLSCREEN:
        result = fprintf(file, "fullscreen = %s\n", settings->fullscreen ? "true" : "false");
        break;
    case SETTING_WINDOW_WIDTH:
        result = fprintf(file, "window-width = %d\n", settings->window_width);
        break;
    case SETTING_WINDOW_HEIGHT:
        result = fprintf(file, "window-height = %d\n", settings->window_height);
        break;
    case SETTING_SCALE_MODE:
        result = fprintf(file, "scale-mode = %s\n", settings->scale_mode);
        break;
    case SETTING_BEZEL:
        result = fprintf(file, "bezel = %s\n", settings->bezel ? "true" : "false");
        break;
    case SETTING_SCANLINES:
        result = fprintf(file, "scanlines = %s\n", settings->scanlines ? "true" : "false");
        break;
    case SETTING_SCANLINE_OPACITY:
        result = fprintf(file, "scanline-opacity = %d\n", settings->scanline_opacity);
        break;
    case SETTING_DRAW_PLAYERS_ABOVE_HUD:
        result = fprintf(file, "draw-players-above-hud = %s\n", settings->draw_players_above_hud ? "true" : "false");
        break;
    case SETTING_COUNT:
        return false;
    }

    return result >= 0;
}

static bool close_written_file(FILE* file) {
    const bool flushed = fflush(file) == 0;
    const bool closed = fclose(file) == 0;
    return flushed && closed;
}

bool ConfigFile_Save(const char* path, const ConfigSettings* source, char* error, size_t error_size) {
    if (path == NULL || source == NULL) {
        SDL_snprintf(error, error_size, "%s", Localize(TEXT_ERR_INVALID_WRITE_PARAMS));
        return false;
    }

    ConfigSettings settings = *source;
    ConfigSettings_Normalize(&settings);

    FILE* input = fopen(path, "rb");

    if (input == NULL && errno != ENOENT) {
        SDL_snprintf(error, error_size, Localize(TEXT_ERR_READ_FOR_SAVE), path, strerror(errno));
        return false;
    }

    char* temporary_path = NULL;

    if (SDL_asprintf(&temporary_path, "%s.tmp-%08x", path, SDL_rand_bits()) < 0 || temporary_path == NULL) {
        if (input != NULL) {
            fclose(input);
        }
        SDL_snprintf(error, error_size, "%s", Localize(TEXT_ERR_CREATE_TEMP_PATH));
        return false;
    }

    FILE* output = fopen(temporary_path, "wb");

    if (output == NULL) {
        if (input != NULL) {
            fclose(input);
        }
        SDL_snprintf(error, error_size, Localize(TEXT_ERR_WRITE_FILE), temporary_path, strerror(errno));
        SDL_free(temporary_path);
        return false;
    }

    bool written[SETTING_COUNT] = { false };
    bool write_ok = true;

    if (input == NULL) {
        write_ok = fprintf(output, "%s\n\n", Localize(TEXT_CONFIG_HEADER)) >= 0;
    } else {
        char line[CONFIG_LINE_LENGTH];

        while (write_ok && fgets(line, sizeof(line), input) != NULL) {
            char key[128];
            char value[256];
            const bool parsed = parse_line(line, key, sizeof(key), value, sizeof(value));
            const int setting_index = parsed ? find_setting_index(key) : -1;

            if (setting_index < 0) {
                write_ok = fputs(line, output) >= 0;
            } else if (!written[setting_index]) {
                write_ok = write_setting(output, (SettingIndex)setting_index, &settings);
                written[setting_index] = write_ok;
            }
        }

        if (ferror(input)) {
            write_ok = false;
        }

        fclose(input);

        if (write_ok) {
            write_ok = fputc('\n', output) != EOF;
        }
    }

    for (int i = 0; i < SETTING_COUNT && write_ok; i++) {
        if (!written[i]) {
            write_ok = write_setting(output, (SettingIndex)i, &settings);
        }
    }

    if (!close_written_file(output)) {
        write_ok = false;
    }

    if (!write_ok) {
        SDL_RemovePath(temporary_path);
        SDL_snprintf(error, error_size, Localize(TEXT_ERR_FINISH_WRITE), path);
        SDL_free(temporary_path);
        return false;
    }

    if (!SDL_RenamePath(temporary_path, path)) {
        SDL_snprintf(error, error_size, Localize(TEXT_ERR_REPLACE_FILE), path, SDL_GetError());
        SDL_RemovePath(temporary_path);
        SDL_free(temporary_path);
        return false;
    }

    SDL_free(temporary_path);
    return true;
}
