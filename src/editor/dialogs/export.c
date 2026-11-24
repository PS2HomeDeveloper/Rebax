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
#include <stdlib.h>
#include <string.h>
#include "editor/dialogs/export.h"
#include "platform/common/window.h"
#include "editor/ui/font.h"
#include "editor/ui/text_field.h"
#include "editor/ui/theme.h"
#include "editor/ui/base.h"
#include "editor/ui/dialog.h"
#include "editor/ui/windows.h"
#include "editor/ui/labeled_button.h"
#include "editor/panels/file_manager.h"
#include "editor/panels/scene_tabs.h"
#include "exporters/compiler/ps2/ps2_exporter.h"
#include "exporters/compiler/ps1/ps1_exporter.h"
#include "exporters/compiler/modern/modern_exporter.h"
#include "project/current_project.h"
#include "project/project_types.h"
#include "packages/package_manager.h"
#include "platform/android/storage.h"

#define PAGE_DEF_W 1040
#define PAGE_DEF_H 650
#define PAGE_W g_page_w
#define PAGE_H g_page_h
static int g_page_w = PAGE_DEF_W, g_page_h = PAGE_DEF_H;
static ui_dialog_drag_t g_drag;
static ui_window_t g_win = UI_WINDOW_INIT(export_dialog_is_open, export_dialog_update, export_dialog_draw);
#define PAD 24
#define CARD_W 142
#define CARD_H 84
#define CARD_GAP 10
#define FIELD_H 32
#define LOG_LINES 28
#define LOG_SHOWN 18
#define PLATFORM_COUNT 7
#define MODERN_COUNT 5
#define BTN_FONT 15
#define BTN_GAP 10

typedef struct { const char *id; const char *label; int package_id; unsigned project_flag; int modern; } export_platform_t;
static const export_platform_t g_platforms[PLATFORM_COUNT] = {
    {"android", "Android", 0, PROJECT_PLATFORM_ANDROID, 1},
    {"linux", "Linux", 1, PROJECT_PLATFORM_LINUX, 1},
    {"windows", "Windows", 2, PROJECT_PLATFORM_WINDOWS, 1},
    {"macos", "macOS", 3, PROJECT_PLATFORM_MACOS, 1},
    {"ios", "iOS", 4, PROJECT_PLATFORM_IOS, 1},
    {"ps2", "PlayStation 2", 5, 0, 0},
    {"ps1", "PlayStation 1", 6, 0, 0}
};
static const char *const g_android_arches[] = {"arm64-v8a", "armeabi-v7a", "x86", "x86_64"};
static const char *const g_desktop_arches[] = {"arm64", "arm", "x86", "x86_64"};

typedef struct { labeled_button_t b; window_texture_t *bg; window_texture_t *tx; } btn_t;
enum { B_EXPORT, B_CANCEL, B_CLOSE, B_DETAILS, B_HIDE, B_BROWSE, B_DEBUG_ON, B_DEBUG_OFF, B_RELEASE_ON, B_RELEASE_OFF, B_COUNT };
static const char *const g_btn_labels[B_COUNT] = {"Export", "Cancel", "Close", "Details", "Hide details", "Browse", "Debug", "Debug", "Release", "Release"};
static const int g_btn_blue[B_COUNT] = {1, 0, 0, 0, 1, 1, 1, 0, 1, 0};
static btn_t g_btn[B_COUNT];

static int g_open, g_ready, g_selected, g_started;
static int g_release, g_details;
static unsigned g_arch_mask;
static text_field_t g_name, g_path;
static char g_status_text[160];
static char g_error_text[240];
static char g_logs[LOG_LINES][220];
static int g_log_count;
static window_texture_t *g_title;
static int g_title_w, g_title_h;

static window_texture_t *text_texture(const char *s, int size, int *w, int *h) { return ui_make_text_texture(s, FONT_WEIGHT_REGULAR, size, w, h); }
static void draw_text(const char *s, int x, int y, int size, unsigned char r, unsigned char g, unsigned char b) {
    int w, h; window_texture_t *t = text_texture(s, size, &w, &h);
    if (t) { window_draw_texture_tinted(t, x, y, w, h, r, g, b); window_destroy_texture(t); }
}
static void btn_make(btn_t *btn, const char *label, int blue) {
    const ui_color_t *c = blue ? &UI_COLOR_BUTTON_BLUE : &UI_COLOR_GRAY_MUTED;
    btn->b = labeled_button_create(label, FONT_WEIGHT_REGULAR, BTN_FONT, UI_BUTTON_PADDING_X, UI_BUTTON_PADDING_Y, UI_BUTTON_RADIUS, c->r, c->g, c->b);
    if (btn->b.width > 0) {
        btn->bg = window_create_texture(btn->b.shape_img.pixels, btn->b.shape_img.px_w, btn->b.shape_img.px_h);
        btn->tx = window_create_texture(btn->b.text_img.pixels, btn->b.text_img.px_w, btn->b.text_img.px_h);
    }
}
static void btn_free(btn_t *btn) {
    if (btn->bg) window_destroy_texture(btn->bg);
    if (btn->tx) window_destroy_texture(btn->tx);
    labeled_button_free(&btn->b);
    memset(btn, 0, sizeof(*btn));
}
static void btn_draw(int id, int x, int y) {
    btn_t *btn = &g_btn[id];
    if (!btn->bg) return;
    window_draw_texture(btn->bg, x, y, btn->b.width, btn->b.height);
    if (btn->tx) window_draw_texture(btn->tx, x + btn->b.text_offset_x, y + btn->b.text_offset_y, btn->b.text_img.width, btn->b.text_img.height);
}
static int btn_hit(int id, int x, int y, int mx, int my) { return ui_point_in_rect(mx, my, x, y, g_btn[id].b.width, g_btn[id].b.height); }
static int btn_w(int id) { return g_btn[id].b.width; }
static int btn_h(void) { return g_btn[B_EXPORT].b.height; }

static int current_status(void) {
    if (!g_started) return 0;
    if (g_platforms[g_selected].modern) {
        modern_export_status_t s = modern_export_get_status();
        if (s == MODERN_EXPORT_RUNNING) return 1;
        if (s == MODERN_EXPORT_SUCCESS) return 2;
        if (s == MODERN_EXPORT_FAILED) return 3;
    } else if (g_selected == 5) {
        ps2_export_status_t s = ps2_export_get_status();
        if (s == PS2_EXPORT_STATUS_RUNNING) return 1;
        if (s == PS2_EXPORT_STATUS_SUCCESS) return 2;
        if (s == PS2_EXPORT_STATUS_FAILED) return 3;
    } else {
        ps1_export_status_t s = ps1_export_get_status();
        if (s == PS1_EXPORT_STATUS_RUNNING) return 1;
        if (s == PS1_EXPORT_STATUS_SUCCESS) return 2;
        if (s == PS1_EXPORT_STATUS_FAILED) return 3;
    }
    return 0;
}
static void clear_logs(void) { g_log_count = 0; memset(g_logs, 0, sizeof(g_logs)); }
static void push_log(const char *line) {
    if (!line || !line[0]) return;
    if (g_log_count < LOG_LINES) snprintf(g_logs[g_log_count++], sizeof(g_logs[0]), "%s", line);
    else { for (int i = 1; i < LOG_LINES; i++) snprintf(g_logs[i - 1], sizeof(g_logs[0]), "%s", g_logs[i]); snprintf(g_logs[LOG_LINES - 1], sizeof(g_logs[0]), "%s", line); }
}
static void pump_logs(void) {
    const char *line;
    while ((line = ps2_export_poll_next_line()) != NULL) push_log(line);
    while ((line = ps1_export_poll_next_line()) != NULL) push_log(line);
}
static int project_modern(void) {
    const project_settings_t *s = current_project_get_settings();
    return s != NULL && s->type == PROJECT_TYPE_MODERN;
}
static int project_allows(int index) {
    const project_settings_t *s = current_project_get_settings();
    if (!s) return 0;
    if (index == 5) return s->type == PROJECT_TYPE_PS2;
    if (index == 6) return s->type == PROJECT_TYPE_PS1;
    return s->type == PROJECT_TYPE_MODERN && (s->platforms & g_platforms[index].project_flag) != 0;
}
static int platform_installed(int index) { return package_platform_state(g_platforms[index].package_id) == PKG_STATE_INSTALLED; }
static int arch_count(void) { return g_platforms[g_selected].modern ? 4 : 0; }
static const char *arch_name(int i) { return g_selected == 0 ? g_android_arches[i] : g_desktop_arches[i]; }
static void select_default_arches(void) {
    g_arch_mask = 0;
    if (g_platforms[g_selected].modern) { for (int i = 0; i < 4; i++) if (package_arch_state(g_platforms[g_selected].package_id, i) == PKG_STATE_INSTALLED) g_arch_mask |= 1u << i; }
}
static void select_platform(int index) { g_selected = index; select_default_arches(); snprintf(g_status_text, sizeof(g_status_text), "Ready"); g_error_text[0] = '\0'; }
static int detail_x(void) { return project_modern() ? 345 : PAD + 14; }
static int mode_y(void) { return project_modern() ? 440 : 292; }
static int footer_y(void) { return PAGE_H - PAD - btn_h(); }
static void close_export(void) {
    if (current_status() == 1) { if (g_platforms[g_selected].modern) modern_export_cancel(); else if (g_selected == 5) ps2_export_cancel(); else ps1_export_cancel(); }
    g_open = 0; g_details = 0; g_name.focused = g_path.focused = 0; window_stop_text_input();
}
static void on_browse_result(const char *path, void *user_data) {
    (void)user_data; if (!path) return; snprintf(g_path.text, sizeof(g_path.text), "%s", path); g_path.cursor_pos = (int)strlen(g_path.text); g_path.scroll_offset = 0;
}
static void ensure_ready(void) {
    if (g_ready) return;
    g_ready = 1; text_field_init(&g_name); text_field_init(&g_path);
    snprintf(g_name.text, sizeof(g_name.text), "MyGame"); g_name.cursor_pos = 6;
    g_title = ui_make_text_texture("Export Project", FONT_WEIGHT_BOLD, 20, &g_title_w, &g_title_h);
    for (int i = 0; i < B_COUNT; i++) btn_make(&g_btn[i], g_btn_labels[i], g_btn_blue[i]);
    package_manager_init(); package_refresh_states();
}
void export_dialog_open(void) {
    ui_dialog_drag_reset(&g_drag);
    android_storage_request_if_needed(); ensure_ready();
    const project_settings_t *s = current_project_get_settings();
    if (!s) return;
    g_release = 0; g_details = 0; g_started = 0; g_error_text[0] = '\0'; clear_logs();
    int first = s->type == PROJECT_TYPE_PS2 ? 5 : (s->type == PROJECT_TYPE_PS1 ? 6 : 0);
    if (first == 0) { for (int i = 0; i < MODERN_COUNT; i++) if (project_allows(i)) { first = i; break; } }
    select_platform(first);
    g_open = 1;
    ui_windows_show(&g_win);
}
int export_dialog_is_open(void) { return g_open; }
static void start_export(void) {
    if (!g_name.text[0] || !g_path.text[0] || !project_allows(g_selected)) return;
    int status = current_status(); if (status == 1) return;
    scene_tabs_save_all(); clear_logs(); g_details = 0; g_error_text[0] = '\0';
    if (g_platforms[g_selected].modern) {
        const char *architectures[4]; int count = 0;
        for (int i = 0; i < arch_count(); i++) if (g_arch_mask & (1u << i)) architectures[count++] = arch_name(i);
        if (count == 0) { snprintf(g_error_text, sizeof(g_error_text), "Select at least one installed architecture."); return; }
        g_started = modern_export_start_multi(g_name.text, g_release, g_path.text, g_platforms[g_selected].id, architectures, count);
        snprintf(g_status_text, sizeof(g_status_text), "Preparing %s (%d architectures)", g_platforms[g_selected].label, count);
    } else if (g_selected == 5) {
        g_started = ps2_export_start(g_name.text, g_release, g_path.text); snprintf(g_status_text, sizeof(g_status_text), "Preparing PlayStation 2 export");
    } else {
        g_started = ps1_export_start(g_name.text, g_release, g_path.text); snprintf(g_status_text, sizeof(g_status_text), "Preparing PlayStation 1 export");
    }
}
void export_dialog_update(int window_w, int window_h) {
    if (!g_open) return;
    int dx, dy; { ui_dialog_place(&g_drag, 1, window_w, window_h, PAGE_DEF_W, PAGE_DEF_H, PAGE_DEF_W, PAGE_DEF_H, 44, 1, &dx, &dy, &g_page_w, &g_page_h); }
    int status = current_status();
    if (status) { if (g_platforms[g_selected].modern) modern_export_update(); else if (g_selected == 5) ps2_export_update(); else ps1_export_update(); pump_logs(); status = current_status(); }
    if (status == 1 && g_platforms[g_selected].modern) snprintf(g_status_text, sizeof(g_status_text), "Building %s (%d/%d) %s...", modern_export_current_architecture(), modern_export_architecture_index() + 1, modern_export_architecture_count(), g_platforms[g_selected].label);
    else if (status == 1) snprintf(g_status_text, sizeof(g_status_text), "Exporting %s...", g_platforms[g_selected].label);
    else if (status == 2) snprintf(g_status_text, sizeof(g_status_text), "Export completed successfully");
    else if (status == 3) snprintf(g_status_text, sizeof(g_status_text), "Export failed");
    int dxx = dx + detail_x();
    int path_w = 360 - btn_w(B_BROWSE) - 10;
    if (status == 0) {
        text_field_update(&g_name, dxx, dy + 142, 360, FIELD_H, 360);
        text_field_update(&g_path, dxx, dy + 214, path_w, FIELD_H, path_w);
        if (g_name.focused || g_path.focused) window_start_text_input(); else window_stop_text_input();
    }
    if (!window_mouse_left_just_pressed()) return;
    int mx = window_mouse_x(), my = window_mouse_y();
    int fy = dy + footer_y();
    int right = dx + PAGE_W - PAD;
    if (ui_dialog_close_button_hit(mx, my, dx + PAGE_W - PAD - 22, dy + 11, 22)) { close_export(); return; }
    int main_id = status == 0 ? B_CANCEL : (status == 1 ? B_CANCEL : B_CLOSE);
    int main_x = right - btn_w(main_id);
    if (btn_hit(main_id, main_x, fy, mx, my)) { close_export(); return; }
    if (status == 1) return;
    if (status == 0) {
        int export_x = main_x - BTN_GAP - btn_w(B_EXPORT);
        if (btn_hit(B_EXPORT, export_x, fy, mx, my)) { start_export(); return; }
    } else {
        int det_id = g_details ? B_HIDE : B_DETAILS;
        int det_x = main_x - BTN_GAP - btn_w(det_id);
        if (btn_hit(det_id, det_x, fy, mx, my)) { g_details = !g_details; }
        return;
    }
    if (project_modern()) {
        for (int i = 0; i < MODERN_COUNT; i++) if (project_allows(i) && ui_point_in_rect(mx, my, dx + PAD + (i % 2) * (CARD_W + CARD_GAP), dy + 82 + (i / 2) * (CARD_H + CARD_GAP), CARD_W, CARD_H)) { select_platform(i); return; }
    }
    if (btn_hit(B_BROWSE, dxx + path_w + 10, dy + 214, mx, my)) { file_manager_open(FILE_MANAGER_ROOT_DEVICE, FILE_MANAGER_MODE_PICK_FOLDER, NULL, 0, on_browse_result, NULL); return; }
    if (g_platforms[g_selected].modern) for (int i = 0; i < arch_count(); i++) { int ay = dy + 322 + i * 30; if (ui_point_in_rect(mx, my, dxx, ay, 320, 26) && package_arch_state(g_platforms[g_selected].package_id, i) == PKG_STATE_INSTALLED) { g_arch_mask ^= 1u << i; return; } }
    int my0 = dy + mode_y() + 30;
    if (btn_hit(B_DEBUG_ON, dxx, my0, mx, my)) { g_release = 0; return; }
    if (btn_hit(B_RELEASE_ON, dxx + btn_w(B_DEBUG_ON) + BTN_GAP, my0, mx, my)) { g_release = 1; return; }
}
static void draw_card(int x, int y, int index) {
    int enabled = project_allows(index), installed = platform_installed(index), selected = index == g_selected;
    ui_color_t card = selected ? UI_COLOR_BUTTON_BLUE : UI_COLOR_SURFACE_CONTROL;
    unsigned char r = card.r, g = card.g, b = card.b;
    window_fill_rect(x, y, CARD_W, CARD_H, r, g, b); draw_text(g_platforms[index].label, x + 10, y + 12, 14, enabled ? 235 : 125, enabled ? 235 : 125, enabled ? 235 : 125);
    draw_text(!enabled ? "Not for this project" : installed ? "Installed" : "Not installed", x + 10, y + 38, 11, installed && enabled ? 115 : 205, installed && enabled ? 210 : 155, installed && enabled ? 135 : 105);
}
void export_dialog_draw(int window_w, int window_h) {
    if (!g_open) return;
    int dx, dy; ui_dialog_draw_backdrop(window_w, window_h); { ui_dialog_place(&g_drag, 0, window_w, window_h, PAGE_DEF_W, PAGE_DEF_H, PAGE_DEF_W, PAGE_DEF_H, 44, 1, &dx, &dy, &g_page_w, &g_page_h); }
    ui_dialog_draw_frame(dx, dy, PAGE_W, PAGE_H); ui_dialog_draw_title_left(g_title, g_title_w, g_title_h, dx, dy, 44, PAD);
    ui_dialog_draw_close_button(dx + PAGE_W - PAD - 22, dy + 11, 22);
    int modern = project_modern();
    if (modern) {
        draw_text("Platforms", dx + PAD, dy + 56, 13, UI_RGB(UI_COLOR_TEXT_MUTED));
        for (int i = 0; i < MODERN_COUNT; i++) draw_card(dx + PAD + (i % 2) * (CARD_W + CARD_GAP), dy + 82 + (i / 2) * (CARD_H + CARD_GAP), i);
    }
    int dxx = dx + detail_x();
    int fy = dy + footer_y();
    int panel_bottom = fy - 14;
    window_fill_rect(dxx - 14, dy + 52, PAGE_W - detail_x() - PAD + 14, panel_bottom - (dy + 52), UI_RGB(UI_COLOR_SURFACE_PANEL));
    draw_text(g_platforms[g_selected].label, dxx, dy + 70, 18, UI_RGB(UI_COLOR_TEXT_PRIMARY));
    int status = current_status();
    int path_w = 360 - btn_w(B_BROWSE) - 10;
    if (status == 0) {
        draw_text("Output name", dxx, dy + 120, 12, UI_RGB(UI_COLOR_TEXT_MUTED)); text_field_draw(&g_name, dxx, dy + 142, 360, FIELD_H, 16);
        draw_text("Output folder", dxx, dy + 192, 12, UI_RGB(UI_COLOR_TEXT_MUTED)); text_field_draw(&g_path, dxx, dy + 214, path_w, FIELD_H, 16);
        btn_draw(B_BROWSE, dxx + path_w + 10, dy + 214);
        if (g_platforms[g_selected].modern) {
            draw_text("Architectures (installed only)", dxx, dy + 286, 12, UI_RGB(UI_COLOR_TEXT_MUTED));
            for (int i = 0; i < arch_count(); i++) {
                int ay = dy + 322 + i * 30, installed = package_arch_state(g_platforms[g_selected].package_id, i) == PKG_STATE_INSTALLED;
                window_fill_rect(dxx, ay, 320, 26, installed ? 56 : 43, installed ? 62 : 45, installed ? 70 : 48);
                draw_text((g_arch_mask & (1u << i)) ? "[x]" : "[ ]", dxx + 8, ay + 6, 11, installed ? 230 : 120, installed ? 230 : 120, installed ? 230 : 120);
                draw_text(arch_name(i), dxx + 42, ay + 6, 11, installed ? 220 : 120, installed ? 220 : 120, installed ? 220 : 120);
            }
        }
        int my0 = dy + mode_y();
        draw_text("Build mode", dxx, my0, 12, UI_RGB(UI_COLOR_TEXT_MUTED));
        btn_draw(g_release ? B_DEBUG_OFF : B_DEBUG_ON, dxx, my0 + 30);
        btn_draw(g_release ? B_RELEASE_ON : B_RELEASE_OFF, dxx + btn_w(B_DEBUG_ON) + BTN_GAP, my0 + 30);
        if (g_error_text[0]) draw_text(g_error_text, dxx, my0 + 82, 11, UI_RGB(UI_COLOR_DANGER));
    } else {
        int total = 12, done = status == 2 ? total : (g_log_count > 0 ? g_log_count % total : 1); if (done > total) done = total;
        draw_text(g_status_text, dxx, dy + 124, 14, status == 3 ? 240 : 220, status == 3 ? 120 : 220, status == 3 ? 110 : 220);
        window_fill_rect(dxx, dy + 164, 480, 12, UI_RGB(UI_COLOR_SURFACE_BORDER)); window_fill_rect(dxx, dy + 164, 480 * done / total, 12, UI_RGB(UI_COLOR_INFO));
        char progress[64]; snprintf(progress, sizeof(progress), "%d/%d", done, total); draw_text(progress, dxx + 490, dy + 162, 11, UI_RGB(UI_COLOR_TEXT_MUTED));
        if (status == 3 && g_log_count > 0) draw_text(g_logs[g_log_count - 1], dxx, dy + 204, 11, UI_RGB(UI_COLOR_DANGER_SOFT));
        if (g_details) { for (int i = 0; i < g_log_count && i < LOG_SHOWN; i++) draw_text(g_logs[i], dxx, dy + 240 + i * 17, 10, UI_RGB(UI_COLOR_TEXT_MUTED)); }
    }
    int right = dx + PAGE_W - PAD;
    int main_id = status == 0 || status == 1 ? B_CANCEL : B_CLOSE;
    int main_x = right - btn_w(main_id);
    btn_draw(main_id, main_x, fy);
    if (status == 0) btn_draw(B_EXPORT, main_x - BTN_GAP - btn_w(B_EXPORT), fy);
    else if (status >= 2) { int det_id = g_details ? B_HIDE : B_DETAILS; btn_draw(det_id, main_x - BTN_GAP - btn_w(det_id), fy); }
}
void export_dialog_shutdown(void) {
    if (g_title) window_destroy_texture(g_title);
    g_title = NULL; text_field_free(&g_name); text_field_free(&g_path);
    for (int i = 0; i < B_COUNT; i++) btn_free(&g_btn[i]);
    g_open = 0; g_ready = 0; g_started = 0; g_details = 0; g_log_count = 0; package_manager_shutdown();
}
