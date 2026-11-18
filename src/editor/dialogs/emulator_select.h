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

#ifndef EMULATOR_SELECT_DIALOG_H
#define EMULATOR_SELECT_DIALOG_H

#include "project/project_types.h"

void emulator_select_dialog_open(project_type_t system, const char *notice);
int emulator_select_dialog_is_open(void);
void emulator_select_dialog_update(int window_w, int window_h);
void emulator_select_dialog_draw(int window_w, int window_h);
void emulator_select_dialog_shutdown(void);

#endif
