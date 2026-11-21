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

#include "editor/ui/theme.h"
#include <stdio.h>
#include <string.h>
#include "editor/dialogs/emulator_select.h"
#include "editor/panels/file_manager.h"
#include "editor/ui/dialog.h"
#include "editor/ui/draw_kit.h"
#include "platform/common/window.h"
#include "run/emulator/emulator_catalog.h"
#include "run/run_mode.h"
#include "run/run_settings.h"

#define DIALOG_W 580
#define DIALOG_H 440
#define PAD 22
#define TITLE_H 44
#define ROW_H 76
#define ROW_GAP 10
#define MAX_ROWS 4

static int g_open = 0;
static int g_skip = 0;
static project_type_t g_system = PROJECT_TYPE_PS2;
static char g_notice[200];
static const emulator_info_t *g_rows[MAX_ROWS];
static int g_row_count = 0;
static const emulator_info_t *g_locating = NULL;

static void collect(void) {
    g_row_count = 0;
    for (int i = 0; i < emulator_catalog_count() && g_row_count < MAX_ROWS; i++) {
        const emulator_info_t *e = emulator_catalog_get(i);
        if (e->system == g_system && emulator_catalog_supported_here(e)) g_rows[g_row_count++] = e;
    }
}

void emulator_select_dialog_open(project_type_t system, const char *notice) {
    g_system = system;
    snprintf(g_notice, sizeof(g_notice), "%s", notice ? notice : "");
    g_locating = NULL;
    collect();
    g_skip = 2;
    g_open = 1;
}

int emulator_select_dialog_is_open(void) { return g_open; }

static void on_located(const char *path, void *user) {
    (void)user;
    const emulator_info_t *emu = g_locating;
    g_locating = NULL;
    if (!path || !emu) return;
    if (!emulator_path_matches(emu, path)) {
        snprintf(g_notice, sizeof(g_notice), "That file is not %s. Pick the %s program file", emu->name, emu->name);
        return;
    }
    emulator_set_executable(emu, path);
    emulator_select(g_system, emu->id);
    g_notice[0] = '\0';
}

typedef struct { int x, y, w, h; } rect_t;

static void row_rects(int dx, int dy, int i, rect_t *row, rect_t *select, rect_t *locate) {
    row->x = dx + PAD; row->y = dy + TITLE_H + 50 + i * (ROW_H + ROW_GAP); row->w = DIALOG_W - PAD * 2; row->h = ROW_H;
    select->w = 96; select->h = 34; select->x = row->x + row->w - 12 - select->w; select->y = row->y + (ROW_H - 34) / 2;
    locate->w = 96; locate->h = 34; locate->x = select->x - 8 - locate->w; locate->y = select->y;
}

void emulator_select_dialog_update(int window_w, int window_h) {
    if (!g_open) return;
    if (g_skip > 0) { g_skip--; return; }
    int dx = (window_w - DIALOG_W) / 2, dy = (window_h - DIALOG_H) / 2;
    if (!window_mouse_left_just_pressed()) return;
    int mx = window_mouse_x(), my = window_mouse_y();
    int cx = dx + DIALOG_W - PAD - 24, cy = dy + (TITLE_H - 24) / 2;
    int bx = dx + DIALOG_W - PAD - 120, by = dy + DIALOG_H - PAD - 36;
    if (ui_dialog_close_button_hit(mx, my, cx, cy, 24) || ui_kit_clicked(bx, by, 120, 36)) { g_open = 0; return; }
    for (int i = 0; i < g_row_count; i++) {
        rect_t row, select, locate;
        row_rects(dx, dy, i, &row, &select, &locate);
        const emulator_info_t *e = g_rows[i];
        int found = emulator_catalog_is_installed(e);
        if (run_host_is_desktop() && ui_kit_clicked(locate.x, locate.y, locate.w, locate.h)) {
            g_locating = e;
            file_manager_open(FILE_MANAGER_ROOT_DEVICE, FILE_MANAGER_MODE_PICK_FILE, NULL, 0, on_located, NULL);
            return;
        }
        if (ui_kit_clicked(select.x, select.y, select.w, select.h)) {
            if (!run_host_is_desktop() && !found) emulator_set_executable(e, "android-app");
            if (run_host_is_desktop() && !found) return;
            emulator_select(g_system, e->id);
            g_notice[0] = '\0';
        }
    }
}

void emulator_select_dialog_draw(int window_w, int window_h) {
    if (!g_open) return;
    int dx = (window_w - DIALOG_W) / 2, dy = (window_h - DIALOG_H) / 2;
    ui_dialog_draw_backdrop(window_w, window_h);
    ui_dialog_draw_frame(dx, dy, DIALOG_W, DIALOG_H);
    const char *title = g_system == PROJECT_TYPE_PS1 ? "PlayStation 1 - choose emulator" : "PlayStation 2 - choose emulator";
    ui_kit_text(title, 17, dx + PAD, dy + (TITLE_H - ui_kit_text_height(title, 17)) / 2, 235, 235, 235);
    ui_dialog_draw_close_button(dx + DIALOG_W - PAD - 24, dy + (TITLE_H - 24) / 2, 24);

    const char *hint = g_notice[0] ? g_notice : "Rebax opens the selected emulator directly and gives it your game.";
    ui_kit_text(hint, 13, dx + PAD, dy + TITLE_H + 14, g_notice[0] ? 230 : 160, g_notice[0] ? 180 : 166, g_notice[0] ? 80 : 174);

    const char *selected = emulator_selected_id(g_system);
    for (int i = 0; i < g_row_count; i++) {
        rect_t row, select, locate;
        row_rects(dx, dy, i, &row, &select, &locate);
        const emulator_info_t *e = g_rows[i];
        int found = emulator_catalog_is_installed(e);
        int chosen = strcmp(selected, e->id) == 0;
        ui_kit_rect(row.x, row.y, row.w, row.h, 9, 36, 37, 42);
        if (chosen) ui_kit_outline(row.x, row.y, row.w, row.h, 9, UI_RGB(UI_COLOR_INFO));
        ui_kit_text(e->name, 16, row.x + 16, row.y + 14, 235, 238, 242);
        const char *state = chosen ? "Selected" : (found ? "Found" : (run_host_is_desktop() ? "Not found - press Locate" : "Press Select if it is installed"));
        ui_kit_text(state, 12, row.x + 16, row.y + 44, chosen ? 110 : 150, chosen ? 190 : 156, chosen ? 235 : 165);
        if (run_host_is_desktop()) ui_kit_button("Locate", locate.x, locate.y, locate.w, locate.h, 1, 0);
        ui_kit_button(chosen ? "Selected" : "Select", select.x, select.y, select.w, select.h, !chosen && (found || !run_host_is_desktop()), 1);
    }
    if (g_row_count == 0) ui_kit_text("No supported emulator for this system on this device.", 14, dx + PAD, dy + TITLE_H + 70, 200, 204, 210);
    ui_kit_button("Close", dx + DIALOG_W - PAD - 120, dy + DIALOG_H - PAD - 36, 120, 36, 1, 0);
}

void emulator_select_dialog_shutdown(void) { g_open = 0; }
