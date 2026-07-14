#ifndef SF3CONFIG_CONFIG_FILE_H
#define SF3CONFIG_CONFIG_FILE_H

#include <stdbool.h>
#include <stddef.h>

#define CONFIG_SCALE_MODE_LENGTH 32

typedef struct ConfigSettings {
    bool fullscreen;
    int window_width;
    int window_height;
    char scale_mode[CONFIG_SCALE_MODE_LENGTH];
    bool bezel;
    bool scanlines;
    int scanline_opacity;
    bool draw_players_above_hud;
} ConfigSettings;

void ConfigSettings_SetDefaults(ConfigSettings* settings);
void ConfigSettings_Normalize(ConfigSettings* settings);
bool ConfigSettings_Equals(const ConfigSettings* left, const ConfigSettings* right);

char* ConfigFile_CreatePath(char* error, size_t error_size);
bool ConfigFile_Load(const char* path, ConfigSettings* settings, char* error, size_t error_size);
bool ConfigFile_Save(const char* path, const ConfigSettings* settings, char* error, size_t error_size);

#endif
