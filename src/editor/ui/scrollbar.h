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

#ifndef UI_SCROLLBAR_H
#define UI_SCROLLBAR_H

#define UI_SCROLLBAR_WIDTH 8

void ui_scrollbar_update(int *scroll_offset, int *dragging,
                          int x, int w, int viewport_y, int viewport_h,
                          int content_h, int wheel_step);

void ui_scrollbar_draw(int scroll_offset, int x, int w,
                        int viewport_y, int viewport_h, int content_h);

#endif
