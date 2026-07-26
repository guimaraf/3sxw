#include "ui.h"
#include "font.h"
#include "language.h"

#include <SDL3/SDL.h>

#define UI_WIDTH 780
#define UI_HEIGHT 730
#define ROW_X 40
#define ROW_WIDTH 700
#define ROW_START_Y 150
#define ROW_HEIGHT 52
#define BUTTON_Y 620
#define BUTTON_WIDTH 210
#define BUTTON_HEIGHT 48
#define LANGUAGE_BUTTON_Y 18
#define LANGUAGE_SELECTOR_WIDTH 124
#define LANGUAGE_SELECTOR_HEIGHT 30
#define LANGUAGE_SELECTOR_ARROW_WIDTH 26
#define LANGUAGE_BUTTON_RIGHT 40

typedef enum Control {
    CONTROL_FULLSCREEN,
    CONTROL_WINDOW_WIDTH,
    CONTROL_WINDOW_HEIGHT,
    CONTROL_SCALE_MODE,
    CONTROL_BEZEL,
    CONTROL_SCANLINES,
    CONTROL_SCANLINE_OPACITY,
    CONTROL_DRAW_PLAYERS_ABOVE_HUD,
    CONTROL_SAVE,
    CONTROL_DEFAULTS,
    CONTROL_CANCEL,
    CONTROL_COUNT,
    CONTROL_NONE = -1,
} Control;

typedef struct UIState {
    SDL_Window* window;
    SDL_Renderer* renderer;
    const char* config_path;
    ConfigSettings* settings;
    ConfigSettings saved_settings;
    Control focused;
    Control hovered;
    int hovered_language_direction;
    bool running;
    char status[160];
} UIState;

static const SDL_Color COLOR_BACKGROUND = { 18, 20, 28, 255 };
static const SDL_Color COLOR_PANEL = { 31, 35, 47, 255 };
static const SDL_Color COLOR_PANEL_HOVER = { 39, 45, 60, 255 };
static const SDL_Color COLOR_PANEL_FOCUS = { 48, 57, 76, 255 };
static const SDL_Color COLOR_BORDER = { 91, 105, 132, 255 };
static const SDL_Color COLOR_ACCENT = { 242, 167, 50, 255 };
static const SDL_Color COLOR_TEXT = { 240, 242, 247, 255 };
static const SDL_Color COLOR_MUTED = { 156, 166, 187, 255 };
static const SDL_Color COLOR_DISABLED = { 91, 98, 113, 255 };
static const SDL_Color COLOR_SUCCESS = { 105, 209, 140, 255 };

static const char* scale_modes[] = {
    "nearest",
    "linear",
    "soft-linear",
    "integer",
    "square-pixels",
};

static void set_color(SDL_Renderer* renderer, SDL_Color color) {
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
}

static void fill_rect(SDL_Renderer* renderer, const SDL_FRect* rect, SDL_Color color) {
    set_color(renderer, color);
    SDL_RenderFillRect(renderer, rect);
}

static void draw_rect(SDL_Renderer* renderer, const SDL_FRect* rect, SDL_Color color) {
    set_color(renderer, color);
    SDL_RenderRect(renderer, rect);
}

static void draw_text(SDL_Renderer* renderer, float x, float y, const char* text, SDL_Color color, float scale) {
    AppFont_Draw(renderer, x, y, text, color, scale);
}

static void draw_centered_text(SDL_Renderer* renderer,
                               const SDL_FRect* rect,
                               const char* text,
                               SDL_Color color,
                               float scale) {
    const float text_width = AppFont_MeasureText(text, scale);
    const float text_height = AppFont_LineHeight(scale);
    draw_text(renderer,
              rect->x + ((rect->w - text_width) / 2.0f),
              rect->y + ((rect->h - text_height) / 2.0f),
              text,
              color,
              scale);
}

static void draw_wrapped_text(SDL_Renderer* renderer,
                              float x,
                              float y,
                              float maximum_width,
                              const char* text,
                              SDL_Color color,
                              float scale) {
    char line[512] = { 0 };
    size_t line_length = 0;
    const char* word = text;

    while (word != NULL && *word != '\0') {
        while (*word == ' ') {
            word++;
        }

        if (*word == '\0') {
            break;
        }

        const char* word_end = word;

        while (*word_end != '\0' && *word_end != ' ') {
            word_end++;
        }

        const size_t word_length = (size_t)(word_end - word);
        const size_t separator_length = line_length > 0 ? 1 : 0;

        if (line_length + separator_length + word_length >= sizeof(line)) {
            break;
        }

        char candidate[512];
        SDL_memcpy(candidate, line, line_length);

        if (separator_length > 0) {
            candidate[line_length] = ' ';
        }

        SDL_memcpy(candidate + line_length + separator_length, word, word_length);
        candidate[line_length + separator_length + word_length] = '\0';

        if (line_length > 0 && AppFont_MeasureText(candidate, scale) > maximum_width) {
            draw_text(renderer, x, y, line, color, scale);
            y += AppFont_LineHeight(scale);
            line_length = 0;
            SDL_memcpy(line, word, word_length);
            line[word_length] = '\0';
            line_length = word_length;
        } else {
            SDL_memcpy(line, candidate, line_length + separator_length + word_length + 1);
            line_length += separator_length + word_length;
        }

        word = word_end;
    }

    if (line_length > 0) {
        draw_text(renderer, x, y, line, color, scale);
    }
}

static SDL_FRect control_rect(Control control) {
    if (control >= CONTROL_FULLSCREEN && control <= CONTROL_DRAW_PLAYERS_ABOVE_HUD) {
        return (SDL_FRect){
            .x = ROW_X,
            .y = ROW_START_Y + ((float)control * ROW_HEIGHT),
            .w = ROW_WIDTH,
            .h = ROW_HEIGHT - 4,
        };
    }

    const int button_index = (int)control - (int)CONTROL_SAVE;
    return (SDL_FRect){
        .x = ROW_X + (button_index * (BUTTON_WIDTH + 35)),
        .y = BUTTON_Y,
        .w = BUTTON_WIDTH,
        .h = BUTTON_HEIGHT,
    };
}

static bool point_in_rect(float x, float y, const SDL_FRect* rect) {
    return x >= rect->x && x < (rect->x + rect->w) && y >= rect->y && y < (rect->y + rect->h);
}

static SDL_FRect language_selector_rect(void) {
    return (SDL_FRect){
        .x = UI_WIDTH - LANGUAGE_BUTTON_RIGHT - LANGUAGE_SELECTOR_WIDTH,
        .y = LANGUAGE_BUTTON_Y,
        .w = LANGUAGE_SELECTOR_WIDTH,
        .h = LANGUAGE_SELECTOR_HEIGHT,
    };
}

static int language_direction_at(float x, float y) {
    const SDL_FRect rect = language_selector_rect();

    if (!point_in_rect(x, y, &rect)) {
        return 0;
    }

    if (x < rect.x + LANGUAGE_SELECTOR_ARROW_WIDTH) {
        return -1;
    }

    if (x >= rect.x + rect.w - LANGUAGE_SELECTOR_ARROW_WIDTH) {
        return 1;
    }

    return 0;
}

static Control control_at(float x, float y) {
    for (int i = 0; i < CONTROL_COUNT; i++) {
        const SDL_FRect rect = control_rect((Control)i);

        if (point_in_rect(x, y, &rect)) {
            return (Control)i;
        }
    }

    return CONTROL_NONE;
}

static bool control_is_enabled(const UIState* state, Control control) {
    if ((control == CONTROL_WINDOW_WIDTH || control == CONTROL_WINDOW_HEIGHT) && state->settings->fullscreen) {
        return false;
    }

    if (control == CONTROL_SCANLINE_OPACITY && !state->settings->scanlines) {
        return false;
    }

    return true;
}

static Control next_enabled_control(const UIState* state, Control start, int direction) {
    int candidate = (int)start;

    for (int i = 0; i < CONTROL_COUNT; i++) {
        candidate = (candidate + direction + CONTROL_COUNT) % CONTROL_COUNT;

        if (control_is_enabled(state, (Control)candidate)) {
            return (Control)candidate;
        }
    }

    return start;
}

static int scale_mode_index(const char* mode) {
    for (size_t i = 0; i < SDL_arraysize(scale_modes); i++) {
        if (SDL_strcmp(mode, scale_modes[i]) == 0) {
            return (int)i;
        }
    }

    return 0;
}

static void set_dirty_status(UIState* state) {
    SDL_strlcpy(state->status, Language_Get(TEXT_STATUS_DIRTY), sizeof(state->status));
}

static void change_language(UIState* state, int direction) {
    const int language_count = Language_GetCount();
    const int next_language = ((int)Language_GetCurrent() + direction + language_count) % language_count;

    Language_SetCurrent((AppLanguage)next_language);
    SDL_SetWindowTitle(state->window, Language_Get(TEXT_APP_NAME));
    SDL_strlcpy(state->status, Language_Get(TEXT_STATUS_LANGUAGE_CHANGED), sizeof(state->status));
}

static bool confirm_discard(UIState* state) {
    if (ConfigSettings_Equals(state->settings, &state->saved_settings)) {
        return true;
    }

    const SDL_MessageBoxButtonData buttons[] = {
        { SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT | SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT,
          0,
          Language_Get(TEXT_CONTINUE_EDITING) },
        { 0, 1, Language_Get(TEXT_DISCARD) },
    };
    const SDL_MessageBoxData message = {
        .flags = SDL_MESSAGEBOX_WARNING | SDL_MESSAGEBOX_BUTTONS_LEFT_TO_RIGHT,
        .window = state->window,
        .title = Language_Get(TEXT_UNSAVED_TITLE),
        .message = Language_Get(TEXT_UNSAVED_MESSAGE),
        .numbuttons = (int)SDL_arraysize(buttons),
        .buttons = buttons,
        .colorScheme = NULL,
    };
    int selected_button = 0;

    if (!SDL_ShowMessageBox(&message, &selected_button)) {
        return false;
    }

    return selected_button == 1;
}

static void save_settings(UIState* state) {
    char error[768] = { 0 };
    ConfigSettings_Normalize(state->settings);

    if (!ConfigFile_Save(state->config_path, state->settings, error, sizeof(error))) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, Language_Get(TEXT_SAVE_ERROR_TITLE), error, state->window);
        SDL_strlcpy(state->status, Language_Get(TEXT_STATUS_SAVE_FAILED), sizeof(state->status));
        return;
    }

    state->saved_settings = *state->settings;
    SDL_strlcpy(state->status, Language_Get(TEXT_STATUS_SAVED), sizeof(state->status));
}

static void adjust_control(UIState* state, Control control, int direction, bool toggle) {
    if (!control_is_enabled(state, control)) {
        return;
    }

    switch (control) {
    case CONTROL_FULLSCREEN:
        state->settings->fullscreen = toggle ? !state->settings->fullscreen : direction > 0;
        break;
    case CONTROL_WINDOW_WIDTH:
        state->settings->window_width += direction * 16;
        break;
    case CONTROL_WINDOW_HEIGHT:
        state->settings->window_height += direction * 9;
        break;
    case CONTROL_SCALE_MODE: {
        const int count = (int)SDL_arraysize(scale_modes);
        const int index = (scale_mode_index(state->settings->scale_mode) + direction + count) % count;
        SDL_strlcpy(state->settings->scale_mode, scale_modes[index], sizeof(state->settings->scale_mode));
        break;
    }
    case CONTROL_BEZEL:
        state->settings->bezel = toggle ? !state->settings->bezel : direction > 0;
        break;
    case CONTROL_SCANLINES:
        state->settings->scanlines = toggle ? !state->settings->scanlines : direction > 0;
        break;
    case CONTROL_SCANLINE_OPACITY:
        state->settings->scanline_opacity += direction * 5;
        break;
    case CONTROL_DRAW_PLAYERS_ABOVE_HUD:
        state->settings->draw_players_above_hud =
            toggle ? !state->settings->draw_players_above_hud : direction > 0;
        break;
    case CONTROL_SAVE:
        save_settings(state);
        return;
    case CONTROL_DEFAULTS:
        ConfigSettings_SetDefaults(state->settings);
        break;
    case CONTROL_CANCEL:
        if (confirm_discard(state)) {
            state->running = false;
        }
        return;
    case CONTROL_COUNT:
    case CONTROL_NONE:
        return;
    }

    ConfigSettings_Normalize(state->settings);
    set_dirty_status(state);
}

static void adjust_control_fine(UIState* state, Control control, int direction) {
    if (!control_is_enabled(state, control)) {
        return;
    }

    switch (control) {
    case CONTROL_WINDOW_WIDTH:
        state->settings->window_width += direction;
        break;
    case CONTROL_WINDOW_HEIGHT:
        state->settings->window_height += direction;
        break;
    case CONTROL_SCANLINE_OPACITY:
        state->settings->scanline_opacity += direction;
        break;
    default:
        adjust_control(state, control, direction, false);
        return;
    }

    ConfigSettings_Normalize(state->settings);
    set_dirty_status(state);
}

static void handle_mouse_button(UIState* state, const SDL_MouseButtonEvent* event) {
    if (event->button != SDL_BUTTON_LEFT || !event->down) {
        return;
    }

    const int language_direction = language_direction_at(event->x, event->y);

    if (language_direction != 0) {
        change_language(state, language_direction);
        return;
    }

    const Control control = control_at(event->x, event->y);

    if (control == CONTROL_NONE || !control_is_enabled(state, control)) {
        return;
    }

    state->focused = control;
    const SDL_FRect rect = control_rect(control);
    int direction = 1;

    if (control == CONTROL_WINDOW_WIDTH || control == CONTROL_WINDOW_HEIGHT || control == CONTROL_SCALE_MODE ||
        control == CONTROL_SCANLINE_OPACITY) {
        const float adjuster_start = rect.x + rect.w - 205.0f;

        if (event->x < adjuster_start) {
            return;
        }

        direction = event->x < (adjuster_start + 50.0f) ? -1 : 1;
    }

    const bool is_toggle = control == CONTROL_FULLSCREEN || control == CONTROL_BEZEL ||
                           control == CONTROL_SCANLINES || control == CONTROL_DRAW_PLAYERS_ABOVE_HUD;
    adjust_control(state, control, direction, is_toggle);
}

static void handle_key(UIState* state, const SDL_KeyboardEvent* event) {
    if (!event->down) {
        return;
    }

    switch (event->key) {
    case SDLK_ESCAPE:
        if (!event->repeat && confirm_discard(state)) {
            state->running = false;
        }
        break;
    case SDLK_TAB: {
        if (event->repeat) {
            break;
        }
        const int direction = (event->mod & SDL_KMOD_SHIFT) ? -1 : 1;
        state->focused = next_enabled_control(state, state->focused, direction);
        break;
    }
    case SDLK_UP:
        state->focused = next_enabled_control(state, state->focused, -1);
        break;
    case SDLK_DOWN:
        state->focused = next_enabled_control(state, state->focused, 1);
        break;
    case SDLK_LEFT:
        if (state->focused >= CONTROL_SAVE) {
            state->focused = next_enabled_control(state, state->focused, -1);
        } else if (event->mod & SDL_KMOD_SHIFT) {
            adjust_control_fine(state, state->focused, -1);
        } else {
            adjust_control(state, state->focused, -1, false);
        }
        break;
    case SDLK_RIGHT:
        if (state->focused >= CONTROL_SAVE) {
            state->focused = next_enabled_control(state, state->focused, 1);
        } else if (event->mod & SDL_KMOD_SHIFT) {
            adjust_control_fine(state, state->focused, 1);
        } else {
            adjust_control(state, state->focused, 1, false);
        }
        break;
    case SDLK_RETURN:
    case SDLK_SPACE:
        if (!event->repeat) {
            adjust_control(state, state->focused, 1, true);
        }
        break;
    default:
        break;
    }
}

static void handle_event(UIState* state, SDL_Event* event) {
    if (event->type == SDL_EVENT_MOUSE_MOTION || event->type == SDL_EVENT_MOUSE_BUTTON_DOWN ||
        event->type == SDL_EVENT_MOUSE_BUTTON_UP) {
        SDL_ConvertEventToRenderCoordinates(state->renderer, event);
    }

    switch (event->type) {
    case SDL_EVENT_QUIT:
    case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
        if (confirm_discard(state)) {
            state->running = false;
        }
        break;
    case SDL_EVENT_MOUSE_MOTION:
        state->hovered = control_at(event->motion.x, event->motion.y);
        state->hovered_language_direction = language_direction_at(event->motion.x, event->motion.y);
        break;
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
        handle_mouse_button(state, &event->button);
        break;
    case SDL_EVENT_KEY_DOWN:
        handle_key(state, &event->key);
        break;
    default:
        break;
    }
}

static void draw_checkbox(UIState* state, const SDL_FRect* row, bool checked, bool enabled) {
    const SDL_FRect box = { row->x + row->w - 52.0f, row->y + 12.0f, 24.0f, 24.0f };
    fill_rect(state->renderer, &box, checked && enabled ? COLOR_ACCENT : COLOR_PANEL);
    draw_rect(state->renderer, &box, enabled ? COLOR_BORDER : COLOR_DISABLED);

    if (checked) {
        draw_centered_text(state->renderer, &box, "X", enabled ? COLOR_BACKGROUND : COLOR_DISABLED, 1.0f);
    }
}

static void draw_adjuster(UIState* state, const SDL_FRect* row, const char* value, bool enabled) {
    const SDL_Color text_color = enabled ? COLOR_TEXT : COLOR_DISABLED;
    const SDL_FRect minus = { row->x + row->w - 205.0f, row->y + 8.0f, 42.0f, 32.0f };
    const SDL_FRect current = { row->x + row->w - 155.0f, row->y + 8.0f, 105.0f, 32.0f };
    const SDL_FRect plus = { row->x + row->w - 42.0f, row->y + 8.0f, 42.0f, 32.0f };

    fill_rect(state->renderer, &minus, COLOR_PANEL);
    fill_rect(state->renderer, &current, COLOR_PANEL);
    fill_rect(state->renderer, &plus, COLOR_PANEL);
    draw_rect(state->renderer, &minus, enabled ? COLOR_BORDER : COLOR_DISABLED);
    draw_rect(state->renderer, &current, enabled ? COLOR_BORDER : COLOR_DISABLED);
    draw_rect(state->renderer, &plus, enabled ? COLOR_BORDER : COLOR_DISABLED);
    draw_centered_text(state->renderer, &minus, "-", text_color, 1.5f);
    draw_centered_text(state->renderer, &current, value, text_color, 1.0f);
    draw_centered_text(state->renderer, &plus, "+", text_color, 1.5f);
}

static void draw_row(UIState* state, Control control, const char* label) {
    const SDL_FRect row = control_rect(control);
    const bool enabled = control_is_enabled(state, control);
    SDL_Color background = COLOR_PANEL;

    if (state->focused == control) {
        background = COLOR_PANEL_FOCUS;
    } else if (state->hovered == control) {
        background = COLOR_PANEL_HOVER;
    }

    fill_rect(state->renderer, &row, background);
    draw_rect(state->renderer, &row, state->focused == control ? COLOR_ACCENT : COLOR_BORDER);
    draw_text(state->renderer, row.x + 18.0f, row.y + 16.0f, label, enabled ? COLOR_TEXT : COLOR_DISABLED, 1.25f);

    char value[64];

    switch (control) {
    case CONTROL_FULLSCREEN:
        draw_checkbox(state, &row, state->settings->fullscreen, enabled);
        break;
    case CONTROL_WINDOW_WIDTH:
        SDL_snprintf(value, sizeof(value), "%d", state->settings->window_width);
        draw_adjuster(state, &row, value, enabled);
        break;
    case CONTROL_WINDOW_HEIGHT:
        SDL_snprintf(value, sizeof(value), "%d", state->settings->window_height);
        draw_adjuster(state, &row, value, enabled);
        break;
    case CONTROL_SCALE_MODE:
        draw_adjuster(state, &row, state->settings->scale_mode, enabled);
        break;
    case CONTROL_BEZEL:
        draw_checkbox(state, &row, state->settings->bezel, enabled);
        break;
    case CONTROL_SCANLINES:
        draw_checkbox(state, &row, state->settings->scanlines, enabled);
        break;
    case CONTROL_SCANLINE_OPACITY:
        SDL_snprintf(value, sizeof(value), "%d%%", state->settings->scanline_opacity);
        draw_adjuster(state, &row, value, enabled);
        break;
    case CONTROL_DRAW_PLAYERS_ABOVE_HUD:
        draw_checkbox(state, &row, state->settings->draw_players_above_hud, enabled);
        break;
    default:
        break;
    }
}

static void draw_button(UIState* state, Control control, const char* label) {
    const SDL_FRect rect = control_rect(control);
    SDL_Color background = COLOR_PANEL;

    if (state->focused == control) {
        background = COLOR_ACCENT;
    } else if (state->hovered == control) {
        background = COLOR_PANEL_HOVER;
    }

    fill_rect(state->renderer, &rect, background);
    draw_rect(state->renderer, &rect, state->focused == control ? COLOR_ACCENT : COLOR_BORDER);
    float text_scale = 1.25f;

    while (text_scale > 0.75f && AppFont_MeasureText(label, text_scale) > rect.w - 20.0f) {
        text_scale -= 0.05f;
    }

    draw_centered_text(state->renderer,
                       &rect,
                       label,
                       state->focused == control ? COLOR_BACKGROUND : COLOR_TEXT,
                       text_scale);
}

static void draw_language_selector(UIState* state) {
    const SDL_FRect rect = language_selector_rect();
    const SDL_FRect left_arrow = {
        .x = rect.x,
        .y = rect.y,
        .w = LANGUAGE_SELECTOR_ARROW_WIDTH,
        .h = rect.h,
    };
    const SDL_FRect right_arrow = {
        .x = rect.x + rect.w - LANGUAGE_SELECTOR_ARROW_WIDTH,
        .y = rect.y,
        .w = LANGUAGE_SELECTOR_ARROW_WIDTH,
        .h = rect.h,
    };
    const SDL_FRect language_code = {
        .x = left_arrow.x + left_arrow.w,
        .y = rect.y,
        .w = rect.w - (LANGUAGE_SELECTOR_ARROW_WIDTH * 2),
        .h = rect.h,
    };

    fill_rect(state->renderer, &rect, COLOR_PANEL);
    draw_rect(state->renderer, &rect, COLOR_BORDER);

    if (state->hovered_language_direction < 0) {
        fill_rect(state->renderer, &left_arrow, COLOR_PANEL_HOVER);
    } else if (state->hovered_language_direction > 0) {
        fill_rect(state->renderer, &right_arrow, COLOR_PANEL_HOVER);
    }

    draw_centered_text(state->renderer, &left_arrow, "<", COLOR_TEXT, 1.0f);
    draw_centered_text(state->renderer, &language_code, Language_GetCode(Language_GetCurrent()), COLOR_ACCENT, 0.85f);
    draw_centered_text(state->renderer, &right_arrow, ">", COLOR_TEXT, 1.0f);
}

static void draw_ui(UIState* state) {
    set_color(state->renderer, COLOR_BACKGROUND);
    SDL_RenderClear(state->renderer);

    draw_text(state->renderer, 40.0f, 14.0f, Language_Get(TEXT_TITLE), COLOR_ACCENT, 2.0f);
    draw_language_selector(state);
    draw_text(state->renderer,
              40.0f,
              58.0f,
              Language_Get(TEXT_APPLY_NEXT_START),
              COLOR_MUTED,
              1.0f);
    draw_wrapped_text(state->renderer,
                      40.0f,
                      82.0f,
                      ROW_WIDTH,
                      Language_Get(TEXT_INPUT_HELP),
                      COLOR_MUTED,
                      1.0f);

    draw_row(state, CONTROL_FULLSCREEN, Language_Get(TEXT_FULLSCREEN));
    draw_row(state, CONTROL_WINDOW_WIDTH, Language_Get(TEXT_WINDOW_WIDTH));
    draw_row(state, CONTROL_WINDOW_HEIGHT, Language_Get(TEXT_WINDOW_HEIGHT));
    draw_row(state, CONTROL_SCALE_MODE, Language_Get(TEXT_SCALE_MODE));
    draw_row(state, CONTROL_BEZEL, Language_Get(TEXT_BEZEL));
    draw_row(state, CONTROL_SCANLINES, Language_Get(TEXT_SCANLINES));
    draw_row(state, CONTROL_SCANLINE_OPACITY, Language_Get(TEXT_SCANLINE_OPACITY));
    draw_row(state, CONTROL_DRAW_PLAYERS_ABOVE_HUD, Language_Get(TEXT_PLAYERS_ABOVE_HUD));

    draw_button(state, CONTROL_SAVE, Language_Get(TEXT_SAVE));
    draw_button(state, CONTROL_DEFAULTS, Language_Get(TEXT_DEFAULTS));
    draw_button(state, CONTROL_CANCEL, Language_Get(TEXT_CANCEL));

    const SDL_Color status_color = ConfigSettings_Equals(state->settings, &state->saved_settings)
                                       ? COLOR_SUCCESS
                                       : COLOR_MUTED;
    draw_text(state->renderer, 40.0f, 682.0f, state->status, status_color, 1.0f);

    char path_text[128];
    SDL_snprintf(path_text, sizeof(path_text), "%s: %.108s", Language_Get(TEXT_FILE_PREFIX), state->config_path);
    draw_text(state->renderer, 40.0f, 704.0f, path_text, COLOR_DISABLED, 0.85f);

    SDL_RenderPresent(state->renderer);
}

bool ConfigUI_Run(SDL_Window* window, SDL_Renderer* renderer, const char* config_path, ConfigSettings* settings) {
    if (window == NULL || renderer == NULL || config_path == NULL || settings == NULL) {
        return false;
    }

    UIState state = {
        .window = window,
        .renderer = renderer,
        .config_path = config_path,
        .settings = settings,
        .saved_settings = *settings,
        .focused = CONTROL_FULLSCREEN,
        .hovered = CONTROL_NONE,
        .hovered_language_direction = 0,
        .running = true,
    };
    SDL_strlcpy(state.status, Language_Get(TEXT_STATUS_LOADED), sizeof(state.status));

    while (state.running) {
        SDL_Event event;

        while (SDL_PollEvent(&event)) {
            handle_event(&state, &event);
        }

        draw_ui(&state);
        SDL_Delay(16);
    }

    return true;
}
