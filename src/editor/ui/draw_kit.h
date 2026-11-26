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

#ifndef DRAW_KIT_H
#define DRAW_KIT_H

int ui_kit_text_width(const char *text, int size);
int ui_kit_text_height(const char *text, int size);

void ui_kit_text(const char *text, int size, int x, int y, int r, int g, int b);

void ui_kit_rect(int x, int y, int w, int h, int radius, int r, int g, int b);

void ui_kit_outline(int x, int y, int w, int h, int radius, int r, int g, int b);

void ui_kit_button(const char *text, int x, int y, int w, int h, int enabled, int primary);

void ui_kit_fit_text_weight(const char *text, int weight, int size, int max_w, char *out, int out_size);
void ui_kit_fit_text(const char *text, int size, int max_w, char *out, int out_size);

int ui_kit_hover(int x, int y, int w, int h);

int ui_kit_clicked(int x, int y, int w, int h);

void ui_kit_shutdown(void);

#endif
