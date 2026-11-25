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
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */
#ifndef SIM_CLOCK_H
#define SIM_CLOCK_H
typedef struct { double accumulator; double step; int paused; int step_requested; } sim_clock_t;
void sim_clock_init(sim_clock_t *clock, int frame_rate);
int sim_clock_update(sim_clock_t *clock, float elapsed, int *out_steps);
void sim_clock_pause(sim_clock_t *clock, int paused);
void sim_clock_step(sim_clock_t *clock);
#endif
