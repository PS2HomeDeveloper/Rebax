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

#ifndef ENGINE_INPUT_H
#define ENGINE_INPUT_H

#include <SDL2/SDL.h>

#ifdef __cplusplus
extern "C" {
#endif

enum { ENGINE_MAX_CONTROLLERS = 4 };

typedef struct {
    Uint8 keys[SDL_NUM_SCANCODES];
    Uint8 keys_pressed[SDL_NUM_SCANCODES];
    Uint8 keys_released[SDL_NUM_SCANCODES];
    Uint32 mouse_buttons;
    Uint32 mouse_pressed;
    Uint32 mouse_released;
    int mouse_x;
    int mouse_y;
    int mouse_delta_x;
    int mouse_delta_y;
    int mouse_wheel_x;
    int mouse_wheel_y;
    int touch_active;
    SDL_TouchID touch_id;
    SDL_FingerID finger_id;
    float touch_x;
    float touch_y;
    SDL_GameController *controllers[ENGINE_MAX_CONTROLLERS];
    Uint8 controller_buttons[ENGINE_MAX_CONTROLLERS][SDL_CONTROLLER_BUTTON_MAX];
    Sint16 controller_axes[ENGINE_MAX_CONTROLLERS][SDL_CONTROLLER_AXIS_MAX];
} engine_input_state_t;

void engine_input_init(void);
void engine_input_shutdown(void);
void engine_input_begin_frame(void);
void engine_input_process_event(const SDL_Event *event);
void engine_input_poll_controllers(void);
const engine_input_state_t *engine_input_get_state(void);
int engine_input_key_down(SDL_Scancode key);
int engine_input_key_pressed(SDL_Scancode key);
int engine_input_mouse_button_down(Uint32 button_mask);
int engine_input_controller_button_down(int controller, SDL_GameControllerButton button);
Sint16 engine_input_controller_axis(int controller, SDL_GameControllerAxis axis);

#ifdef __cplusplus
}
#endif

#endif
