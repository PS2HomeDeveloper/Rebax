/*
 *  ____  _____ ____    _    __  __
 * |  _ \| ____| __ )  / \   \ \/ /
 * | |_) |  _| |  _ \ / _ \   \  / 
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

#ifndef CURRENT_PROJECT_H
#define CURRENT_PROJECT_H

#include "project/project_file.h"

void current_project_set(const char *root_path, const project_settings_t *settings);

void current_project_clear(void);

const char *current_project_get_path(void);

const project_settings_t *current_project_get_settings(void);

int current_project_is_open(void);

#endif
