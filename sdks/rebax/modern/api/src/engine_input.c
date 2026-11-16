/*
 *  ____  _____ ____    _    __  __
 * |  _ \| ____| __ )  / \   \ \/ /
 * | |_) |  _| |  _ \ / _ \   \  / 
 * |  _ <| |___| |_) / ___ \  /  \ 
 * |_| \_\_____|____/_/   \_\/_/\_\
 *
 * Rebax Engine
 * Copyright (c) 2026 PS2HomeDeveloper
 *
 * Licensed under the MIT License.
 * See the LICENSE file for details.
 */

#include <string.h>
#include "engine_input.h"

static engine_input_state_t g_state;

static int controller_slot_for_id(SDL_JoystickID id) {
    for (int i = 0; i < ENGINE_MAX_CONTROLLERS; i++) {
        if (g_state.controllers[i] != NULL && SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(g_state.controllers[i])) == id) return i;
    }
    return -1;
}

static void controller_open(int device_index) {
    if (!SDL_IsGameController(device_index)) return;
    SDL_GameController *controller = SDL_GameControllerOpen(device_index);
    if (controller == NULL) return;
    for (int i = 0; i < ENGINE_MAX_CONTROLLERS; i++) {
        if (g_state.controllers[i] == NULL) {
            g_state.controllers[i] = controller;
            return;
        }
    }
    SDL_GameControllerClose(controller);
}

void engine_input_init(void) {
    memset(&g_state, 0, sizeof(g_state));
    int count = SDL_NumJoysticks();
    for (int i = 0; i < count; i++) controller_open(i);
}

void engine_input_shutdown(void) {
    for (int i = 0; i < ENGINE_MAX_CONTROLLERS; i++) {
        if (g_state.controllers[i] != NULL) SDL_GameControllerClose(g_state.controllers[i]);
    }
    memset(&g_state, 0, sizeof(g_state));
}

void engine_input_begin_frame(void) {
    memset(g_state.keys_pressed, 0, sizeof(g_state.keys_pressed));
    memset(g_state.keys_released, 0, sizeof(g_state.keys_released));
    g_state.mouse_pressed = 0;
    g_state.mouse_released = 0;
    g_state.mouse_delta_x = 0;
    g_state.mouse_delta_y = 0;
    g_state.mouse_wheel_x = 0;
    g_state.mouse_wheel_y = 0;
}

void engine_input_process_event(const SDL_Event *event) {
    if (event == NULL) return;
    switch (event->type) {
        case SDL_QUIT:
            break;
        case SDL_KEYDOWN:
            if (event->key.keysym.scancode >= SDL_SCANCODE_UNKNOWN && event->key.keysym.scancode < SDL_NUM_SCANCODES) {
                Uint8 *down = &g_state.keys[event->key.keysym.scancode];
                if (!*down && event->key.repeat == 0) g_state.keys_pressed[event->key.keysym.scancode] = 1;
                *down = 1;
            }
            break;
        case SDL_KEYUP:
            if (event->key.keysym.scancode >= SDL_SCANCODE_UNKNOWN && event->key.keysym.scancode < SDL_NUM_SCANCODES) {
                g_state.keys[event->key.keysym.scancode] = 0;
                g_state.keys_released[event->key.keysym.scancode] = 1;
            }
            break;
        case SDL_MOUSEMOTION:
            g_state.mouse_x = event->motion.x;
            g_state.mouse_y = event->motion.y;
            g_state.mouse_delta_x += event->motion.xrel;
            g_state.mouse_delta_y += event->motion.yrel;
            break;
        case SDL_MOUSEBUTTONDOWN: {
            if (event->button.button == 0 || event->button.button > 32) break;
            Uint32 mask = SDL_BUTTON(event->button.button);
            if ((g_state.mouse_buttons & mask) == 0) g_state.mouse_pressed |= mask;
            g_state.mouse_buttons |= mask;
            g_state.mouse_x = event->button.x;
            g_state.mouse_y = event->button.y;
            break;
        }
        case SDL_MOUSEBUTTONUP: {
            if (event->button.button == 0 || event->button.button > 32) break;
            Uint32 mask = SDL_BUTTON(event->button.button);
            g_state.mouse_buttons &= ~mask;
            g_state.mouse_released |= mask;
            g_state.mouse_x = event->button.x;
            g_state.mouse_y = event->button.y;
            break;
        }
        case SDL_MOUSEWHEEL:
            g_state.mouse_wheel_x += event->wheel.x;
            g_state.mouse_wheel_y += event->wheel.y;
            break;
        case SDL_FINGERDOWN:
        case SDL_FINGERMOTION:
            g_state.touch_active = 1;
            g_state.touch_id = event->tfinger.touchId;
            g_state.finger_id = event->tfinger.fingerId;
            g_state.touch_x = event->tfinger.x;
            g_state.touch_y = event->tfinger.y;
            break;
        case SDL_FINGERUP:
            if (g_state.touch_id == event->tfinger.touchId && g_state.finger_id == event->tfinger.fingerId) g_state.touch_active = 0;
            break;
        case SDL_CONTROLLERDEVICEADDED:
            controller_open(event->cdevice.which);
            break;
        case SDL_CONTROLLERDEVICEREMOVED: {
            int slot = controller_slot_for_id(event->cdevice.which);
            if (slot >= 0) {
                SDL_GameControllerClose(g_state.controllers[slot]);
                g_state.controllers[slot] = NULL;
                memset(g_state.controller_buttons[slot], 0, sizeof(g_state.controller_buttons[slot]));
                memset(g_state.controller_axes[slot], 0, sizeof(g_state.controller_axes[slot]));
            }
            break;
        }
        case SDL_WINDOWEVENT:
            if (event->window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
                memset(g_state.keys, 0, sizeof(g_state.keys));
                g_state.mouse_buttons = 0;
            }
            break;
        default:
            break;
    }
}

void engine_input_poll_controllers(void) {
    for (int i = 0; i < ENGINE_MAX_CONTROLLERS; i++) {
        SDL_GameController *controller = g_state.controllers[i];
        if (controller == NULL || SDL_GameControllerGetAttached(controller) != SDL_TRUE) continue;
        for (int b = 0; b < SDL_CONTROLLER_BUTTON_MAX; b++) {
            g_state.controller_buttons[i][b] = (Uint8)SDL_GameControllerGetButton(controller, (SDL_GameControllerButton)b);
        }
        for (int a = 0; a < SDL_CONTROLLER_AXIS_MAX; a++) {
            g_state.controller_axes[i][a] = SDL_GameControllerGetAxis(controller, (SDL_GameControllerAxis)a);
        }
    }
}

const engine_input_state_t *engine_input_get_state(void) { return &g_state; }
int engine_input_key_down(SDL_Scancode key) { return key >= 0 && key < SDL_NUM_SCANCODES && g_state.keys[key] != 0; }
int engine_input_key_pressed(SDL_Scancode key) { return key >= 0 && key < SDL_NUM_SCANCODES && g_state.keys_pressed[key] != 0; }
int engine_input_mouse_button_down(Uint32 button_mask) { return (g_state.mouse_buttons & button_mask) != 0; }
int engine_input_controller_button_down(int controller, SDL_GameControllerButton button) {
    return controller >= 0 && controller < ENGINE_MAX_CONTROLLERS && button >= 0 && button < SDL_CONTROLLER_BUTTON_MAX && g_state.controller_buttons[controller][button] != 0;
}
Sint16 engine_input_controller_axis(int controller, SDL_GameControllerAxis axis) {
    if (controller < 0 || controller >= ENGINE_MAX_CONTROLLERS || axis < 0 || axis >= SDL_CONTROLLER_AXIS_MAX) return 0;
    return g_state.controller_axes[controller][axis];
}
