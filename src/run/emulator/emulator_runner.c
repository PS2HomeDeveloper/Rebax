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

#include "run/emulator/emulator_runner.h"

int emulator_runner_start(project_type_t system) { (void)system; return 0; }
void emulator_runner_stop(void) {}
int emulator_runner_is_running(void) { return 0; }
const char *emulator_runner_error(void) { return "Emulator run is not implemented yet"; }
