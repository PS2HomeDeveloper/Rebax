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

#include "core/engine_settings.h"
#include "core/i18n.h"
#include "editor/panels/settings_page.h"
#include "editor/ui/base.h"
#include "editor/ui/draw_kit.h"
#include "packages/package_manager.h"
#include "platform/common/window.h"

#define SIDEBAR_W 190
#define TOP_H 56
#define ROW_H 44
#define CATEGORY_COUNT 4

typedef enum { CAT_GENERAL, CAT_INTERFACE, CAT_PACKAGES, CAT_ABOUT } category_t;

static const char *g_categories[CATEGORY_COUNT] = {"General", "Interface", "Packages", "About"};
static const int g_scale_options[] = {75, 100, 125, 150, 175, 200};
#define SCALE_OPTION_COUNT 6

static int g_category;
static int g_close;
static int g_scroll;

void settings_page_open(void) {
    g_category = CAT_GENERAL;
    g_close = 0;
    g_scroll = 0;
    package_refresh_states();
}

int settings_page_wants_close(void) {
    int v = g_close;
    g_close = 0;
    return v;
}

static void text_c(const char *s, int size, int x, int y, const ui_color_t *c) {
    ui_kit_text(tr(s), size, x, y, c->r, c->g, c->b);
}

static int draw_section_title(const char *s, int x, int y) {
    ui_color_t c = UI_COLOR_TEXT_BRIGHT;
    text_c(s, 20, x, y, &c);
    return y + 38;
}

static int choice_button(const char *label, int x, int y, int w, int selected) {
    int h = 34;
    int hover = ui_kit_hover(x, y, w, h);
    if (selected) ui_kit_rect(x, y, w, h, 7, UI_RGB(UI_COLOR_BUTTON_BLUE));
    else ui_kit_rect(x, y, w, h, 7, UI_RGB(hover ? UI_COLOR_SURFACE_BORDER : UI_COLOR_SURFACE_CONTROL));
    int tw = ui_kit_text_width(label, 14), th = ui_kit_text_height(label, 14);
    ui_kit_text(label, 14, x + (w - tw) / 2, y + (h - th) / 2, UI_RGB(UI_COLOR_TEXT_BRIGHT));
    return ui_kit_clicked(x, y, w, h);
}

static int g_language_changed = 0;

static int draw_general(int x, int y, int w) {
    ui_color_t muted = UI_COLOR_TEXT_MUTED;
    y = draw_section_title("Language", x, y);
    text_c("Choose the language used by the engine.", 14, x, y, &muted);
    y += 30;
    int cx = x;
    for (int i = 0; i < i18n_language_count(); i++) {
        const char *name = i18n_language_name(i);
        int bw = ui_kit_text_width(name, 14) + 36;
        if (bw < 110) bw = 110;
        if (cx + bw > x + w && cx > x) { cx = x; y += 42; }
        if (choice_button(name, cx, y, bw, strcmp(i18n_current_language(), i18n_language_code(i)) == 0)) {
            i18n_set_language(i18n_language_code(i));
            engine_settings_set_language(i18n_language_code(i));
            g_language_changed = 1;
        }
        cx += bw + 10;
    }
    y += 52;
    text_c("More languages can be added from the languages folder.", 13, x, y, &muted);
    if (g_language_changed) {
        y += 28;
        text_c("Restart the engine to apply the language everywhere.", 13, x, y, &muted);
    }
    return y + 40;
}

static int draw_interface(int x, int y, int w) {
    ui_color_t muted = UI_COLOR_TEXT_MUTED;
    y = draw_section_title("Interface size", x, y);
    text_c("Makes everything larger or smaller. The default is chosen for your screen.", 14, x, y, &muted);
    y += 30;
    int current = engine_settings_get()->ui_scale_percent;
    if (current == 0) current = 100;
    int cx = x;
    for (int i = 0; i < SCALE_OPTION_COUNT; i++) {
        char label[16];
        snprintf(label, sizeof(label), "%d%%", g_scale_options[i]);
        int bw = 84;
        if (cx + bw > x + w && cx > x) { cx = x; y += 42; }
        if (choice_button(label, cx, y, bw, current == g_scale_options[i])) {
            engine_settings_set_ui_scale(g_scale_options[i]);
            window_set_ui_scale_percent(g_scale_options[i]);
        }
        cx += bw + 10;
    }
    return y + 52;
}

static int draw_packages(int x, int y, int w) {
    ui_color_t muted = UI_COLOR_TEXT_MUTED;
    ui_color_t bright = UI_COLOR_TEXT_PRIMARY;
    y = draw_section_title("Installed packages", x, y);
    text_c("Remove a package to free up storage. You can install it again later.", 14, x, y, &muted);
    y += 34;
    int busy = package_is_busy();
    for (int p = 0; p < PKG_PLATFORM_COUNT; p++) {
        pkg_state_t st = package_platform_state(p);
        ui_kit_rect(x, y, w, ROW_H, 8, UI_RGB(UI_COLOR_SURFACE_PANEL));
        ui_kit_text(package_platform_name(p), 15, x + 16, y + (ROW_H - 18) / 2, bright.r, bright.g, bright.b);
        const char *state = st == PKG_STATE_INSTALLED ? "Installed" :
                            st == PKG_STATE_PARTIAL ? "Partly installed" :
                            st == PKG_STATE_UNAVAILABLE ? "Unavailable" : "Not installed";
        const ui_color_t *sc = st == PKG_STATE_INSTALLED ? &UI_COLOR_SUCCESS : &UI_COLOR_TEXT_FAINT;
        int removable = (st == PKG_STATE_INSTALLED || st == PKG_STATE_PARTIAL) && !busy;
        int bw = 100;
        int bx = x + w - bw - 10;
        const char *translated = tr(state);
        ui_kit_text(translated, 13, bx - ui_kit_text_width(translated, 13) - 16, y + (ROW_H - 16) / 2, sc->r, sc->g, sc->b);
        if (st == PKG_STATE_INSTALLED || st == PKG_STATE_PARTIAL) {
            ui_kit_button(tr("Uninstall"), bx, y + 6, bw, ROW_H - 12, removable, 0);
            if (removable && ui_kit_clicked(bx, y + 6, bw, ROW_H - 12)) package_uninstall_platform(p);
        }
        y += ROW_H + 8;
    }
    return y + 10;
}

static int draw_about(int x, int y, int w) {
    (void)w;
    ui_color_t muted = UI_COLOR_TEXT_MUTED;
    y = draw_section_title("Rebax Engine", x, y);
    text_c("Copyright (c) 2026 PS2HomeDeveloper", 14, x, y, &muted);
    y += 26;
    text_c("Released under the Mozilla Public License 2.0.", 14, x, y, &muted);
    return y + 40;
}

void settings_page_update_and_draw(int window_w, int window_h) {
    window_clear(UI_RGB(UI_COLOR_BLACK_MUTED));

    int cx = SIDEBAR_W + 28;
    int cw = window_w - cx - 28;
    if (cw < 160) cw = 160;
    int view_h = window_h - TOP_H;
    int wheel = window_mouse_wheel_delta();
    if (wheel != 0) g_scroll -= wheel * 40;
    if (g_scroll < 0) g_scroll = 0;

    int y = TOP_H + 24 - g_scroll;
    int end;
    switch (g_category) {
        case CAT_GENERAL: end = draw_general(cx, y, cw); break;
        case CAT_INTERFACE: end = draw_interface(cx, y, cw); break;
        case CAT_PACKAGES: end = draw_packages(cx, y, cw); break;
        default: end = draw_about(cx, y, cw); break;
    }
    int content_h = end - (TOP_H - g_scroll) + 24;
    int max_scroll = content_h > view_h ? content_h - view_h : 0;
    if (g_scroll > max_scroll) g_scroll = max_scroll;

    ui_kit_rect(0, 0, SIDEBAR_W, window_h, 0, UI_RGB(UI_COLOR_SURFACE_PANEL));
    ui_kit_rect(0, 0, window_w, TOP_H, 0, UI_RGB(UI_COLOR_SURFACE_RAISED));

    ui_kit_button(tr("Back"), 12, 11, 90, 34, 1, 0);
    if (ui_kit_clicked(12, 11, 90, 34)) g_close = 1;
    const char *title = tr("Settings");
    ui_kit_text(title, 20, (window_w - ui_kit_text_width(title, 20)) / 2, (TOP_H - 24) / 2, UI_RGB(UI_COLOR_TEXT_BRIGHT));

    int cy = TOP_H + 12;
    for (int i = 0; i < CATEGORY_COUNT; i++) {
        int sel = (g_category == i);
        int hover = ui_kit_hover(8, cy, SIDEBAR_W - 16, 40);
        if (sel) ui_kit_rect(8, cy, SIDEBAR_W - 16, 40, 7, UI_RGB(UI_COLOR_BUTTON_BLUE));
        else if (hover) ui_kit_rect(8, cy, SIDEBAR_W - 16, 40, 7, UI_RGB(UI_COLOR_SURFACE_CONTROL));
        const char *label = tr(g_categories[i]);
        ui_kit_text(label, 15, 24, cy + (40 - ui_kit_text_height(label, 15)) / 2, UI_RGB(UI_COLOR_TEXT_BRIGHT));
        if (ui_kit_clicked(8, cy, SIDEBAR_W - 16, 40)) { g_category = i; g_scroll = 0; }
        cy += 46;
    }
}
