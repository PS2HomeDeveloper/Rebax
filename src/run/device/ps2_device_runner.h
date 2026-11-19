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

#ifndef PS2_DEVICE_RUNNER_H
#define PS2_DEVICE_RUNNER_H

int ps2_device_runner_start(void);
void ps2_device_runner_update(void);
void ps2_device_runner_stop(void);
int ps2_device_runner_is_running(void);
const char *ps2_device_runner_error(void);
const char *ps2_device_runner_poll_line(void);

#endif
