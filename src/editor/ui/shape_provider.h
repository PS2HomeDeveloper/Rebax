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

#ifndef SHAPE_PROVIDER_H
#define SHAPE_PROVIDER_H

typedef struct {
    unsigned char *pixels;
    int width;
    int height;
    int px_w;
    int px_h;
} shape_image_t;

void shape_provider_set_scale(float scale);
float shape_provider_get_scale(void);
int shape_provider_to_px(int logical);
shape_image_t shape_provider_render_rect_px(int px_w, int px_h, double radius_px,
                                             unsigned char r, unsigned char g, unsigned char b);

shape_image_t shape_provider_render_rect(int width, int height, int corner_radius,
                                          unsigned char r, unsigned char g, unsigned char b);

void shape_provider_free_image(shape_image_t *img);

#endif
