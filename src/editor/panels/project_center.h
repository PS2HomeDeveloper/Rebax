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

#ifndef UI_PROJECT_CENTER_H
#define UI_PROJECT_CENTER_H

#include "editor/ui/theme.h"

typedef struct {
    int x, y, w, h;
} ui_rect_t;

#define UI_SEARCHBAR_HEIGHT   60
#define UI_RIGHT_PANEL_WIDTH  220
#define UI_PANEL_GAP          16

void ui_project_center_layout(int window_w, int window_h,
                               ui_rect_t *out_searchbar,
                               ui_rect_t *out_project_panel,
                               ui_rect_t *out_buttons_panel);

void ui_project_center_draw(int window_w, int window_h);

void ui_project_center_shutdown(void);

#endif
