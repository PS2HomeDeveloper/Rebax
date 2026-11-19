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
#include <string.h>
#include "run/device/ps2_device_runner.h"
#include "exporters/compiler/ps2/ps2_exporter.h"
#include "libraries/ps2link/ps2link_client.h"
#include "filesystem/storage_paths.h"
#include "filesystem/rebax_fs.h"
#include "run/run_settings.h"

#define RUN_ELF_NAME "rebax_run"

typedef enum { PHASE_IDLE, PHASE_EXPORT, PHASE_LINK, PHASE_DONE } phase_t;

static phase_t g_phase = PHASE_IDLE;
static char g_error[400];
static char g_dir[1024];
static char g_line[512];

static void fail(const char *text) {
    snprintf(g_error, sizeof(g_error), "%s", text);
    g_phase = PHASE_DONE;
}

int ps2_device_runner_start(void) {
    g_error[0] = '\0';
    snprintf(g_dir, sizeof(g_dir), "%s/ps2_run", rebax_temp_export_dir());
    rebax_fs_mkdir_p(g_dir);
    if (!ps2_export_start(RUN_ELF_NAME, 0, g_dir)) { fail("Could not start the PlayStation 2 build"); return 0; }
    g_phase = PHASE_EXPORT;
    return 1;
}

void ps2_device_runner_update(void) {
    if (g_phase == PHASE_EXPORT) {
        ps2_export_update();
        while (ps2_export_poll_next_line() != NULL) {}
        ps2_export_status_t s = ps2_export_get_status();
        if (s == PS2_EXPORT_STATUS_FAILED) fail("The PlayStation 2 build failed");
        else if (s == PS2_EXPORT_STATUS_SUCCESS) {
            if (ps2link_start(run_settings_ps2_address(), g_dir, RUN_ELF_NAME ".elf")) g_phase = PHASE_LINK;
            else fail("Could not start the network session");
        }
    } else if (g_phase == PHASE_LINK) {
        ps2link_state_t s = ps2link_get_state();
        if (s == PS2LINK_STATE_FAILED) fail(ps2link_get_error());
        else if (s == PS2LINK_STATE_ENDED) g_phase = PHASE_DONE;
    }
}

void ps2_device_runner_stop(void) {
    if (g_phase == PHASE_EXPORT) ps2_export_cancel();
    if (g_phase == PHASE_LINK) ps2link_stop();
    g_phase = PHASE_IDLE;
}

int ps2_device_runner_is_running(void) { return g_phase == PHASE_EXPORT || g_phase == PHASE_LINK; }

const char *ps2_device_runner_error(void) { return g_error; }

const char *ps2_device_runner_poll_line(void) {
    const char *l = ps2link_poll_line();
    if (!l) return NULL;
    snprintf(g_line, sizeof(g_line), "%s", l);
    return g_line;
}
