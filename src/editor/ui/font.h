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

#ifndef FONT_H
#define FONT_H

typedef enum {
    FONT_WEIGHT_REGULAR,
    FONT_WEIGHT_BOLD
} font_weight_t;

typedef struct {
    unsigned char *pixels;
    int width;
    int height;
    int px_w;
    int px_h;
} font_text_image_t;

int font_init(void);

void font_set_scale(float scale);

font_text_image_t font_render_text(const char *text, font_weight_t weight,
                                    int pixel_size);

font_text_image_t font_render_text_colored(const char *text, font_weight_t weight,
                                            int pixel_size, const unsigned char *rgb_by_byte);

int font_measure_text(const char *text, font_weight_t weight, int pixel_size);

void font_free_text_image(font_text_image_t *img);

void font_shutdown(void);

#endif
