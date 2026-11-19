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

#include "run/emulator/emulator_runner.h"

int emulator_runner_start(project_type_t system) { (void)system; return 0; }
void emulator_runner_stop(void) {}
int emulator_runner_is_running(void) { return 0; }
const char *emulator_runner_error(void) { return "Emulator run is not implemented yet"; }
