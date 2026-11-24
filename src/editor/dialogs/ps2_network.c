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
#include "editor/dialogs/ps2_network.h"
#include "editor/panels/file_manager.h"
#include "editor/ui/dialog.h"
#include "editor/ui/windows.h"
#include "editor/ui/draw_kit.h"
#include "editor/ui/text_field.h"
#include "libraries/ps2link/ps2link_probe.h"
#include "platform/common/window.h"
#include "run/device/ps2_link_files.h"
#include "run/run_mode.h"
#include "run/run_settings.h"

#define DEFAULT_W 600
#define DEFAULT_H 500
#define DIALOG_W g_dw
#define DIALOG_H g_dh
static int g_dw = DEFAULT_W, g_dh = DEFAULT_H;
static ui_dialog_drag_t g_drag;
static ui_window_t g_win = UI_WINDOW_INIT(ps2_network_dialog_is_open, ps2_network_dialog_update, ps2_network_dialog_draw);
static int g_interactive = 0;
#define PAD 22
#define TITLE_H 44
#define BUTTON_H 36

typedef enum { EXPORT_NONE, EXPORT_ELF, EXPORT_CONFIG } export_kind_t;

static int g_open = 0;
static int g_ready = 0;
static int g_skip = 0;
static text_field_t g_address;
static char g_message[300];
static int g_message_ok = 0;
static export_kind_t g_pending = EXPORT_NONE;
static char g_tested_address[64];

static void ensure_ready(void) {
    if (g_ready) return;
    g_ready = 1;
    text_field_init(&g_address);
}

void ps2_network_dialog_open(void) {
    ensure_ready();
    snprintf(g_address.text, sizeof(g_address.text), "%s", run_settings_ps2_address());
    g_address.cursor_pos = (int)strlen(g_address.text);
    g_address.scroll_offset = 0;
    g_address.focused = 0;
    g_message[0] = '\0';
    g_tested_address[0] = '\0';
    ps2link_probe_reset();
    g_pending = EXPORT_NONE;
    g_skip = 2;
    g_open = 1;
    ui_dialog_drag_reset(&g_drag);
    ui_windows_show(&g_win);
}

int ps2_network_dialog_is_open(void) { return g_open; }

static void close_dialog(void) {
    window_stop_text_input();
    g_address.focused = 0;
    g_open = 0;
}

static void on_folder(const char *path, void *user) {
    (void)user;
    if (!path) { g_pending = EXPORT_NONE; return; }
    char error[200] = "";
    int ok;
    if (g_pending == EXPORT_ELF) ok = ps2_link_export_elf(path, error, sizeof(error));
    else ok = ps2_link_export_ipconfig(path, g_address.text, error, sizeof(error));
    if (ok) snprintf(g_message, sizeof(g_message), "%s exported successfully", g_pending == EXPORT_ELF ? "ps2link.elf" : "IPCONFIG.DAT");
    else snprintf(g_message, sizeof(g_message), "%s", error);
    g_message_ok = ok;
    g_pending = EXPORT_NONE;
}

typedef struct { int x, y, w, h; } rect_t;

typedef struct {
    rect_t close, test, export_elf, export_cfg, done, cancel, field;
} layout_t;

static void compute(int window_w, int window_h, layout_t *l, int *dx, int *dy) {
    ui_dialog_place(&g_drag, g_interactive, window_w, window_h, DEFAULT_W, DEFAULT_H, DEFAULT_W, DEFAULT_H, TITLE_H, 1, dx, dy, &g_dw, &g_dh);
    int x = *dx, y = *dy;
    l->close = (rect_t){ x + DIALOG_W - PAD - 24, y + (TITLE_H - 24) / 2, 24, 24 };
    l->test = (rect_t){ x + DIALOG_W - PAD - 150, y + TITLE_H + 10 + 5, 140, 34 };
    l->field = (rect_t){ x + PAD + 190, y + TITLE_H + 66, 200, 32 };
    l->export_elf = (rect_t){ x + PAD, y + TITLE_H + 266, (DIALOG_W - PAD * 2 - 12) / 2, BUTTON_H };
    l->export_cfg = (rect_t){ x + PAD + (DIALOG_W - PAD * 2 - 12) / 2 + 12, y + TITLE_H + 266, (DIALOG_W - PAD * 2 - 12) / 2, BUTTON_H };
    l->done = (rect_t){ x + DIALOG_W - PAD - 120, y + DIALOG_H - PAD - BUTTON_H, 120, BUTTON_H };
    l->cancel = (rect_t){ x + DIALOG_W - PAD - 120 - 12 - 120, y + DIALOG_H - PAD - BUTTON_H, 120, BUTTON_H };
}

void ps2_network_dialog_update(int window_w, int window_h) {
    if (!g_open) return;
    if (g_skip > 0) { g_skip--; return; }
    layout_t l; int dx, dy;
    g_interactive = 1;
    compute(window_w, window_h, &l, &dx, &dy);

    int was = g_address.focused;
    text_field_update(&g_address, l.field.x, l.field.y, l.field.w, l.field.h, l.field.w);
    if (g_address.focused != was) { if (g_address.focused) window_start_text_input(); else window_stop_text_input(); }

    if (strcmp(g_address.text, g_tested_address) != 0 && ps2link_probe_state() != PS2LINK_PROBE_CHECKING) ps2link_probe_reset();

    int valid = run_settings_valid_address(g_address.text);
    if (!window_mouse_left_just_pressed()) return;
    int mx = window_mouse_x(), my = window_mouse_y();
    if (ui_dialog_close_button_hit(mx, my, l.close.x, l.close.y, l.close.w) || ui_kit_clicked(l.cancel.x, l.cancel.y, l.cancel.w, l.cancel.h)) { close_dialog(); return; }
    if (valid && ui_kit_clicked(l.test.x, l.test.y, l.test.w, l.test.h)) {
        snprintf(g_tested_address, sizeof(g_tested_address), "%s", g_address.text);
        ps2link_probe_start(g_address.text);
        g_message[0] = '\0';
    }
    if (ui_kit_clicked(l.export_elf.x, l.export_elf.y, l.export_elf.w, l.export_elf.h)) {
        if (!ps2_link_elf_available()) { snprintf(g_message, sizeof(g_message), "ps2link.elf is missing. Update the PlayStation 2 package"); g_message_ok = 0; }
        else { g_pending = EXPORT_ELF; file_manager_open(FILE_MANAGER_ROOT_DEVICE, FILE_MANAGER_MODE_PICK_FOLDER, NULL, 0, on_folder, NULL); }
    }
    if (valid && ui_kit_clicked(l.export_cfg.x, l.export_cfg.y, l.export_cfg.w, l.export_cfg.h)) {
        g_pending = EXPORT_CONFIG;
        file_manager_open(FILE_MANAGER_ROOT_DEVICE, FILE_MANAGER_MODE_PICK_FOLDER, NULL, 0, on_folder, NULL);
    }
    if (valid && ui_kit_clicked(l.done.x, l.done.y, l.done.w, l.done.h)) {
        run_settings_set_ps2_address(g_address.text);
        run_settings_set_ps2_configured(1);
        close_dialog();
    }
}

static const char *step_text(int i) {
    static const char *steps[4] = {
        "Connect the PlayStation 2 to this device with a network cable.",
        "Export IPCONFIG.DAT and ps2link.elf to a USB drive (buttons below).",
        "Put IPCONFIG.DAT in mc0:/SYS-CONF/ and run ps2link.elf.",
        "Press Test connection. When it turns green, press Done."
    };
    return steps[i];
}

void ps2_network_dialog_draw(int window_w, int window_h) {
    if (!g_open) return;
    layout_t l; int dx, dy;
    g_interactive = 0;
    compute(window_w, window_h, &l, &dx, &dy);
    ui_dialog_draw_backdrop(window_w, window_h);
    ui_dialog_draw_frame(dx, dy, DIALOG_W, DIALOG_H);
    ui_kit_text("PlayStation 2 - network run", 17, dx + PAD, dy + (TITLE_H - ui_kit_text_height("PlayStation 2 - network run", 17)) / 2, 235, 235, 235);
    ui_dialog_draw_close_button(l.close.x, l.close.y, l.close.w);

    int card_x = dx + PAD, card_w = DIALOG_W - PAD * 2;
    int sy = dy + TITLE_H + 10;
    ui_kit_rect(card_x, sy, card_w, 44, 9, 36, 37, 42);
    ps2link_probe_state_t st = ps2link_probe_state();
    int r = 120, g = 124, b = 132;
    const char *line = "Not tested yet";
    char buf[160];
    if (st == PS2LINK_PROBE_CHECKING) { r = 224; g = 178; b = 60; line = "Checking..."; }
    else if (st == PS2LINK_PROBE_REACHABLE) { r = 80; g = 200; b = 110; snprintf(buf, sizeof(buf), "Connected - PlayStation 2 found at %s", g_tested_address); line = buf; }
    else if (st == PS2LINK_PROBE_UNREACHABLE) { r = 220; g = 80; b = 80; snprintf(buf, sizeof(buf), "Not connected - no answer from %s", g_tested_address); line = buf; }
    ui_kit_rect(card_x + 16, sy + 15, 14, 14, 7, r, g, b);
    ui_kit_text(line, 14, card_x + 40, sy + (44 - ui_kit_text_height(line, 14)) / 2, 225, 228, 232);
    ui_kit_button("Test connection", l.test.x, l.test.y, l.test.w, l.test.h, run_settings_valid_address(g_address.text) && st != PS2LINK_PROBE_CHECKING, 0);

    ui_kit_text("PlayStation 2 address", 14, card_x, l.field.y + 8, 190, 195, 202);
    text_field_draw(&g_address, l.field.x, l.field.y, l.field.w, l.field.h, 14);
    if (!run_settings_valid_address(g_address.text)) ui_kit_text("Invalid address", 12, l.field.x + l.field.w + 12, l.field.y + 8, 220, 90, 90);

    int py = dy + TITLE_H + 108;
    ui_kit_rect(card_x, py, card_w, 148, 9, 36, 37, 42);
    int ty = py + 10;
    for (int i = 0; i < 4; i++) {
        char num[8];
        snprintf(num, sizeof(num), "%d", i + 1);
        ui_kit_rect(card_x + 14, ty + 1, 20, 20, 10, UI_RGB(UI_COLOR_BUTTON_BLUE));
        ui_kit_text(num, 12, card_x + 14 + (20 - ui_kit_text_width(num, 12)) / 2, ty + 3, 240, 240, 240);
        ui_kit_text(step_text(i), 13, card_x + 44, ty + 2, 210, 214, 220);
        ty += 24;
    }
    ui_kit_text("Android / iPhone: use a USB-C to Ethernet (RJ45) adapter (OTG).", 12, card_x + 14, ty + 2, 150, 156, 165);
    ui_kit_text("Set this device's Ethernet address to the same network, e.g. 192.168.0.20.", 12, card_x + 14, ty + 18, 150, 156, 165);

    int valid = run_settings_valid_address(g_address.text);
    ui_kit_button("Export ps2link.elf", l.export_elf.x, l.export_elf.y, l.export_elf.w, l.export_elf.h, 1, 0);
    ui_kit_button("Export IPCONFIG.DAT", l.export_cfg.x, l.export_cfg.y, l.export_cfg.w, l.export_cfg.h, valid, 0);
    if (g_message[0]) ui_kit_text(g_message, 13, card_x, l.export_elf.y + BUTTON_H + 12, g_message_ok ? 110 : 225, g_message_ok ? 200 : 100, g_message_ok ? 120 : 100);

    ui_kit_button("Cancel", l.cancel.x, l.cancel.y, l.cancel.w, l.cancel.h, 1, 0);
    ui_kit_button("Done", l.done.x, l.done.y, l.done.w, l.done.h, valid, 1);
}

void ps2_network_dialog_shutdown(void) {
    if (g_ready) text_field_free(&g_address);
    g_ready = 0;
    g_open = 0;
}
