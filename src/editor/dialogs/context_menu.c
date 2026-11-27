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

#include <stdio.h>
#include <string.h>

#include "core/i18n.h"
#include "editor/dialogs/context_menu.h"
#include "editor/ui/base.h"
#include "editor/ui/theme.h"
#include "editor/ui/windows.h"
#include "platform/common/window.h"

#define MENU_WIDTH 200
#define ITEM_HEIGHT 34
#define SEPARATOR_HEIGHT 7
#define MENU_PAD 4

static int g_open = 0;
static int g_x, g_y, g_w, g_h;
static int g_count = 0;
static context_menu_item_t g_items[CONTEXT_MENU_MAX_ITEMS];
static char g_labels[CONTEXT_MENU_MAX_ITEMS][96];
static window_texture_t *g_text[CONTEXT_MENU_MAX_ITEMS];
static int g_text_w[CONTEXT_MENU_MAX_ITEMS], g_text_h[CONTEXT_MENU_MAX_ITEMS];
static context_menu_callback_t g_callback = NULL;
static void *g_user = NULL;
static int g_hover = -1;
static int g_want_x, g_want_y;
static ui_window_t g_win = UI_WINDOW_INIT(context_menu_is_open, context_menu_update, context_menu_draw);

static void free_textures(void) {
    for (int i = 0; i < CONTEXT_MENU_MAX_ITEMS; i++) {
        if (g_text[i]) window_destroy_texture(g_text[i]);
        g_text[i] = NULL;
    }
}

static int item_y(int index) {
    int y = g_y + MENU_PAD;
    for (int i = 0; i < index; i++) {
        if (g_items[i].separator_before) y += SEPARATOR_HEIGHT;
        y += ITEM_HEIGHT;
    }
    if (g_items[index].separator_before) y += SEPARATOR_HEIGHT;
    return y;
}

void context_menu_open(int x, int y, const context_menu_item_t *items, int count,
                       context_menu_callback_t callback, void *user_data) {
    if (items == NULL || count <= 0) return;
    if (count > CONTEXT_MENU_MAX_ITEMS) count = CONTEXT_MENU_MAX_ITEMS;
    free_textures();
    g_count = count;
    g_h = MENU_PAD * 2;
    for (int i = 0; i < count; i++) {
        g_items[i] = items[i];
        snprintf(g_labels[i], sizeof(g_labels[i]), "%s", tr(items[i].label));
        g_items[i].label = g_labels[i];
        g_text[i] = ui_make_text_texture(g_labels[i], FONT_WEIGHT_REGULAR, 13, &g_text_w[i], &g_text_h[i]);
        g_h += ITEM_HEIGHT + (items[i].separator_before ? SEPARATOR_HEIGHT : 0);
    }
    g_w = MENU_WIDTH;
    g_want_x = x;
    g_want_y = y;
    g_callback = callback;
    g_user = user_data;
    g_hover = -1;
    g_open = 1;
    ui_windows_show(&g_win);
}

int context_menu_is_open(void) { return g_open; }

static void place(int window_w, int window_h) {
    g_x = g_want_x;
    g_y = g_want_y;
    if (g_x + g_w > window_w - 6) g_x = window_w - g_w - 6;
    if (g_y + g_h > window_h - 6) g_y = window_h - g_h - 6;
    if (g_x < 6) g_x = 6;
    if (g_y < 6) g_y = 6;
}

void context_menu_update(int window_w, int window_h) {
    if (!g_open) return;
    place(window_w, window_h);
    int mx = window_mouse_x(), my = window_mouse_y();
    g_hover = -1;
    for (int i = 0; i < g_count; i++) {
        int y = item_y(i);
        if (g_items[i].enabled && ui_point_in_rect(mx, my, g_x + MENU_PAD, y, g_w - MENU_PAD * 2, ITEM_HEIGHT)) g_hover = i;
    }
    if (window_key_just_pressed_escape()) { g_open = 0; return; }
    if (window_mouse_left_just_pressed() || window_mouse_right_just_pressed()) {
        int inside = ui_point_in_rect(mx, my, g_x, g_y, g_w, g_h);
        int chosen = (inside && g_hover >= 0) ? g_items[g_hover].id : -1;
        g_open = 0;
        if (chosen >= 0 && g_callback) g_callback(chosen, g_user);
    }
}

void context_menu_draw(int window_w, int window_h) {
    if (!g_open) return;
    place(window_w, window_h);
    window_fill_rect(g_x + 3, g_y + 4, g_w, g_h, 6, 8, 12);
    window_fill_rect(g_x - 1, g_y - 1, g_w + 2, g_h + 2, UI_RGB(UI_COLOR_SURFACE_BORDER));
    window_fill_rect(g_x, g_y, g_w, g_h, UI_RGB(UI_COLOR_SURFACE_PANEL));
    for (int i = 0; i < g_count; i++) {
        int y = item_y(i);
        if (g_items[i].separator_before) window_fill_rect(g_x + 10, y - SEPARATOR_HEIGHT / 2, g_w - 20, 1, UI_RGB(UI_COLOR_SURFACE_BORDER));
        if (i == g_hover) window_fill_rect(g_x + MENU_PAD, y, g_w - MENU_PAD * 2, ITEM_HEIGHT, UI_RGB(UI_COLOR_HOVER_BLUE));
        if (g_text[i]) {
            ui_color_t c = g_items[i].enabled ? UI_COLOR_TEXT_PRIMARY : UI_COLOR_TEXT_DISABLED;
            window_draw_texture_tinted(g_text[i], g_x + 14, y + (ITEM_HEIGHT - g_text_h[i]) / 2, g_text_w[i], g_text_h[i], UI_RGB(c));
        }
    }
}

void context_menu_shutdown(void) {
    free_textures();
    g_open = 0;
}
