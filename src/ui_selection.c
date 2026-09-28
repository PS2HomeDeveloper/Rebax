#include <stddef.h>

#include "ui_selection.h"
#include "ui_common.h"

#define UI_SELECTION_HOVER_ALPHA     0.25
#define UI_SELECTION_SELECTED_ALPHA  0.55

static window_texture_t *make_tinted(int w, int h, int corner_radius, double alpha) {
    shape_image_t img = shape_provider_render_rect(w, h, corner_radius,
        UI_COLOR_BUTTON_BLUE.r, UI_COLOR_BUTTON_BLUE.g, UI_COLOR_BUTTON_BLUE.b);
    if (img.pixels == NULL) return NULL;
    ui_dim_alpha(&img, alpha);
    window_texture_t *tex = window_create_texture(img.pixels, img.width, img.height);
    shape_provider_free_image(&img);
    return tex;
}

window_texture_t *ui_selection_make_hover(int w, int h, int corner_radius) {
    return make_tinted(w, h, corner_radius, UI_SELECTION_HOVER_ALPHA);
}

window_texture_t *ui_selection_make_selected(int w, int h, int corner_radius) {
    return make_tinted(w, h, corner_radius, UI_SELECTION_SELECTED_ALPHA);
}
