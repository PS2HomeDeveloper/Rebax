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
 * Licensed under the MIT License.
 * See the LICENSE file for details.
 */

#ifndef DRAW_KIT_H
#define DRAW_KIT_H

int ui_kit_text_width(const char *text, int size);
int ui_kit_text_height(const char *text, int size);

void ui_kit_text(const char *text, int size, int x, int y, int r, int g, int b);

void ui_kit_rect(int x, int y, int w, int h, int radius, int r, int g, int b);

void ui_kit_outline(int x, int y, int w, int h, int radius, int r, int g, int b);

void ui_kit_button(const char *text, int x, int y, int w, int h, int enabled, int primary);

int ui_kit_hover(int x, int y, int w, int h);

int ui_kit_clicked(int x, int y, int w, int h);

void ui_kit_shutdown(void);

#endif
