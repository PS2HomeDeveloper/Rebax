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

#include "core/i18n.h"
#include <stddef.h>

#include "editor/ui/labeled_button.h"

labeled_button_t labeled_button_create(const char *text, font_weight_t weight,
                                        int font_pixel_size,
                                        int padding_x, int padding_y,
                                        int corner_radius,
                                        unsigned char r, unsigned char g, unsigned char b) {
    labeled_button_t out = {0};

    if (!font_init()) {
        return out;
    }

    font_text_image_t txt = font_render_text(tr(text), weight, font_pixel_size);
    if (txt.pixels == NULL) {
        return out;
    }

    int ink_l = txt.px_w, ink_r = -1, ink_t = txt.px_h, ink_b = -1;
    for (int yy = 0; yy < txt.px_h; yy++) {
        const unsigned char *row = txt.pixels + (size_t)yy * txt.px_w * 4;
        for (int xx = 0; xx < txt.px_w; xx++) {
            if (row[xx * 4 + 3] > 24) {
                if (xx < ink_l) ink_l = xx;
                if (xx > ink_r) ink_r = xx;
                if (yy < ink_t) ink_t = yy;
                if (yy > ink_b) ink_b = yy;
            }
        }
    }
    float inv = txt.px_w > 0 && txt.width > 0 ? (float)txt.width / (float)txt.px_w : 1.0f;
    int lead_x = 0, ink_w = txt.width;
    if (ink_r >= ink_l) {
        lead_x = (int)(ink_l * inv);
        ink_w = (int)((ink_r - ink_l + 1) * inv + 0.999f);
    }
    int btn_w = ink_w + padding_x * 2;
    int btn_h = txt.height + padding_y * 2;

    shape_image_t shape = shape_provider_render_rect(btn_w, btn_h, corner_radius, r, g, b);
    if (shape.pixels == NULL) {
        font_free_text_image(&txt);
        return out;
    }

    out.shape_img      = shape;
    out.text_img       = txt;
    out.width           = shape.width;
    out.height          = shape.height;
    out.text_offset_x  = padding_x - lead_x;
    out.text_offset_y  = padding_y;
    return out;
}

void labeled_button_free(labeled_button_t *btn) {
    if (btn == NULL) {
        return;
    }
    shape_provider_free_image(&btn->shape_img);
    font_free_text_image(&btn->text_img);
    btn->width = btn->height = 0;
}
