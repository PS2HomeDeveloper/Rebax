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
 * Licensed under the MIT License.
 * See the LICENSE file for details.
 */

#ifndef EMULATOR_RUNNER_H
#define EMULATOR_RUNNER_H

#include "project/project_types.h"

int emulator_runner_start(project_type_t system);
void emulator_runner_stop(void);
int emulator_runner_is_running(void);
const char *emulator_runner_error(void);

#endif
