/*
 *  ____  _____ ____    _    __  __
 * |  _ \| ____| __ )  / \   \ \/ /
 * | |_) | _| | _ \ / _ \   \  /
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

#include <stdio.h>
#include "run/run_session.h"
#include "run/simulation/simulation_runner.h"
#include "run/emulator/emulator_runner.h"
#include "run/compiled/compiled_runner.h"
#include "run/device/ps2_device_runner.h"
#include "editor/panels/scene_tabs.h"

static run_mode_t g_mode = RUN_MODE_SIMULATION;
static project_type_t g_type = PROJECT_TYPE_MODERN;
static int g_running = 0;
static char g_message[400];

static void set_message(const char *text) { snprintf(g_message, sizeof(g_message), "%s", text ? text : ""); }

int run_session_start(run_mode_t mode, project_type_t type) {
    if (g_running) return 0;
    scene_tabs_save_all();
    g_mode = mode;
    g_type = type;
    set_message("");
    int ok = 0;
    const char *error = "";
    if (mode == RUN_MODE_SIMULATION) { ok = simulation_runner_start(); error = simulation_runner_error(); }
    else if (mode == RUN_MODE_COMPILED) { ok = compiled_runner_start(); error = compiled_runner_error(); }
    else if (mode == RUN_MODE_EMULATOR) { ok = emulator_runner_start(type); error = emulator_runner_error(); }
    else if (mode == RUN_MODE_PS2_DEVICE) { ok = ps2_device_runner_start(); error = ps2_device_runner_error(); }
    if (!ok) { set_message(error); return 0; }
    g_running = 1;
    set_message("Running...");
    return 1;
}

void run_session_stop(void) {
    if (!g_running) return;
    if (g_mode == RUN_MODE_SIMULATION) simulation_runner_stop();
    else if (g_mode == RUN_MODE_COMPILED) compiled_runner_stop();
    else if (g_mode == RUN_MODE_EMULATOR) emulator_runner_stop();
    else if (g_mode == RUN_MODE_PS2_DEVICE) ps2_device_runner_stop();
    g_running = 0;
    set_message("");
}

void run_session_update(void) {
    if (!g_running) return;
    int alive = 1;
    const char *error = "";
    if (g_mode == RUN_MODE_SIMULATION) { simulation_runner_update(); alive = simulation_runner_is_running(); error = simulation_runner_error(); }
    else if (g_mode == RUN_MODE_COMPILED) { compiled_runner_update(); alive = compiled_runner_is_running(); error = compiled_runner_error(); }
    else if (g_mode == RUN_MODE_EMULATOR) alive = emulator_runner_is_running();
    else if (g_mode == RUN_MODE_PS2_DEVICE) {
        ps2_device_runner_update();
        alive = ps2_device_runner_is_running();
        error = ps2_device_runner_error();
        while (ps2_device_runner_poll_line() != NULL) {}
    }
    (void)g_type;
    if (!alive) { g_running = 0; set_message(error); }
}

int run_session_is_running(void) { return g_running; }

const char *run_session_message(void) { return g_message; }
