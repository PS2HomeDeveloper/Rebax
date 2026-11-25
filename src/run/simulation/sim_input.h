#ifndef SIM_INPUT_H
#define SIM_INPUT_H

typedef enum {
    SIM_PAD_UP,
    SIM_PAD_DOWN,
    SIM_PAD_LEFT,
    SIM_PAD_RIGHT,
    SIM_PAD_CONFIRM,
    SIM_PAD_CANCEL,
    SIM_PAD_START,
    SIM_PAD_SELECT,
    SIM_PAD_COUNT
} sim_pad_button_t;

typedef struct {
    int buttons[SIM_PAD_COUNT];
    float axis_x;
    float axis_y;
} sim_pad_state_t;

void sim_input_begin_frame(void);
void sim_input_set_profile(int playstation_layout);
const sim_pad_state_t *sim_input_pad(void);
int sim_input_touch_supported(void);

#endif
