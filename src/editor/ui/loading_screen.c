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

#include <math.h>

#include "editor/ui/loading_screen.h"
#include "platform/common/window.h"
#include "editor/ui/theme.h"

#define DOT_COUNT        8
#define DOT_SIZE         6
#define DOT_SIZE_ACTIVE  12
#define ORBIT_RADIUS     34
#define FRAMES_PER_STEP  6
#define AUTO_HIDE_FRAMES 90

static int g_visible = 0;
static int g_frame_counter = 0;
static int g_just_finished = 0;
static int g_auto_hide = 1;

void loading_screen_show(void) {
    g_visible = 1;
    g_frame_counter = 0;
    g_just_finished = 0;
    g_auto_hide = 1;
}

void loading_screen_show_indefinite(void) {
    g_visible = 1;
    g_frame_counter = 0;
    g_just_finished = 0;
    g_auto_hide = 0;
}

void loading_screen_hide(void) {
    g_visible = 0;
}

int loading_screen_is_visible(void) {
    return g_visible;
}

int loading_screen_just_finished(void) {
    if (g_just_finished) {
        g_just_finished = 0;
        return 1;
    }
    return 0;
}

void loading_screen_update(void) {
    if (!g_visible) {
        return;
    }
    g_frame_counter++;
    if (g_auto_hide && g_frame_counter >= AUTO_HIDE_FRAMES) {
        g_visible = 0;
        g_just_finished = 1;
    }
}

void loading_screen_draw(int window_w, int window_h) {
    if (!g_visible) {
        return;
    }

    window_clear(UI_COLOR_BLACK_MUTED.r, UI_COLOR_BLACK_MUTED.g, UI_COLOR_BLACK_MUTED.b);

    int cx = window_w / 2;
    int cy = window_h / 2;
    int active = (g_frame_counter / FRAMES_PER_STEP) % DOT_COUNT;

    for (int i = 0; i < DOT_COUNT; i++) {
        double angle = (2.0 * 3.14159265358979 * i) / DOT_COUNT;
        int dx = cx + (int)(cos(angle) * ORBIT_RADIUS);
        int dy = cy + (int)(sin(angle) * ORBIT_RADIUS);

        int is_active = (i == active);
        int size = is_active ? DOT_SIZE_ACTIVE : DOT_SIZE;

        window_fill_rect(dx - size / 2, dy - size / 2, size, size,
                          UI_COLOR_BUTTON_BLUE.r, UI_COLOR_BUTTON_BLUE.g, UI_COLOR_BUTTON_BLUE.b);
    }
}
