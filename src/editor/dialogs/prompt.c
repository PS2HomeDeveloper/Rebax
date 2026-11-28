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
#include "editor/dialogs/prompt.h"
#include "editor/ui/base.h"
#include "editor/ui/dialog.h"
#include "editor/ui/labeled_button.h"
#include "editor/ui/text_field.h"
#include "editor/ui/scrollbar.h"
#include "editor/ui/theme.h"
#include "editor/ui/windows.h"
#include "platform/common/window.h"

#define DEFAULT_W 380
#define TITLEBAR_HEIGHT 40
#define CLOSE_BTN_SIZE 22
#define BUTTON_GAP 10
#define FIELD_H 32

static int g_open = 0, g_input_mode = 0, g_dw = DEFAULT_W, g_dh = 170;
static ui_dialog_drag_t g_drag;
static ui_window_t g_win = UI_WINDOW_INIT(prompt_is_open, prompt_update, prompt_draw);
static text_field_t g_field;
static int g_field_ready = 0;
static char g_message[240];
static prompt_callback_t g_callback;
static void *g_user;
static window_texture_t *g_title_tex, *g_message_tex, *g_ok_bg, *g_ok_txt, *g_cancel_bg, *g_cancel_txt;
static int g_title_w, g_title_h, g_message_w, g_message_h;
static labeled_button_t g_ok, g_cancel;
static int g_buttons_ready = 0;

static void free_visuals(void) {
    if (g_title_tex) window_destroy_texture(g_title_tex);
    if (g_message_tex) window_destroy_texture(g_message_tex);
    if (g_ok_bg) window_destroy_texture(g_ok_bg);
    if (g_ok_txt) window_destroy_texture(g_ok_txt);
    if (g_cancel_bg) window_destroy_texture(g_cancel_bg);
    if (g_cancel_txt) window_destroy_texture(g_cancel_txt);
    g_title_tex = g_message_tex = g_ok_bg = g_ok_txt = g_cancel_bg = g_cancel_txt = NULL;
    if (g_buttons_ready) { labeled_button_free(&g_ok); labeled_button_free(&g_cancel); }
    g_buttons_ready = 0;
}

static void make_visuals(const char *title, const char *ok_label, int danger) {
    free_visuals();
    g_title_tex = ui_make_text_texture(title, FONT_WEIGHT_BOLD, 16, &g_title_w, &g_title_h);
    if (!g_input_mode) g_message_tex = ui_make_text_texture(g_message, FONT_WEIGHT_REGULAR, 13, &g_message_w, &g_message_h);
    ui_color_t main = danger ? UI_COLOR_DANGER_BG : UI_COLOR_BUTTON_BLUE;
    g_ok = labeled_button_create(ok_label, FONT_WEIGHT_REGULAR, 13, 18, 8, 4, UI_RGB(main));
    g_cancel = labeled_button_create(tr("Cancel"), FONT_WEIGHT_REGULAR, 13, 18, 8, 4, UI_RGB(UI_COLOR_GRAY_MUTED));
    g_ok_bg = window_create_texture(g_ok.shape_img.pixels, g_ok.shape_img.px_w, g_ok.shape_img.px_h);
    g_ok_txt = window_create_texture(g_ok.text_img.pixels, g_ok.text_img.px_w, g_ok.text_img.px_h);
    g_cancel_bg = window_create_texture(g_cancel.shape_img.pixels, g_cancel.shape_img.px_w, g_cancel.shape_img.px_h);
    g_cancel_txt = window_create_texture(g_cancel.text_img.pixels, g_cancel.text_img.px_w, g_cancel.text_img.px_h);
    g_buttons_ready = 1;
}

static void begin(const char *title, const char *ok_label, int danger, prompt_callback_t callback, void *user_data) {
    g_callback = callback;
    g_user = user_data;
    g_dh = g_input_mode ? 170 : 150;
    make_visuals(tr(title), tr(ok_label), danger);
    ui_dialog_drag_reset(&g_drag);
    ui_scrollbar_cancel_touch();
    g_open = 1;
    ui_windows_show(&g_win);
}

void prompt_input_open(const char *title, const char *initial, const char *ok_label,
                       prompt_callback_t callback, void *user_data) {
    if (!g_field_ready) { text_field_init(&g_field); g_field_ready = 1; }
    g_input_mode = 1;
    snprintf(g_field.text, sizeof(g_field.text), "%s", initial ? initial : "");
    g_field.cursor_pos = (int)strlen(g_field.text);
    g_field.scroll_offset = 0;
    g_field.focused = 1;
    begin(title, ok_label, 0, callback, user_data);
}

void prompt_confirm_open(const char *title, const char *message, const char *ok_label, int danger,
                         prompt_callback_t callback, void *user_data) {
    g_input_mode = 0;
    snprintf(g_message, sizeof(g_message), "%s", tr(message));
    begin(title, ok_label, danger, callback, user_data);
}

int prompt_is_open(void) { return g_open; }

static void finish(int accepted) {
    prompt_callback_t callback = g_callback;
    void *user = g_user;
    char text[TEXT_FIELD_MAX_LEN];
    snprintf(text, sizeof(text), "%s", g_field.text);
    g_open = 0;
    g_field.focused = 0;
    window_stop_text_input();
    if (accepted && callback) callback(g_input_mode ? text : "", user);
}

static void layout(int dx, int dy, int *field_y, int *buttons_y, int *cancel_x, int *ok_x) {
    *field_y = dy + TITLEBAR_HEIGHT + 22;
    *buttons_y = dy + g_dh - 16 - g_ok.height;
    *cancel_x = dx + g_dw - 16 - g_cancel.width;
    *ok_x = *cancel_x - BUTTON_GAP - g_ok.width;
}

void prompt_update(int window_w, int window_h) {
    if (!g_open) return;
    int dx, dy;
    ui_dialog_place(&g_drag, 1, window_w, window_h, DEFAULT_W, g_dh, DEFAULT_W, g_dh, TITLEBAR_HEIGHT, 0, &dx, &dy, &g_dw, &g_dh);
    int field_y, buttons_y, cancel_x, ok_x;
    layout(dx, dy, &field_y, &buttons_y, &cancel_x, &ok_x);
    int mx = window_mouse_x(), my = window_mouse_y();
    if (g_input_mode) {
        text_field_update(&g_field, dx + 16, field_y, g_dw - 32, FIELD_H, g_dw - 32);
        if (g_field.focused) window_start_text_input();
        if (window_key_just_pressed_enter() && g_field.text[0]) { finish(1); return; }
    } else if (window_key_just_pressed_enter()) { finish(1); return; }
    if (window_key_just_pressed_escape()) { finish(0); return; }
    if (!window_mouse_left_just_pressed()) return;
    if (ui_dialog_close_button_hit(mx, my, dx + g_dw - 16 - CLOSE_BTN_SIZE, dy + (TITLEBAR_HEIGHT - CLOSE_BTN_SIZE) / 2, CLOSE_BTN_SIZE)) { finish(0); return; }
    if (ui_point_in_rect(mx, my, ok_x, buttons_y, g_ok.width, g_ok.height)) { if (!g_input_mode || g_field.text[0]) finish(1); }
    else if (ui_point_in_rect(mx, my, cancel_x, buttons_y, g_cancel.width, g_cancel.height)) finish(0);
}

void prompt_draw(int window_w, int window_h) {
    if (!g_open) return;
    ui_dialog_draw_backdrop(window_w, window_h);
    int dx, dy;
    ui_dialog_place(&g_drag, 0, window_w, window_h, DEFAULT_W, g_dh, DEFAULT_W, g_dh, TITLEBAR_HEIGHT, 0, &dx, &dy, &g_dw, &g_dh);
    ui_dialog_draw_frame(dx, dy, g_dw, g_dh);
    ui_dialog_draw_title_left(g_title_tex, g_title_w, g_title_h, dx, dy, TITLEBAR_HEIGHT, 16);
    ui_dialog_draw_close_button(dx + g_dw - 16 - CLOSE_BTN_SIZE, dy + (TITLEBAR_HEIGHT - CLOSE_BTN_SIZE) / 2, CLOSE_BTN_SIZE);
    window_fill_rect(dx + 1, dy + TITLEBAR_HEIGHT, g_dw - 2, 1, UI_RGB(UI_COLOR_SURFACE_BORDER));
    int field_y, buttons_y, cancel_x, ok_x;
    layout(dx, dy, &field_y, &buttons_y, &cancel_x, &ok_x);
    if (g_input_mode) text_field_draw(&g_field, dx + 16, field_y, g_dw - 32, FIELD_H, 14);
    else if (g_message_tex) window_draw_texture(g_message_tex, dx + 16, field_y + 4, g_message_w, g_message_h);
    window_draw_texture(g_ok_bg, ok_x, buttons_y, g_ok.width, g_ok.height);
    window_draw_texture(g_ok_txt, ok_x + g_ok.text_offset_x, buttons_y + g_ok.text_offset_y, g_ok.text_img.width, g_ok.text_img.height);
    window_draw_texture(g_cancel_bg, cancel_x, buttons_y, g_cancel.width, g_cancel.height);
    window_draw_texture(g_cancel_txt, cancel_x + g_cancel.text_offset_x, buttons_y + g_cancel.text_offset_y, g_cancel.text_img.width, g_cancel.text_img.height);
}

void prompt_shutdown(void) {
    free_visuals();
    if (g_field_ready) { text_field_free(&g_field); g_field_ready = 0; }
    g_open = 0;
}
