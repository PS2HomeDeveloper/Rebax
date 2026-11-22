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

#ifndef UI_COMMON_H
#define UI_COMMON_H

#include "platform/common/window.h"
#include "editor/ui/font.h"
#include "editor/ui/labeled_button.h"
#include "editor/ui/shape_provider.h"
#include "editor/ui/theme.h"

#define UI_BUTTON_RADIUS      3
#define UI_BUTTON_PADDING_X  14
#define UI_BUTTON_PADDING_Y   6

window_texture_t *ui_make_text_texture(const char *text, font_weight_t weight,
                                        int pixel_size, int *out_w, int *out_h);

void ui_make_blue_button(const char *text, int font_pixel_size,
                          labeled_button_t *out_btn,
                          window_texture_t **out_btn_tex,
                          window_texture_t **out_txt_tex);

void ui_dim_alpha(shape_image_t *img, double factor);

void ui_center_rect(int window_w, int window_h, int w, int h, int *out_x, int *out_y);

int ui_point_in_rect(int px, int py, int x, int y, int w, int h);

int ui_text_contains_ci(const char *text, const char *needle);

#endif
