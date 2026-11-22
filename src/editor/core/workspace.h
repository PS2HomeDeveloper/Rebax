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

#ifndef EDITOR_WORKSPACE_H
#define EDITOR_WORKSPACE_H

typedef enum {
    EDITOR_CAMERA_MODE_2D,
    EDITOR_CAMERA_MODE_3D
} editor_camera_mode_t;

void editor_workspace_set_camera_mode(editor_camera_mode_t mode);
void editor_workspace_activate_script(void);

void editor_workspace_update(int window_w, int window_h);

void editor_workspace_draw(int window_w, int window_h);

void editor_workspace_shutdown(void);

#endif
