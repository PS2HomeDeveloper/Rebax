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

#ifndef EMULATOR_SELECT_DIALOG_H
#define EMULATOR_SELECT_DIALOG_H

#include "project/project_types.h"

void emulator_select_dialog_open(project_type_t system, const char *notice);
int emulator_select_dialog_is_open(void);
void emulator_select_dialog_update(int window_w, int window_h);
void emulator_select_dialog_draw(int window_w, int window_h);
void emulator_select_dialog_shutdown(void);

#endif
