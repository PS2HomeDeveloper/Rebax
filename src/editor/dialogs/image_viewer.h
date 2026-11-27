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

#ifndef IMAGE_VIEWER_H
#define IMAGE_VIEWER_H

int image_viewer_open(const char *path);
int image_viewer_is_open(void);
void image_viewer_update(int window_w, int window_h);
void image_viewer_draw(int window_w, int window_h);
void image_viewer_shutdown(void);

#endif
