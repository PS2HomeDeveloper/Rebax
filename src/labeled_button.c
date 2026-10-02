/*
 * ============================================================
 * labeled_button.c
 * ============================================================
 */

#include <stddef.h>

#include "labeled_button.h"

labeled_button_t labeled_button_create(const char *text, font_weight_t weight,
                                        int font_pixel_size,
                                        int padding_x, int padding_y,
                                        int corner_radius,
                                        unsigned char r, unsigned char g, unsigned char b) {
    labeled_button_t out = {0};

    if (!font_init()) {
        return out;
    }

    font_text_image_t txt = font_render_text(text, weight, font_pixel_size);
    if (txt.pixels == NULL) {
        return out;
    }

    int btn_w = txt.width  + padding_x * 2;
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
    out.text_offset_x  = padding_x;
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
