#include <stddef.h>
#include <string.h>
#include <ctype.h>

#include "ui_common.h"

window_texture_t *ui_make_text_texture(const char *text, font_weight_t weight,
                                        int pixel_size, int *out_w, int *out_h) {
    font_text_image_t img = font_render_text(text, weight, pixel_size);
    window_texture_t *tex = NULL;
    if (img.pixels != NULL) {
        tex = window_create_texture(img.pixels, img.width, img.height);
        *out_w = img.width;
        *out_h = img.height;
        font_free_text_image(&img);
    }
    return tex;
}

void ui_make_blue_button(const char *text, int font_pixel_size,
                          labeled_button_t *out_btn,
                          window_texture_t **out_btn_tex,
                          window_texture_t **out_txt_tex) {
    *out_btn = labeled_button_create(text, FONT_WEIGHT_REGULAR, font_pixel_size,
                                      UI_BUTTON_PADDING_X, UI_BUTTON_PADDING_Y,
                                      UI_BUTTON_RADIUS,
                                      UI_COLOR_BUTTON_BLUE.r, UI_COLOR_BUTTON_BLUE.g,
                                      UI_COLOR_BUTTON_BLUE.b);
    if (out_btn->width > 0) {
        *out_btn_tex = window_create_texture(out_btn->shape_img.pixels,
                                              out_btn->shape_img.width, out_btn->shape_img.height);
        *out_txt_tex = window_create_texture(out_btn->text_img.pixels,
                                              out_btn->text_img.width, out_btn->text_img.height);
    }
}

void ui_dim_alpha(shape_image_t *img, double factor) {
    if (img->pixels == NULL) return;
    int count = img->width * img->height;
    for (int i = 0; i < count; i++) {
        unsigned char *a = &img->pixels[i * 4 + 3];
        *a = (unsigned char)((double)(*a) * factor);
    }
}

void ui_center_rect(int window_w, int window_h, int w, int h, int *out_x, int *out_y) {
    *out_x = (window_w - w) / 2;
    *out_y = (window_h - h) / 2;
}

int ui_point_in_rect(int px, int py, int x, int y, int w, int h) {
    return px >= x && px < x + w && py >= y && py < y + h;
}

int ui_text_contains_ci(const char *text, const char *needle) {
    if (needle[0] == '\0') return 1;
    size_t needle_len = strlen(needle);
    size_t text_len = strlen(text);
    if (needle_len > text_len) return 0;
    for (size_t i = 0; i + needle_len <= text_len; i++) {
        size_t j = 0;
        while (j < needle_len &&
               tolower((unsigned char)text[i + j]) == tolower((unsigned char)needle[j])) {
            j++;
        }
        if (j == needle_len) return 1;
    }
    return 0;
}
