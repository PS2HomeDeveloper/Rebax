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

#ifndef VIEWPORT_2D_H
#define VIEWPORT_2D_H

void viewport_2d_update(int x, int y, int w, int h);
void viewport_2d_focus_scene(void);

void viewport_2d_draw(int x, int y, int w, int h);

void viewport_2d_project(float world_x, float world_y,
                          int cam_x, int cam_y, int cam_w, int cam_h,
                          int *out_screen_x, int *out_screen_y);

void viewport_2d_unproject(int screen_x, int screen_y,
                            int cam_x, int cam_y, int cam_w, int cam_h,
                            float *out_world_x, float *out_world_y);
float viewport_2d_get_zoom(void);

#endif
