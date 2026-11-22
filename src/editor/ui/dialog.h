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

#ifndef UI_DIALOG_H
#define UI_DIALOG_H

#include "platform/common/window.h"

typedef struct {
    int off_x, off_y;
    int active;
    int grab_x, grab_y;
} ui_dialog_drag_t;

void ui_dialog_place(ui_dialog_drag_t *drag, int interactive, int window_w, int window_h,
                     int w, int h, int title_h, int *out_x, int *out_y);
void ui_dialog_drag_reset(ui_dialog_drag_t *drag);
void ui_dialog_draw_backdrop(int window_w, int window_h);
void ui_dialog_draw_frame(int x, int y, int w, int h);
void ui_dialog_draw_title_left(window_texture_t *title, int title_w, int title_h,
                               int x, int y, int titlebar_height, int padding);
void ui_dialog_draw_title_centered(window_texture_t *title, int title_w, int title_h,
                                   int x, int y, int w, int titlebar_height);
void ui_dialog_draw_close_button(int x, int y, int size);
int ui_dialog_close_button_hit(int mouse_x, int mouse_y, int x, int y, int size);
void ui_dialog_shutdown(void);

#endif
