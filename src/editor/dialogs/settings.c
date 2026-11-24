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
#include "editor/dialogs/settings.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "platform/common/window.h"
#include "editor/ui/base.h"
#include "editor/ui/dialog.h"
#include "editor/ui/windows.h"
#include "editor/ui/font.h"
#include "editor/ui/labeled_button.h"
#include "editor/ui/text_field.h"
#include "editor/ui/theme.h"
#include "project/current_project.h"
#include "project/project_file.h"
#include "project/project_types.h"

#define PAGE_DEF_W 760
#define PAGE_DEF_H 520
#define PAGE_W g_pw
#define PAGE_H g_ph
static int g_pw = PAGE_DEF_W, g_ph = PAGE_DEF_H;
static ui_dialog_drag_t g_drag;
static ui_window_t g_win = UI_WINDOW_INIT(project_settings_dialog_is_open, project_settings_dialog_update, project_settings_dialog_draw);
#define PAD 28
#define SIDEBAR_W 180
#define FIELD_H 32
#define PRESET_H 30
#define PRESET_GAP 8

static int g_open, g_ready, g_section, g_section_count;
typedef struct { labeled_button_t b; window_texture_t *bg; window_texture_t *tx; } btn_t;
static btn_t g_save_btn, g_cancel_btn;
static void btn_make(btn_t *btn, const char *label, int blue) {
    const ui_color_t *c = blue ? &UI_COLOR_BUTTON_BLUE : &UI_COLOR_GRAY_MUTED;
    btn->b = labeled_button_create(label, FONT_WEIGHT_REGULAR, 15, UI_BUTTON_PADDING_X, UI_BUTTON_PADDING_Y, UI_BUTTON_RADIUS, c->r, c->g, c->b);
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
static void btn_draw(const btn_t *btn, int x, int y) {
    if (!btn->bg) return;
    window_draw_texture(btn->bg, x, y, btn->b.width, btn->b.height);
    if (btn->tx) window_draw_texture(btn->tx, x + btn->b.text_offset_x, y + btn->b.text_offset_y, btn->b.text_img.width, btn->b.text_img.height);
}
static int project_is_modern(void) {
    const project_settings_t *s = current_project_get_settings();
    return s != NULL && s->type == PROJECT_TYPE_MODERN;
}
#define BTN_GAP 10
static text_field_t g_name, g_width, g_height;
static window_texture_t *g_title, *g_general, *g_display, *g_video, *g_input, *g_audio, *g_platforms;
static int g_tw, g_th, g_gw, g_gh, g_dw, g_dh, g_vw, g_vh, g_iw, g_ih, g_aw, g_ah, g_pw, g_ph;
static const char *g_sections[5];
static int g_section_ready[5];
static const int g_preset_w[] = {320, 640, 800, 1280, 1920};
static const int g_preset_h[] = {240, 448, 600, 720, 1080};

static window_texture_t *make(const char *s, int size, int *w, int *h) {
    return ui_make_text_texture(s, FONT_WEIGHT_REGULAR, size, w, h);
}
static void set_field(text_field_t *f, int value) {
    snprintf(f->text, sizeof(f->text), "%d", value);
    f->cursor_pos = (int)strlen(f->text); f->scroll_offset = 0;
}
static int field_value(const text_field_t *f, int fallback) {
    int v = atoi(f->text); return v > 0 && v <= 16384 ? v : fallback;
}
static void refresh_sections(void) {
    const project_settings_t *s = current_project_get_settings();
    g_section_count = 0;
    if (s == NULL || s->type == PROJECT_TYPE_MODERN) {
        g_sections[0] = "General"; g_sections[1] = "Display"; g_sections[2] = "Input"; g_sections[3] = "Audio"; g_sections[4] = "Platforms";
        g_section_count = 5; g_section_ready[0] = g_section_ready[1] = 1; g_section_ready[2] = g_section_ready[3] = g_section_ready[4] = 0;
    } else {
        g_sections[0] = "General"; g_sections[1] = "Video"; g_sections[2] = "Audio";
        g_section_count = 3; g_section_ready[0] = g_section_ready[1] = 1; g_section_ready[2] = 0;
    }
}
static void refresh_fields(void) {
    const project_settings_t *s = current_project_get_settings();
    if (s == NULL) return;
    snprintf(g_name.text, sizeof(g_name.text), "%s", s->name);
    g_name.cursor_pos = (int)strlen(g_name.text);
    set_field(&g_width, s->display_width); set_field(&g_height, s->display_height);
}
static void ensure_ready(void) {
    if (g_ready) return;
    g_ready = 1;
    text_field_init(&g_name); text_field_init(&g_width); text_field_init(&g_height);
    g_title = make("Project Settings", 20, &g_tw, &g_th);
    g_general = make("General", 15, &g_gw, &g_gh); g_display = make("Display", 15, &g_dw, &g_dh); g_video = make("Video", 15, &g_vw, &g_vh);
    g_input = make("Input", 15, &g_iw, &g_ih); g_audio = make("Audio", 15, &g_aw, &g_ah);
    g_platforms = make("Platforms", 15, &g_pw, &g_ph);
    btn_make(&g_save_btn, "Save", 1); btn_make(&g_cancel_btn, "Cancel", 0);
}
void project_settings_dialog_open(void) { ui_dialog_drag_reset(&g_drag); ensure_ready(); refresh_sections(); refresh_fields(); g_section = 0; g_open = current_project_is_open(); if (g_open) ui_windows_show(&g_win); }
int project_settings_dialog_is_open(void) { return g_open; }
static void close_dialog(void) {
    g_open = 0; g_name.focused = g_width.focused = g_height.focused = 0; window_stop_text_input();
}
static void save_settings(void) {
    const project_settings_t *old = current_project_get_settings();
    const char *root = current_project_get_path();
    if (old == NULL || root == NULL) return;
    project_settings_t s = *old;
    snprintf(s.name, sizeof(s.name), "%s", g_name.text[0] ? g_name.text : old->name);
    if (project_is_modern()) {
        s.display_width = field_value(&g_width, old->display_width);
        s.display_height = field_value(&g_height, old->display_height);
    }
    if (project_file_save(root, &s)) current_project_set(root, &s);
    close_dialog();
}
void project_settings_dialog_update(int window_w, int window_h) {
    (void)window_w; (void)window_h;
    if (!g_open) return;
    int dx, dy; { ui_dialog_place(&g_drag, 1, window_w, window_h, PAGE_DEF_W, PAGE_DEF_H, PAGE_DEF_W, PAGE_DEF_H, 44, 1, &dx, &dy, &g_pw, &g_ph); }
    int content_x = dx + SIDEBAR_W + PAD + 16, content_w = PAGE_W - SIDEBAR_W - PAD * 2 - 16;
    if (window_mouse_left_just_pressed()) {
        int mx = window_mouse_x(), my = window_mouse_y();
        if (ui_dialog_close_button_hit(mx, my, dx + PAGE_W - PAD - 22, dy + 11, 22)) { close_dialog(); return; }
        for (int i = 0; i < g_section_count; i++) if (ui_point_in_rect(mx, my, dx + 12, dy + 76 + i * 48, SIDEBAR_W - 24, 38)) { if (g_section_ready[i]) g_section = i; }
        if (g_section == 1 && project_is_modern()) {
            for (int i = 0; i < 5; i++) {
                int x = content_x + (i % 3) * 150, y = dy + 208 + (i / 3) * 38;
                if (ui_point_in_rect(mx, my, x, y, 140, PRESET_H)) { set_field(&g_width, g_preset_w[i]); set_field(&g_height, g_preset_h[i]); }
            }
        }
        int by = dy + PAGE_H - PAD - g_save_btn.b.height;
        int cancel_x = dx + PAGE_W - PAD - g_cancel_btn.b.width;
        int save_x = cancel_x - BTN_GAP - g_save_btn.b.width;
        if (ui_point_in_rect(mx, my, save_x, by, g_save_btn.b.width, g_save_btn.b.height)) { save_settings(); return; }
        if (ui_point_in_rect(mx, my, cancel_x, by, g_cancel_btn.b.width, g_cancel_btn.b.height)) { close_dialog(); return; }
    }
    if (g_section == 0) text_field_update(&g_name, content_x, dy + 142, content_w, FIELD_H, content_w);
    if (g_section == 1 && project_is_modern()) {
        text_field_update(&g_width, content_x, dy + 142, 150, FIELD_H, 150);
        text_field_update(&g_height, content_x + 178, dy + 142, 150, FIELD_H, 150);
    }
    if (g_name.focused || g_width.focused || g_height.focused) window_start_text_input(); else window_stop_text_input();
}
static void draw_text(window_texture_t *t, int x, int y, int w, int h, unsigned char c) { if (t) window_draw_texture_tinted(t, x, y, w, h, c, c, c); }
static void draw_label(const char *s, int x, int y) { int w, h; window_texture_t *t = make(s, 13, &w, &h); draw_text(t, x, y, w, h, 190); if (t) window_destroy_texture(t); }
void project_settings_dialog_draw(int window_w, int window_h) {
    if (!g_open) return;
    int dx, dy; ui_dialog_draw_backdrop(window_w, window_h); { ui_dialog_place(&g_drag, 0, window_w, window_h, PAGE_DEF_W, PAGE_DEF_H, PAGE_DEF_W, PAGE_DEF_H, 44, 1, &dx, &dy, &g_pw, &g_ph); }
    ui_dialog_draw_frame(dx, dy, PAGE_W, PAGE_H); ui_dialog_draw_title_left(g_title, g_tw, g_th, dx + PAD, dy, 44, PAD);
    window_fill_rect(dx, dy + 44, SIDEBAR_W, PAGE_H - 44, UI_COLOR_BLACK_MUTED.r, UI_COLOR_BLACK_MUTED.g, UI_COLOR_BLACK_MUTED.b);
    for (int i = 0; i < g_section_count; i++) {
        int y = dy + 76 + i * 48, active = i == g_section;
        window_fill_rect(dx + 12, y, SIDEBAR_W - 24, 38, active ? UI_COLOR_BUTTON_BLUE.r : UI_COLOR_BLACK_MUTED.r, active ? UI_COLOR_BUTTON_BLUE.g : UI_COLOR_BLACK_MUTED.g, active ? UI_COLOR_BUTTON_BLUE.b : UI_COLOR_BLACK_MUTED.b);
        int w = 0, h = 0; window_texture_t *t = i == 0 ? g_general : i == 1 ? (project_is_modern() ? g_display : g_video) : i == 2 ? (project_is_modern() ? g_input : g_audio) : i == 3 ? g_audio : g_platforms;
        if (i == 0) { w = g_gw; h = g_gh; } else if (i == 1) { w = project_is_modern() ? g_dw : g_vw; h = project_is_modern() ? g_dh : g_vh; } else if (i == 2) { w = project_is_modern() ? g_iw : g_aw; h = project_is_modern() ? g_ih : g_ah; } else if (i == 3) { w = g_aw; h = g_ah; } else { w = g_pw; h = g_ph; }
        draw_text(t, dx + 24, y + (38 - h) / 2, w, h, g_section_ready[i] ? 235 : 120);
        if (!g_section_ready[i]) draw_label("Soon", dx + SIDEBAR_W - 52, y + 12);
    }
    int x = dx + SIDEBAR_W + PAD + 16, w = PAGE_W - SIDEBAR_W - PAD * 2 - 16;
    if (g_section == 0) {
        draw_label("Game name", x, dy + 112); text_field_draw(&g_name, x, dy + 142, w, FIELD_H, 16);
    } else if (g_section == 1 && !project_is_modern()) {
        const project_settings_t *ps = current_project_get_settings();
        draw_label(ps != NULL && ps->type == PROJECT_TYPE_PS2 ? "PlayStation 2 projects use the console video mode." : "PlayStation 1 projects use the console video modes.", x, dy + 112);
        draw_label("Resolution is not configured for this project type.", x, dy + 144);
    } else if (g_section == 1) {
        draw_label("Width", x, dy + 112); draw_label("Height", x + 178, dy + 112);
        text_field_draw(&g_width, x, dy + 142, 150, FIELD_H, 16); text_field_draw(&g_height, x + 178, dy + 142, 150, FIELD_H, 16);
        draw_label("Presets", x, dy + 190);
        for (int i = 0; i < 5; i++) { int px = x + (i % 3) * 150, py = dy + 208 + (i / 3) * 38; window_fill_rect(px, py, 140, PRESET_H, UI_RGB(UI_COLOR_SURFACE_BORDER)); char label[32]; snprintf(label, sizeof(label), "%d x %d", g_preset_w[i], g_preset_h[i]); int tw, th; window_texture_t *t = make(label, 12, &tw, &th); draw_text(t, px + 8, py + 8, tw, th, 220); if (t) window_destroy_texture(t); }
        int cw = field_value(&g_width, 640), ch = field_value(&g_height, 448); draw_label(cw >= ch ? "Orientation: Landscape" : "Orientation: Portrait", x, dy + 306);
        draw_label("Custom values are saved to project.rebax.", x, dy + 344);
    } else { draw_label("This section is planned for a future release.", x, dy + 120); }
    int by = dy + PAGE_H - PAD - g_save_btn.b.height;
    int cancel_x = dx + PAGE_W - PAD - g_cancel_btn.b.width;
    btn_draw(&g_cancel_btn, cancel_x, by);
    btn_draw(&g_save_btn, cancel_x - BTN_GAP - g_save_btn.b.width, by);
}
void project_settings_dialog_shutdown(void) { btn_free(&g_save_btn); btn_free(&g_cancel_btn); text_field_free(&g_name); text_field_free(&g_width); text_field_free(&g_height); if (g_title) window_destroy_texture(g_title); if (g_general) window_destroy_texture(g_general); if (g_display) window_destroy_texture(g_display); if (g_video) window_destroy_texture(g_video); if (g_input) window_destroy_texture(g_input); if (g_audio) window_destroy_texture(g_audio); if (g_platforms) window_destroy_texture(g_platforms); memset(&g_name, 0, sizeof(g_name)); g_title = g_general = g_display = g_video = g_input = g_audio = g_platforms = NULL; g_open = g_ready = 0; }
