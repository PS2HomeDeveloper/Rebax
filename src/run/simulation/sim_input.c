#include "run/simulation/sim_input.h"
#include "platform/common/window.h"

static sim_pad_state_t g_pad;
static int g_playstation_layout;

void sim_input_begin_frame(void) {
    int touch_x;
    int touch_y;
    int touch_active;
    g_pad.buttons[SIM_PAD_UP] = window_key_down_w() || window_controller_button(11);
    g_pad.buttons[SIM_PAD_DOWN] = window_key_down_s() || window_controller_button(12);
    g_pad.buttons[SIM_PAD_LEFT] = window_key_down_a() || window_controller_button(13);
    g_pad.buttons[SIM_PAD_RIGHT] = window_key_down_d() || window_controller_button(14);
    g_pad.buttons[SIM_PAD_CONFIRM] = window_key_just_pressed_enter() || window_controller_button(0);
    g_pad.buttons[SIM_PAD_CANCEL] = window_key_just_pressed_escape() || window_controller_button(1);
    g_pad.buttons[SIM_PAD_START] = window_key_just_pressed_f();
    g_pad.buttons[SIM_PAD_SELECT] = window_key_down_shift();
    touch_x = window_mouse_x();
    touch_y = window_mouse_y();
    touch_active = sim_input_touch_supported() && window_mouse_left_down();
    if (touch_active && touch_x < window_get_width() / 2) {
        g_pad.buttons[SIM_PAD_LEFT] |= touch_x < window_get_width() / 4;
        g_pad.buttons[SIM_PAD_RIGHT] |= touch_x >= window_get_width() / 4;
        g_pad.buttons[SIM_PAD_UP] |= touch_y < window_get_height() / 2;
        g_pad.buttons[SIM_PAD_DOWN] |= touch_y >= window_get_height() / 2;
    } else if (touch_active) {
        g_pad.buttons[SIM_PAD_CONFIRM] = 1;
    }
    g_pad.axis_x = window_controller_axis(0);
    if (g_pad.axis_x == 0.0f) g_pad.axis_x = (float)g_pad.buttons[SIM_PAD_RIGHT] -
                   (float)g_pad.buttons[SIM_PAD_LEFT];
    g_pad.axis_y = window_controller_axis(1);
    if (g_pad.axis_y == 0.0f) g_pad.axis_y = (float)g_pad.buttons[SIM_PAD_DOWN] -
                   (float)g_pad.buttons[SIM_PAD_UP];
}

void sim_input_set_profile(int playstation_layout) {
    g_playstation_layout = playstation_layout != 0;
    (void)g_playstation_layout;
}

const sim_pad_state_t *sim_input_pad(void) {
    return &g_pad;
}

int sim_input_touch_supported(void) {
#if defined(__ANDROID__)
    return 1;
#else
    return 0;
#endif
}
