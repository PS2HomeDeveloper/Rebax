#include <stddef.h>

#include "run/simulation/sim_clock.h"

void sim_clock_init(sim_clock_t *clock, int rate) {
    if (clock == NULL) return;
    clock->accumulator = 0.0;
    clock->step = 1.0 / (rate > 0 ? rate : 60);
    clock->paused = 0;
    clock->step_requested = 0;
}

int sim_clock_update(sim_clock_t *clock, float elapsed, int *out_steps) {
    int steps = 0;
    if (clock == NULL || out_steps == NULL) return 0;
    if (elapsed < 0.0f) elapsed = 0.0f;
    if (elapsed > 0.25f) elapsed = 0.25f;
    if (!clock->paused) clock->accumulator += elapsed;
    if (clock->step_requested) {
        clock->step_requested = 0;
        steps = 1;
    } else {
        while (!clock->paused && clock->accumulator >= clock->step && steps < 5) {
            clock->accumulator -= clock->step;
            steps++;
        }
    }
    if (steps == 5 && clock->accumulator > clock->step * 2.0) {
        clock->accumulator = 0.0;
    }
    *out_steps = steps;
    return steps;
}

void sim_clock_pause(sim_clock_t *clock, int paused) {
    if (clock != NULL) clock->paused = paused != 0;
}

void sim_clock_step(sim_clock_t *clock) {
    if (clock != NULL) clock->step_requested = 1;
}
