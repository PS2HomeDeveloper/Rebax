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

#ifndef LABELED_BUTTON_H
#define LABELED_BUTTON_H

#include "editor/ui/shape_provider.h"
#include "editor/ui/font.h"

typedef struct {
    shape_image_t      shape_img;
    font_text_image_t  text_img;
    int width;
    int height;
    int text_offset_x;
    int text_offset_y;
} labeled_button_t;

labeled_button_t labeled_button_create(const char *text, font_weight_t weight,
                                        int font_pixel_size,
                                        int padding_x, int padding_y,
                                        int corner_radius,
                                        unsigned char r, unsigned char g, unsigned char b);

void labeled_button_free(labeled_button_t *btn);

#endif
