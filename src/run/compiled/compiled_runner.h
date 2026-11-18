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

#ifndef COMPILED_RUNNER_H
#define COMPILED_RUNNER_H

int compiled_runner_start(void);
void compiled_runner_update(void);
void compiled_runner_stop(void);
int compiled_runner_is_running(void);
const char *compiled_runner_error(void);

#endif
