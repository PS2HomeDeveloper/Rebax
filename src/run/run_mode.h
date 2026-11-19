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

#ifndef RUN_MODE_H
#define RUN_MODE_H

#include "project/project_types.h"

typedef enum {
    RUN_MODE_SIMULATION = 0,
    RUN_MODE_COMPILED,
    RUN_MODE_EMULATOR,
    RUN_MODE_PS2_DEVICE,
    RUN_MODE_COUNT
} run_mode_t;

typedef struct {
    run_mode_t mode;
    int available;
    int has_settings;
    char label[48];
    char reason[160];
    int package_platform;
} run_mode_info_t;

int run_modes_for_project(project_type_t type, run_mode_info_t *out, int max);

run_mode_t run_mode_default(project_type_t type);

int run_host_platform_index(void);

int run_host_is_desktop(void);

#endif
