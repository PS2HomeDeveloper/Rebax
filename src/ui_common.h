/*
 * ============================================================
 * ui_common.h
 * ============================================================
 * Small UI utilities shared across all engine windows and panels -
 * were duplicated identically in export_dialog, project_dialog,
 * asset_browser and add_node_dialog, etc. Any new window should
 * call them from here instead of writing its own copy.
 * ============================================================
 */

#ifndef UI_COMMON_H
#define UI_COMMON_H

#include "window.h"
#include "font.h"
#include "labeled_button.h"
#include "shape_provider.h"
#include "ui_theme.h"

/* Unified button corner radius, and text padding inside the button */
#define UI_BUTTON_RADIUS      3
#define UI_BUTTON_PADDING_X  14
#define UI_BUTTON_PADDING_Y   6

/* Converts text to an image ready for screen drawing. Returns NULL on failure.
 * out_w/out_h receive the text dimensions in pixels */
window_texture_t *ui_make_text_texture(const char *text, font_weight_t weight,
                                        int pixel_size, int *out_w, int *out_h);

/* Creates a unified blue button sized to fit its text: out_btn is the button itself,
 * out_btn_tex is its background image, and out_txt_tex is its text image */
void ui_make_blue_button(const char *text, int font_pixel_size,
                          labeled_button_t *out_btn,
                          window_texture_t **out_btn_tex,
                          window_texture_t **out_txt_tex);

/* Multiplies the alpha of every pixel in the image by factor (0.25 = four times lighter) */
void ui_dim_alpha(shape_image_t *img, double factor);

/* Computes the top-left corner for an element of size (w, h) centered in the window */
void ui_center_rect(int window_w, int window_h, int w, int h, int *out_x, int *out_y);

/* Is the point (px, py) inside the rectangle? */
int ui_point_in_rect(int px, int py, int x, int y, int w, int h);

/* Does text contain needle (case-insensitive)? If needle is empty returns 1 -
 * used for filtering lists by the search field */
int ui_text_contains_ci(const char *text, const char *needle);

#endif /* UI_COMMON_H */
