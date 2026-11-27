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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "core/i18n.h"
#include "editor/dialogs/image_viewer.h"
#include "editor/ui/base.h"
#include "editor/ui/dialog.h"
#include "editor/ui/theme.h"
#include "editor/ui/windows.h"
#include "filesystem/rebax_fs.h"
#include "formats/image_decoder.h"
#include "platform/common/window.h"

#define DEFAULT_W 680
#define DEFAULT_H 520
#define MIN_W 360
#define MIN_H 300
#define TITLEBAR_HEIGHT 40
#define CLOSE_BTN_SIZE 22
#define INFO_HEIGHT 74
#define CHECKER 16

static int g_open = 0, g_dw = DEFAULT_W, g_dh = DEFAULT_H;
static ui_dialog_drag_t g_drag;
static ui_window_t g_win = UI_WINDOW_INIT(image_viewer_is_open, image_viewer_update, image_viewer_draw);
static window_texture_t *g_image, *g_title_tex, *g_info_tex[4];
static int g_title_w, g_title_h, g_info_w[4], g_info_h[4];
static int g_img_w, g_img_h;
static float g_zoom = 1.0f, g_pan_x = 0.0f, g_pan_y = 0.0f;
static int g_fit_pending = 1;
static int g_dragging = 0, g_last_mx, g_last_my;
static float g_pinch_start = 0.0f, g_pinch_zoom = 1.0f;

static void free_visuals(void) {
    if (g_image) window_destroy_texture(g_image);
    if (g_title_tex) window_destroy_texture(g_title_tex);
    for (int i = 0; i < 4; i++) { if (g_info_tex[i]) window_destroy_texture(g_info_tex[i]); g_info_tex[i] = NULL; }
    g_image = g_title_tex = NULL;
}

static void format_size(long long bytes, char *out, size_t n) {
    if (bytes >= 1024 * 1024) snprintf(out, n, "%.2f MB", (double)bytes / (1024.0 * 1024.0));
    else if (bytes >= 1024) snprintf(out, n, "%.1f KB", (double)bytes / 1024.0);
    else snprintf(out, n, "%lld B", bytes);
}

static void set_info(int index, const char *text) {
    g_info_tex[index] = ui_make_text_texture(text, FONT_WEIGHT_REGULAR, 12, &g_info_w[index], &g_info_h[index]);
}

int image_viewer_open(const char *path) {
    int w = 0, h = 0;
    unsigned char *pixels;
    struct stat st;
    char name[160], line[256], size[48];
    int has_alpha = 0;
    if (path == NULL) return 0;
    pixels = image_loader_editor_load(path, &w, &h, 0, 0, 0);
    if (pixels == NULL || w <= 0 || h <= 0) { free(pixels); return 0; }
    free_visuals();
    g_image = window_create_texture(pixels, w, h);
    for (size_t i = 3; i < (size_t)w * (size_t)h * 4; i += 4) if (pixels[i] != 255) { has_alpha = 1; break; }
    free(pixels);
    if (g_image == NULL) return 0;
    g_img_w = w;
    g_img_h = h;
    rebax_fs_basename(path, name, sizeof(name));
    g_title_tex = ui_make_text_texture(name, FONT_WEIGHT_BOLD, 16, &g_title_w, &g_title_h);
    snprintf(line, sizeof(line), tr("Dimensions: %d x %d pixels"), w, h);
    set_info(0, line);
    snprintf(line, sizeof(line), "%s", has_alpha ? tr("Colour: RGBA, 8 bits per channel (32 bits per pixel)") : tr("Colour: RGB, 8 bits per channel (24 bits per pixel)"));
    set_info(1, line);
    format_size(stat(path, &st) == 0 ? (long long)st.st_size : 0, size, sizeof(size));
    const char *ext = rebax_fs_extension(path);
    snprintf(line, sizeof(line), tr("File: %s, %s"), ext && ext[0] ? ext : "?", size);
    set_info(2, line);
    snprintf(line, sizeof(line), tr("Memory when loaded: %.2f MB"), (double)w * h * 4.0 / (1024.0 * 1024.0));
    set_info(3, line);
    g_fit_pending = 1;
    g_pan_x = g_pan_y = 0.0f;
    g_dragging = 0;
    ui_dialog_drag_reset(&g_drag);
    g_open = 1;
    ui_windows_show(&g_win);
    return 1;
}

int image_viewer_is_open(void) { return g_open; }

static void fit(int area_w, int area_h) {
    float zx = (float)(area_w - 24) / (float)g_img_w, zy = (float)(area_h - 24) / (float)g_img_h;
    g_zoom = zx < zy ? zx : zy;
    if (g_zoom > 1.0f && g_img_w * g_zoom > 4 * g_img_w) g_zoom = 4.0f;
    if (g_zoom < 0.02f) g_zoom = 0.02f;
    g_pan_x = g_pan_y = 0.0f;
}

static void area_rect(int dx, int dy, int *x, int *y, int *w, int *h) {
    *x = dx + 12;
    *y = dy + TITLEBAR_HEIGHT + 10;
    *w = g_dw - 24;
    *h = g_dh - TITLEBAR_HEIGHT - INFO_HEIGHT - 20;
}

void image_viewer_update(int window_w, int window_h) {
    if (!g_open) return;
    int dx, dy, ax, ay, aw, ah;
    ui_dialog_place(&g_drag, 1, window_w, window_h, DEFAULT_W, DEFAULT_H, MIN_W, MIN_H, TITLEBAR_HEIGHT, 1, &dx, &dy, &g_dw, &g_dh);
    area_rect(dx, dy, &ax, &ay, &aw, &ah);
    if (g_fit_pending) { fit(aw, ah); g_fit_pending = 0; }
    int mx = window_mouse_x(), my = window_mouse_y();
    if (window_key_just_pressed_escape()) { g_open = 0; return; }
    if (window_mouse_left_just_pressed() &&
        ui_dialog_close_button_hit(mx, my, dx + g_dw - 16 - CLOSE_BTN_SIZE, dy + (TITLEBAR_HEIGHT - CLOSE_BTN_SIZE) / 2, CLOSE_BTN_SIZE)) { g_open = 0; return; }
    int inside = ui_point_in_rect(mx, my, ax, ay, aw, ah);
    if (window_touch_count() == 2) {
        int x1, y1, x2, y2;
        window_touch_point(0, &x1, &y1);
        window_touch_point(1, &x2, &y2);
        float d = sqrtf((float)((x1 - x2) * (x1 - x2) + (y1 - y2) * (y1 - y2)));
        if (g_pinch_start <= 0.0f) { g_pinch_start = d; g_pinch_zoom = g_zoom; }
        else if (g_pinch_start > 1.0f) g_zoom = g_pinch_zoom * d / g_pinch_start;
        g_dragging = 0;
    } else {
        g_pinch_start = 0.0f;
        if (window_mouse_left_just_pressed() && inside) { g_dragging = 1; g_last_mx = mx; g_last_my = my; }
        if (g_dragging && window_mouse_left_down()) {
            g_pan_x += (float)(mx - g_last_mx);
            g_pan_y += (float)(my - g_last_my);
            g_last_mx = mx;
            g_last_my = my;
        }
        if (!window_mouse_left_down()) g_dragging = 0;
    }
    int wheel = window_mouse_wheel_delta();
    if (wheel && inside) g_zoom *= wheel > 0 ? 1.15f : 0.87f;
    if (g_zoom < 0.02f) g_zoom = 0.02f;
    if (g_zoom > 64.0f) g_zoom = 64.0f;
    if (window_key_just_pressed_f()) fit(aw, ah);
}

void image_viewer_draw(int window_w, int window_h) {
    if (!g_open) return;
    ui_dialog_draw_backdrop(window_w, window_h);
    int dx, dy, ax, ay, aw, ah;
    ui_dialog_place(&g_drag, 0, window_w, window_h, DEFAULT_W, DEFAULT_H, MIN_W, MIN_H, TITLEBAR_HEIGHT, 1, &dx, &dy, &g_dw, &g_dh);
    area_rect(dx, dy, &ax, &ay, &aw, &ah);
    ui_dialog_draw_frame(dx, dy, g_dw, g_dh);
    ui_dialog_draw_title_left(g_title_tex, g_title_w, g_title_h, dx, dy, TITLEBAR_HEIGHT, 16);
    ui_dialog_draw_close_button(dx + g_dw - 16 - CLOSE_BTN_SIZE, dy + (TITLEBAR_HEIGHT - CLOSE_BTN_SIZE) / 2, CLOSE_BTN_SIZE);
    window_fill_rect(dx + 1, dy + TITLEBAR_HEIGHT, g_dw - 2, 1, UI_RGB(UI_COLOR_SURFACE_BORDER));
    window_fill_rect(ax - 1, ay - 1, aw + 2, ah + 2, UI_RGB(UI_COLOR_SURFACE_BORDER));
    window_fill_rect(ax, ay, aw, ah, 0x2A, 0x2F, 0x38);
    window_set_clip_rect(ax, ay, aw, ah);
    for (int cy = 0; cy * CHECKER < ah; cy++)
        for (int cx = (cy & 1); cx * CHECKER < aw; cx += 2)
            window_fill_rect(ax + cx * CHECKER, ay + cy * CHECKER, CHECKER, CHECKER, 0x34, 0x3A, 0x45);
    if (g_image) {
        int iw = (int)(g_img_w * g_zoom), ih = (int)(g_img_h * g_zoom);
        if (iw < 1) iw = 1;
        if (ih < 1) ih = 1;
        window_draw_texture(g_image, ax + (aw - iw) / 2 + (int)g_pan_x, ay + (ah - ih) / 2 + (int)g_pan_y, iw, ih);
    }
    window_clear_clip_rect();
    int iy = ay + ah + 10;
    for (int i = 0; i < 4; i++) {
        if (!g_info_tex[i]) continue;
        int col = i % 2, row = i / 2;
        window_draw_texture_tinted(g_info_tex[i], ax + col * (aw / 2 + 4), iy + row * 22, g_info_w[i], g_info_h[i], UI_RGB(UI_COLOR_TEXT_MUTED));
    }
    char zoom[48];
    snprintf(zoom, sizeof(zoom), "%d%%", (int)(g_zoom * 100.0f + 0.5f));
    int zw = 0, zh = 0;
    window_texture_t *zt = ui_make_text_texture(zoom, FONT_WEIGHT_REGULAR, 12, &zw, &zh);
    if (zt) { window_draw_texture_tinted(zt, ax + aw - zw - 8, ay + 8, zw, zh, UI_RGB(UI_COLOR_TEXT_FAINT)); window_destroy_texture(zt); }
}

void image_viewer_shutdown(void) {
    free_visuals();
    g_open = 0;
}
