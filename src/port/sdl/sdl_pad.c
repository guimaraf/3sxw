#include "port/sdl/sdl_pad.h"
#include "port/config/keymap.h"

#include <SDL3/SDL.h>

#define INPUT_SOURCES_MAX 2
#define INPUT_AXIS_DEADZONE (SDL_MAX_SINT16 / 4)

typedef enum SDLPad_InputType { SDLPAD_INPUT_NONE = 0, SDLPAD_INPUT_GAMEPAD, SDLPAD_INPUT_KEYBOARD } SDLPad_InputType;

typedef struct SDLPad_GamepadInputSource {
    Uint32 type;
    SDL_Gamepad* gamepad;
} SDLPad_GamepadInputSource;

typedef struct SDLPad_KeyboardInputSource {
    Uint32 type;
} SDLPad_KeyboardInputSource;

typedef union SDLPad_InputSource {
    Uint32 type;
    SDLPad_GamepadInputSource gamepad;
    SDLPad_KeyboardInputSource keyboard;
} SDLPad_InputSource;

static SDLPad_InputSource input_sources[INPUT_SOURCES_MAX] = { 0 };
static int connected_input_sources = 0;
static int keyboard_index = -1;
static bool input_enabled = true;
static bool wait_for_neutral[INPUT_SOURCES_MAX] = { false };
static bool suppressed_scancodes[SDL_SCANCODE_COUNT] = { false };

static int input_source_index_from_joystick_id(SDL_JoystickID id) {
    for (int i = 0; i < INPUT_SOURCES_MAX; i++) {
        const SDLPad_InputSource* input_source = &input_sources[i];

        if (input_source->type != SDLPAD_INPUT_GAMEPAD) {
            continue;
        }

        const SDL_JoystickID this_id = SDL_GetGamepadID(input_source->gamepad.gamepad);

        if (this_id == id) {
            return i;
        }
    }

    return -1;
}

static void setup_keyboard() {
    if (keyboard_index >= 0) {
        return;
    }

    for (int i = 0; i < SDL_arraysize(input_sources); i++) {
        SDLPad_InputSource* input_source = &input_sources[i];

        if (input_source->type == SDLPAD_INPUT_NONE) {
            input_source->type = SDLPAD_INPUT_KEYBOARD;
            keyboard_index = i;
            wait_for_neutral[i] = true;
            connected_input_sources += 1;
            break;
        }
    }
}

static void remove_keyboard() {
    if (keyboard_index < 0) {
        return;
    }

    for (int i = 0; i < SDL_arraysize(input_sources); i++) {
        SDLPad_InputSource* input_source = &input_sources[i];

        if (input_source->type == SDLPAD_INPUT_KEYBOARD) {
            input_source->type = SDLPAD_INPUT_NONE;
            keyboard_index = -1;
            wait_for_neutral[i] = true;
            connected_input_sources -= 1;
            break;
        }
    }
}

static void handle_gamepad_added_event(SDL_GamepadDeviceEvent* event) {
    // Remove keyboard to potentially make space for the new gamepad
    remove_keyboard();

    if (connected_input_sources >= INPUT_SOURCES_MAX) {
        return;
    }

    SDL_Gamepad* gamepad = SDL_OpenGamepad(event->which);

    if (gamepad == NULL) {
        SDL_Log("Couldn't open gamepad: %s", SDL_GetError());
        setup_keyboard();
        return;
    }

    for (int i = 0; i < INPUT_SOURCES_MAX; i++) {
        SDLPad_InputSource* input_source = &input_sources[i];

        if (input_source->type != SDLPAD_INPUT_NONE) {
            continue;
        }

        input_source->type = SDLPAD_INPUT_GAMEPAD;
        input_source->gamepad.gamepad = gamepad;
        wait_for_neutral[i] = true;
        break;
    }

    connected_input_sources += 1;

    // Setup keyboard again, if there's a free slot
    setup_keyboard();
}

static void handle_gamepad_removed_event(SDL_GamepadDeviceEvent* event) {
    const int index = input_source_index_from_joystick_id(event->which);

    if (index < 0) {
        return;
    }

    SDLPad_InputSource* input_source = &input_sources[index];
    SDL_CloseGamepad(input_source->gamepad.gamepad);
    input_source->type = SDLPAD_INPUT_NONE;
    input_source->gamepad.gamepad = NULL;
    wait_for_neutral[index] = true;
    connected_input_sources -= 1;

    // Setup keyboard in the newly freed slot
    setup_keyboard();
}

static void update_suppressed_scancodes(const bool* keys) {
    for (int i = 0; i < SDL_SCANCODE_COUNT; i++) {
        if (suppressed_scancodes[i] && !keys[i]) {
            suppressed_scancodes[i] = false;
        }
    }
}

static bool any_pressed(const bool* keys, KeymapButton button) {
    bool result = false;
    const SDL_Scancode* codes = Keymap_GetScancodes(button);

    for (int i = 0; i < KEYMAP_CODES_PER_BUTTON; i++) {
        const SDL_Scancode code = codes[i];

        if (code == SDL_SCANCODE_UNKNOWN) {
            break;
        }

        result = result || (keys[code] && !suppressed_scancodes[code]);
    }

    return result;
}

static void get_keyboard_state(SDLPad_ButtonState* state) {
    SDL_zerop(state);
    const bool* keys = SDL_GetKeyboardState(NULL);
    const bool alt_held = keys[SDL_SCANCODE_LALT] || keys[SDL_SCANCODE_RALT] || (SDL_GetModState() & SDL_KMOD_ALT);
    update_suppressed_scancodes(keys);

    state->dpad_up = any_pressed(keys, KEYMAP_BUTTON_UP);
    state->dpad_left = any_pressed(keys, KEYMAP_BUTTON_LEFT);
    state->dpad_down = any_pressed(keys, KEYMAP_BUTTON_DOWN);
    state->dpad_right = any_pressed(keys, KEYMAP_BUTTON_RIGHT);
    state->north = any_pressed(keys, KEYMAP_BUTTON_NORTH);
    state->west = any_pressed(keys, KEYMAP_BUTTON_WEST);
    state->south = any_pressed(keys, KEYMAP_BUTTON_SOUTH);
    state->east = any_pressed(keys, KEYMAP_BUTTON_EAST);
    state->left_shoulder = any_pressed(keys, KEYMAP_BUTTON_LEFT_SHOULDER);
    state->right_shoulder = any_pressed(keys, KEYMAP_BUTTON_RIGHT_SHOULDER);
    state->left_trigger = any_pressed(keys, KEYMAP_BUTTON_LEFT_TRIGGER) ? SDL_MAX_SINT16 : 0;
    state->right_trigger = any_pressed(keys, KEYMAP_BUTTON_RIGHT_TRIGGER) ? SDL_MAX_SINT16 : 0;
    state->left_stick = any_pressed(keys, KEYMAP_BUTTON_LEFT_STICK);
    state->right_stick = any_pressed(keys, KEYMAP_BUTTON_RIGHT_STICK);
    state->back = any_pressed(keys, KEYMAP_BUTTON_BACK);
    state->start = !alt_held && any_pressed(keys, KEYMAP_BUTTON_START);

#if DEBUG
    state->right_stick |= keys[SDL_SCANCODE_TAB];
#endif
}

static void get_gamepad_state(int id, SDLPad_ButtonState* state) {
    const SDL_Gamepad* pad = input_sources[id].gamepad.gamepad;

    state->dpad_up = SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_DPAD_UP);
    state->dpad_left = SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_DPAD_LEFT);
    state->dpad_down = SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_DPAD_DOWN);
    state->dpad_right = SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_DPAD_RIGHT);
    state->north = SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_NORTH);
    state->west = SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_WEST);
    state->south = SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_SOUTH);
    state->east = SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_EAST);
    state->left_shoulder = SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_LEFT_SHOULDER);
    state->right_shoulder = SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER);
    state->left_stick = SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_LEFT_STICK);
    state->right_stick = SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_RIGHT_STICK);
    state->back = SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_BACK);
    state->start = SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_START);

    state->left_trigger = SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFT_TRIGGER);
    state->right_trigger = SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER);
    state->left_stick_x = SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFTX);
    state->left_stick_y = SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFTY);
    state->right_stick_x = SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_RIGHTX);
    state->right_stick_y = SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_RIGHTY);

    if (state->left_trigger <= INPUT_AXIS_DEADZONE) {
        state->left_trigger = 0;
    }

    if (state->right_trigger <= INPUT_AXIS_DEADZONE) {
        state->right_trigger = 0;
    }
}

static bool stick_is_neutral(Sint16 x, Sint16 y) {
    const Sint64 magnitude_squared = ((Sint64)x * x) + ((Sint64)y * y);
    const Sint64 deadzone_squared = (Sint64)INPUT_AXIS_DEADZONE * INPUT_AXIS_DEADZONE;
    return magnitude_squared <= deadzone_squared;
}

static bool button_state_is_neutral(const SDLPad_ButtonState* state) {
    return !state->south && !state->east && !state->west && !state->north && !state->back && !state->start &&
           !state->left_stick && !state->right_stick && !state->left_shoulder && !state->right_shoulder &&
           state->left_trigger == 0 && state->right_trigger == 0 && !state->dpad_up && !state->dpad_down &&
           !state->dpad_left && !state->dpad_right && stick_is_neutral(state->left_stick_x, state->left_stick_y) &&
           stick_is_neutral(state->right_stick_x, state->right_stick_y);
}

void SDLPad_Init() {
    setup_keyboard();
}

void SDLPad_Quit() {
    for (int i = 0; i < SDL_arraysize(input_sources); i++) {
        SDLPad_InputSource* input_source = &input_sources[i];

        if (input_source->type == SDLPAD_INPUT_GAMEPAD && input_source->gamepad.gamepad != NULL) {
            SDL_CloseGamepad(input_source->gamepad.gamepad);
        }
    }

    SDL_zeroa(input_sources);
    SDL_zeroa(wait_for_neutral);
    SDL_zeroa(suppressed_scancodes);
    connected_input_sources = 0;
    keyboard_index = -1;
    input_enabled = true;
}

void SDLPad_SetInputEnabled(bool enabled) {
    input_enabled = enabled;

    if (!enabled) {
        for (int i = 0; i < SDL_arraysize(wait_for_neutral); i++) {
            wait_for_neutral[i] = true;
        }
    }
}

void SDLPad_SuppressKeyboardScancodeUntilReleased(SDL_Scancode scancode) {
    if (scancode > SDL_SCANCODE_UNKNOWN && scancode < SDL_SCANCODE_COUNT) {
        suppressed_scancodes[scancode] = true;
    }
}

void SDLPad_HandleGamepadDeviceEvent(SDL_GamepadDeviceEvent* event) {
    switch (event->type) {
    case SDL_EVENT_GAMEPAD_ADDED:
        handle_gamepad_added_event(event);
        break;

    case SDL_EVENT_GAMEPAD_REMOVED:
        handle_gamepad_removed_event(event);
        break;

    default:
        // Do nothing
        break;
    }
}

bool SDLPad_IsGamepadConnected(int id) {
    return input_sources[id].type != SDLPAD_INPUT_NONE;
}

void SDLPad_GetButtonState(int id, SDLPad_ButtonState* state) {
    SDL_zerop(state);

    if (id < 0 || id >= SDL_arraysize(input_sources) || input_sources[id].type == SDLPAD_INPUT_NONE) {
        return;
    }

    if (id == keyboard_index) {
        get_keyboard_state(state);
    } else {
        get_gamepad_state(id, state);
    }

    if (!input_enabled || wait_for_neutral[id]) {
        if (input_enabled && button_state_is_neutral(state)) {
            wait_for_neutral[id] = false;
        }

        SDL_zerop(state);
    }
}

void SDLPad_RumblePad(int id, bool low_freq_enabled, Uint8 high_freq_rumble) {
    const SDLPad_InputSource* input_source = &input_sources[id];

    if (input_source->type != SDLPAD_INPUT_GAMEPAD) {
        return;
    }

    const Uint16 low_freq_rumble = low_freq_enabled ? UINT16_MAX : 0;
    const Uint16 high_freq_rumble_adjusted = ((float)high_freq_rumble / UINT8_MAX) * UINT16_MAX;
    const Uint32 duration = high_freq_rumble_adjusted > 0 ? 500 : 200;

    SDL_RumbleGamepad(input_source->gamepad.gamepad, low_freq_rumble, high_freq_rumble_adjusted, duration);
}
