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


#ifndef UI_WINDOWS_H
#define UI_WINDOWS_H

typedef struct ui_window {
    int (*is_open)(void);
    void (*update)(int window_w, int window_h);
    void (*draw)(int window_w, int window_h);
    unsigned order;
    int registered;
    int x, y, w, h;
    int armed;
} ui_window_t;

#define UI_WINDOW_INIT(is_open_fn, update_fn, draw_fn) { is_open_fn, update_fn, draw_fn, 0, 0, 0, 0, 0, 0, 0 }

void ui_windows_show(ui_window_t *window);
int ui_windows_any_open(void);
void ui_windows_update(int window_w, int window_h);
void ui_windows_draw(int window_w, int window_h);

int ui_windows_context_is_front(void);
void ui_windows_context_report(int x, int y, int w, int h);

#endif
