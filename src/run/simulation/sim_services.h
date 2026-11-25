#ifndef SIM_SERVICES_H
#define SIM_SERVICES_H

#include "run/simulation/sim_input.h"

typedef struct {
    const sim_pad_state_t *(*pad)(void);
    float (*time_delta)(void);
    void (*video_size)(int *width, int *height);
    void (*audio_submit)(const void *samples, int count);
} sim_services_t;

const sim_services_t *sim_services_get(void);

#endif
