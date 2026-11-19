/*
 *  ____  _____ ____    _    __  __
 * |  _ \| ____| __ )  / \   \ \/ /
 * | |_) | _| | _ \ / _ \   \  /
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
#include "editor/panels/run_controls.h"
#include "editor/dialogs/packages.h"
#include "editor/dialogs/ps2_network.h"
#include "editor/dialogs/emulator_select.h"
#include "editor/ui/base.h"
#include "editor/ui/draw_kit.h"
#include "editor/ui/icon_atlas.h"
#include "editor/ui/theme.h"
#include "platform/common/window.h"
#include "project/current_project.h"
#include "run/run_mode.h"
#include "run/run_session.h"
#include "run/run_settings.h"
#include "run/emulator/emulator_catalog.h"

#define GROUP_H 32
#define PLAY_W 40
#define MORE_W 30
#define ICON_PX 24
#define MENU_W 380
#define ITEM_H 44
#define ITEM_MARGIN 5
#define SETTINGS_W 42

static int g_menu_open = 0;
static run_mode_t g_selected = RUN_MODE_SIMULATION;
static char g_notice[200];

static project_type_t current_type(void) {
    const project_settings_t *s = current_project_get_settings();
    return s ? s->type : PROJECT_TYPE_MODERN;
}

typedef struct { int group_x, group_y, group_w, play_x, more_x, menu_x, menu_y; } layout_t;

static void layout(int window_w, layout_t *l) {
    l->group_w = PLAY_W + 1 + MORE_W + 6;
    l->group_x = (window_w - l->group_w) / 2;
    l->group_y = (RUN_CONTROLS_HEIGHT - GROUP_H) / 2;
    l->play_x = l->group_x + 3;
    l->more_x = l->play_x + PLAY_W + 1;
    l->menu_x = window_w / 2 - MENU_W / 2;
    l->menu_y = l->group_y + GROUP_H + 6;
}

static int selected_index(const run_mode_info_t *list, int n) {
    for (int i = 0; i < n; i++) if (list[i].mode == g_selected && list[i].available) return i;
    return 0;
}

static void open_emulator_window(project_type_t type, int missing) {
    emulator_select_dialog_open(type, missing ? "The selected emulator was not found. Choose another one." : "");
}

static int needs_setup(run_mode_t mode, project_type_t type) {
    if (mode == RUN_MODE_EMULATOR) return !emulator_selection_valid(type);
    if (mode == RUN_MODE_PS2_DEVICE) return !run_settings_ps2_configured();
    return 0;
}

static void open_setup(run_mode_t mode, project_type_t type) {
    if (mode == RUN_MODE_EMULATOR) open_emulator_window(type, run_settings_emulator_id(type)[0] != '\0');
    else if (mode == RUN_MODE_PS2_DEVICE) ps2_network_dialog_open();
}

void run_controls_update(int window_w) {
    run_session_update();
    int mx = window_mouse_x(), my = window_mouse_y();
    int clicked = window_mouse_left_just_pressed();
    layout_t l;
    layout(window_w, &l);
    project_type_t type = current_type();
    run_mode_info_t list[4];
    int n = run_modes_for_project(type, list, 4);

    if (g_menu_open) {
        if (!clicked) return;
        g_menu_open = 0;
        for (int i = 0; i < n; i++) {
            int y = l.menu_y + ITEM_MARGIN + i * ITEM_H;
            if (!ui_point_in_rect(mx, my, l.menu_x, y, MENU_W, ITEM_H)) continue;
            int on_settings = list[i].has_settings && mx >= l.menu_x + MENU_W - ITEM_MARGIN - SETTINGS_W;
            if (!list[i].available) {
                snprintf(g_notice, sizeof(g_notice), "%s", list[i].reason);
                packages_dialog_open();
                return;
            }
            g_selected = list[i].mode;
            g_notice[0] = '\0';
            if (on_settings) {
                if (list[i].mode == RUN_MODE_EMULATOR) open_emulator_window(type, 0);
                else ps2_network_dialog_open();
            } else if (needs_setup(list[i].mode, type)) open_setup(list[i].mode, type);
        }
        return;
    }
    if (!clicked) return;
    if (ui_point_in_rect(mx, my, l.more_x, l.group_y, MORE_W, GROUP_H) && !run_session_is_running()) { g_menu_open = 1; return; }
    if (ui_point_in_rect(mx, my, l.play_x, l.group_y, PLAY_W, GROUP_H)) {
        if (run_session_is_running()) { run_session_stop(); return; }
        run_mode_t mode = list[selected_index(list, n)].mode;
        g_notice[0] = '\0';
        if (needs_setup(mode, type)) { open_setup(mode, type); return; }
        run_session_start(mode, type);
    }
}

int run_controls_menu_is_open(void) { return g_menu_open; }

void run_controls_draw(int window_w) {
    layout_t l;
    layout(window_w, &l);
    int running = run_session_is_running();
    ui_kit_rect(l.group_x, l.group_y, l.group_w, GROUP_H, 9, 46, 48, 54);
    ui_kit_outline(l.group_x, l.group_y, l.group_w, GROUP_H, 9, 78, 81, 89);
    if (ui_kit_hover(l.play_x, l.group_y, PLAY_W, GROUP_H)) ui_kit_rect(l.play_x, l.group_y + 2, PLAY_W, GROUP_H - 4, 7, 66, 69, 77);
    if (!running && (g_menu_open || ui_kit_hover(l.more_x, l.group_y, MORE_W, GROUP_H))) ui_kit_rect(l.more_x, l.group_y + 2, MORE_W, GROUP_H - 4, 7, 66, 69, 77);
    window_fill_rect(l.more_x - 1, l.group_y + 7, 1, GROUP_H - 14, 84, 87, 95);
    icon_atlas_draw(running ? ICON_stop : ICON_tree_expand_collapse, l.play_x + (PLAY_W - ICON_PX) / 2, l.group_y + (GROUP_H - ICON_PX) / 2, ICON_PX);
    if (!running) icon_atlas_draw(ICON_more, l.more_x + (MORE_W - ICON_PX) / 2, l.group_y + (GROUP_H - ICON_PX) / 2, ICON_PX);
    const char *msg = run_session_message()[0] ? run_session_message() : g_notice;
    if (msg[0]) ui_kit_text(msg, 12, l.group_x + l.group_w + 12, (RUN_CONTROLS_HEIGHT - ui_kit_text_height(msg, 12)) / 2, 165, 170, 178);
}

void run_controls_draw_menu(int window_w) {
    if (!g_menu_open) return;
    layout_t l;
    layout(window_w, &l);
    project_type_t type = current_type();
    run_mode_info_t list[4];
    int n = run_modes_for_project(type, list, 4);
    int sel = selected_index(list, n);
    int mx = window_mouse_x(), my = window_mouse_y();
    int total_h = n * ITEM_H + ITEM_MARGIN * 2;
    ui_kit_rect(l.menu_x - 1, l.menu_y - 1, MENU_W + 2, total_h + 2, 11, 84, 87, 95);
    ui_kit_rect(l.menu_x, l.menu_y, MENU_W, total_h, 10, 32, 33, 38);
    for (int i = 0; i < n; i++) {
        int x = l.menu_x + ITEM_MARGIN, y = l.menu_y + ITEM_MARGIN + i * ITEM_H, w = MENU_W - ITEM_MARGIN * 2, h = ITEM_H - 4;
        int hover = ui_point_in_rect(mx, my, x, y, w, h);
        if (i == sel) ui_kit_rect(x, y, w, h, 8, UI_COLOR_BUTTON_BLUE.r, UI_COLOR_BUTTON_BLUE.g, UI_COLOR_BUTTON_BLUE.b);
        else if (hover) ui_kit_rect(x, y, w, h, 8, 52, 54, 61);
        int c = list[i].available ? 238 : 112;
        ui_kit_text(list[i].label, 14, x + 14, y + (h - ui_kit_text_height(list[i].label, 14)) / 2, c, c, c);
        if (list[i].has_settings && list[i].available) {
            int sx = x + w - SETTINGS_W;
            if (hover && mx >= sx) ui_kit_rect(sx, y + 3, SETTINGS_W - 3, h - 6, 7, 70, 90, 110);
            window_fill_rect(sx - 1, y + 8, 1, h - 16, 90, 105, 120);
            icon_atlas_draw(ICON_more, sx + (SETTINGS_W - 3 - 20) / 2, y + (h - 20) / 2, 20);
        }
    }
}

void run_controls_shutdown(void) {
    run_session_stop();
}
