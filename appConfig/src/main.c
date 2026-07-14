#include "config_file.h"
#include "font.h"
#include "localization.h"
#include "ui.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <stdio.h>

#define UI_WIDTH 780
#define UI_HEIGHT 680

static void show_startup_error(const char* message, SDL_Window* window) {
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, Localize(TEXT_APP_NAME), message, window);
    fprintf(stderr, "%s: %s\n", Localize(TEXT_APP_NAME), message);
}

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    Localization_Init();
    SDL_SetAppMetadata(Localize(TEXT_APP_NAME), "1.0", NULL);

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        show_startup_error(SDL_GetError(), NULL);
        return 1;
    }

    char error[768] = { 0 };
    char* config_path = ConfigFile_CreatePath(error, sizeof(error));

    if (config_path == NULL) {
        show_startup_error(error, NULL);
        SDL_Quit();
        return 1;
    }

    ConfigSettings settings;

    if (!ConfigFile_Load(config_path, &settings, error, sizeof(error))) {
        show_startup_error(error, NULL);
        SDL_free(config_path);
        SDL_Quit();
        return 1;
    }

    SDL_Window* window = NULL;
    SDL_Renderer* renderer = NULL;

    if (!SDL_CreateWindowAndRenderer(Localize(TEXT_APP_NAME), UI_WIDTH, UI_HEIGHT, 0, &window, &renderer)) {
        show_startup_error(SDL_GetError(), NULL);
        SDL_free(config_path);
        SDL_Quit();
        return 1;
    }

    if (!SDL_SetRenderLogicalPresentation(renderer, UI_WIDTH, UI_HEIGHT, SDL_LOGICAL_PRESENTATION_LETTERBOX)) {
        show_startup_error(SDL_GetError(), window);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_free(config_path);
        SDL_Quit();
        return 1;
    }

    AppFont_Init(renderer);

    const bool ui_result = ConfigUI_Run(window, renderer, config_path, &settings);

    AppFont_Quit();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_free(config_path);
    SDL_Quit();
    return ui_result ? 0 : 1;
}
