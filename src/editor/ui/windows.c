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


#include <stddef.h>

#include "editor/ui/windows.h"
#include "platform/common/window.h"

#define MAX_WINDOWS 24
#define HIT_MARGIN 6

static ui_window_t *g_windows[MAX_WINDOWS];
static int g_count = 0;
static unsigned g_next_order = 1;
static ui_window_t *g_current = NULL;
static ui_window_t *g_front = NULL;

static int window_open(const ui_window_t *w) {
    return w->is_open != NULL && w->is_open();
}

static ui_window_t *find_front(void) {
    ui_window_t *front = NULL;
    for (int i = 0; i < g_count; i++) {
        if (!window_open(g_windows[i])) continue;
        if (front == NULL || g_windows[i]->order > front->order) front = g_windows[i];
    }
    return front;
}

void ui_windows_show(ui_window_t *window) {
    if (window == NULL) return;
    if (!window->registered && g_count < MAX_WINDOWS) {
        g_windows[g_count++] = window;
        window->registered = 1;
    }
    window->order = g_next_order++;
    window->w = 0;
    window->h = 0;
    window->armed = 0;
}

int ui_windows_any_open(void) {
    return find_front() != NULL;
}

static int contains(const ui_window_t *w, int mx, int my) {
    if (w->w <= 0 || w->h <= 0) return 0;
    return mx >= w->x - HIT_MARGIN && mx < w->x + w->w + HIT_MARGIN &&
           my >= w->y - HIT_MARGIN && my < w->y + w->h + HIT_MARGIN;
}

static void raise_under_mouse(void) {
    int mx = window_mouse_x(), my = window_mouse_y();
    ui_window_t *hit = NULL;
    for (int i = 0; i < g_count; i++) {
        ui_window_t *w = g_windows[i];
        if (!window_open(w) || !contains(w, mx, my)) continue;
        if (hit == NULL || w->order > hit->order) hit = w;
    }
    if (hit != NULL && hit != find_front()) hit->order = g_next_order++;
}

void ui_windows_update(int window_w, int window_h) {
    if (find_front() == NULL) return;
    if (window_mouse_left_just_pressed()) raise_under_mouse();
    ui_window_t *front = find_front();
    if (front == NULL || front->update == NULL) return;
    if (!front->armed) {
        if (window_mouse_left_down() || window_mouse_left_just_pressed()) return;
        front->armed = 1;
    }
    g_current = front;
    g_front = front;
    front->update(window_w, window_h);
    g_current = NULL;
    g_front = NULL;
}

void ui_windows_draw(int window_w, int window_h) {
    ui_window_t *open[MAX_WINDOWS];
    int n = 0;
    for (int i = 0; i < g_count; i++) if (window_open(g_windows[i])) open[n++] = g_windows[i];
    for (int i = 1; i < n; i++) {
        ui_window_t *key = open[i];
        int j = i - 1;
        while (j >= 0 && open[j]->order > key->order) { open[j + 1] = open[j]; j--; }
        open[j + 1] = key;
    }
    if (n == 0) return;
    g_front = open[n - 1];
    for (int i = 0; i < n; i++) {
        if (open[i]->draw == NULL) continue;
        g_current = open[i];
        open[i]->draw(window_w, window_h);
    }
    g_current = NULL;
    g_front = NULL;
}

int ui_windows_context_is_front(void) {
    return g_current == NULL || g_current == g_front;
}

void ui_windows_context_report(int x, int y, int w, int h) {
    if (g_current == NULL) return;
    g_current->x = x;
    g_current->y = y;
    g_current->w = w;
    g_current->h = h;
}
