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

#ifndef FILE_SYSTEM_PANEL_H
#define FILE_SYSTEM_PANEL_H

void file_system_panel_update(int x, int y, int w, int h);
void file_system_panel_draw(int x, int y, int w, int h);
void file_system_panel_shutdown(void);

void file_system_panel_open_scene(const char *path);

#endif
