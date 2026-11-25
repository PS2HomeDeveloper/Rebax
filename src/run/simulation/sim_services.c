#include <stddef.h>

#include "run/simulation/sim_services.h"
#include "platform/common/window.h"

static const sim_pad_state_t *service_pad(void) {
    return sim_input_pad();
}

static float service_time_delta(void) {
    return window_get_delta_time();
}

static void service_video_size(int *width, int *height) {
    if (width != NULL) *width = window_get_width();
    if (height != NULL) *height = window_get_height();
}

static void service_audio_submit(const void *samples, int count) {
    (void)samples;
    (void)count;
}

const sim_services_t *sim_services_get(void) {
    static const sim_services_t services = {
        service_pad,
        service_time_delta,
        service_video_size,
        service_audio_submit
    };
    return &services;
}
