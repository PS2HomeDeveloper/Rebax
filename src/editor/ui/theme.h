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

/* theme.h - shared colors and appearance constants for all engine UIs. */
#ifndef UI_THEME_H
#define UI_THEME_H

typedef struct {
    unsigned char r, g, b, a;
} ui_color_t;

static const ui_color_t UI_COLOR_BLACK_MUTED = {0x0F, 0x13, 0x1A, 0xFF};
static const ui_color_t UI_COLOR_GRAY_MUTED = {0x30, 0x36, 0x42, 0xFF};
static const ui_color_t UI_COLOR_BUTTON_BLUE = {0x2A, 0x6F, 0x97, 0xFF};
static const ui_color_t UI_COLOR_VIEWPORT_BLACK = {0x00, 0x00, 0x00, 0xFF};
static const ui_color_t UI_COLOR_PANEL_BG = {0x24, 0x2A, 0x35, 0xFF};
static const ui_color_t UI_COLOR_HOVER_BLUE = {0x20, 0x55, 0x75, 0xFF};
static const ui_color_t UI_COLOR_TAB_ACTIVE = {0x1E, 0x3F, 0x56, 0xFF};
static const ui_color_t UI_COLOR_TAB_HOVER = {0x17, 0x2B, 0x3A, 0xFF};

#define UI_RGB(color) (color).r, (color).g, (color).b

static const ui_color_t UI_COLOR_BACKDROP = {0x00, 0x00, 0x00, 0xFF};
static const ui_color_t UI_COLOR_SURFACE_BASE = {0x11, 0x16, 0x1D, 0xFF};
static const ui_color_t UI_COLOR_SURFACE_RAISED = {0x18, 0x1E, 0x27, 0xFF};
static const ui_color_t UI_COLOR_SURFACE_PANEL = {0x1F, 0x26, 0x31, 0xFF};
static const ui_color_t UI_COLOR_SURFACE_CONTROL = {0x28, 0x30, 0x3C, 0xFF};
static const ui_color_t UI_COLOR_SURFACE_BORDER = {0x34, 0x3D, 0x4B, 0xFF};
static const ui_color_t UI_COLOR_SCROLL_TRACK = {0x28, 0x30, 0x3C, 0xFF};
static const ui_color_t UI_COLOR_SCROLL_THUMB = {0x5A, 0x68, 0x7C, 0xFF};
static const ui_color_t UI_COLOR_TEXT_BRIGHT = {0xF4, 0xF6, 0xFA, 0xFF};
static const ui_color_t UI_COLOR_TEXT_PRIMARY = {0xE6, 0xEA, 0xF0, 0xFF};
static const ui_color_t UI_COLOR_TEXT_MUTED = {0xA9, 0xB3, 0xC1, 0xFF};
static const ui_color_t UI_COLOR_TEXT_FAINT = {0x8A, 0x94, 0xA3, 0xFF};
static const ui_color_t UI_COLOR_TEXT_DISABLED = {0x5F, 0x6B, 0x7C, 0xFF};
static const ui_color_t UI_COLOR_SELECTION = {0x4B, 0x9B, 0xFF, 0xFF};
static const ui_color_t UI_COLOR_INFO = {0x50, 0x9B, 0xD2, 0xFF};
static const ui_color_t UI_COLOR_CODE_CYAN = {0x5A, 0xD2, 0xF0, 0xFF};
static const ui_color_t UI_COLOR_SUCCESS = {0x46, 0xDC, 0x78, 0xFF};
static const ui_color_t UI_COLOR_WARNING = {0xF5, 0xBE, 0x37, 0xFF};
static const ui_color_t UI_COLOR_WARNING_SOFT = {0xDC, 0xC3, 0x7D, 0xFF};
static const ui_color_t UI_COLOR_DANGER = {0xEB, 0x69, 0x5F, 0xFF};
static const ui_color_t UI_COLOR_DANGER_SOFT = {0xF0, 0x96, 0x8C, 0xFF};
static const ui_color_t UI_COLOR_DANGER_BG = {0x5A, 0x28, 0x28, 0xFF};

#endif /* UI_THEME_H */
