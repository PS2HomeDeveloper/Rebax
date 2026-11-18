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

#ifndef RUN_SESSION_H
#define RUN_SESSION_H

#include "run/run_mode.h"

int run_session_start(run_mode_t mode, project_type_t type);
void run_session_stop(void);
void run_session_update(void);
int run_session_is_running(void);
const char *run_session_message(void);

#endif
