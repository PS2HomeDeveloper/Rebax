/*
 * ============================================================
 * labeled_button.h
 * ============================================================
 * Combines a shape (from shape_provider) and text (from font) into a single button,
 * with a size automatically computed = text size + fixed padding on each side. Provides
 * a ready way for any UI library (ui_project_center, project_dialog, etc.) to create a
 * button that always fits its content.
 * ============================================================
 */

#ifndef LABELED_BUTTON_H
#define LABELED_BUTTON_H

#include "shape_provider.h"
#include "font.h"

typedef struct {
    shape_image_t      shape_img;    /* Button background pixels at the final size */
    font_text_image_t  text_img;     /* The text pixels themselves */
    int width;                       /* = shape_img.width, for convenience */
    int height;                      /* = shape_img.height, for convenience */
    int text_offset_x;               /* Text position inside the button from the top-left corner */
    int text_offset_y;
} labeled_button_t;

/* Creates a button with a size automatically computed = text size + fixed padding on each side,
 * with a specified color and corner radius. Calls font_init internally when needed.
 * On failure, fields are empty (width = 0) */
labeled_button_t labeled_button_create(const char *text, font_weight_t weight,
                                        int font_pixel_size,
                                        int padding_x, int padding_y,
                                        int corner_radius,
                                        unsigned char r, unsigned char g, unsigned char b);

/* Frees all memory associated with a button created via labeled_button_create */
void labeled_button_free(labeled_button_t *btn);

#endif /* LABELED_BUTTON_H */
