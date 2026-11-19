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

#ifndef RUN_SETTINGS_H
#define RUN_SETTINGS_H

#include "project/project_types.h"

const char *run_settings_ps2_address(void);
void run_settings_set_ps2_address(const char *address);

int run_settings_ps2_configured(void);
void run_settings_set_ps2_configured(int configured);

const char *run_settings_emulator_id(project_type_t system);
void run_settings_set_emulator_id(project_type_t system, const char *id);

const char *run_settings_emulator_path(const char *emulator_id);
void run_settings_set_emulator_path(const char *emulator_id, const char *path);

int run_settings_valid_address(const char *address);

#endif
