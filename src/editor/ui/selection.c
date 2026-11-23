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

#include <stddef.h>

#include "editor/ui/selection.h"
#include "editor/ui/base.h"

#define UI_SELECTION_HOVER_ALPHA     0.25
#define UI_SELECTION_SELECTED_ALPHA  0.55

static window_texture_t *make_tinted(int w, int h, int corner_radius, double alpha) {
    shape_image_t img = shape_provider_render_rect(w, h, corner_radius,
        UI_COLOR_BUTTON_BLUE.r, UI_COLOR_BUTTON_BLUE.g, UI_COLOR_BUTTON_BLUE.b);
    if (img.pixels == NULL) return NULL;
    ui_dim_alpha(&img, alpha);
    window_texture_t *tex = window_create_texture(img.pixels, img.px_w, img.px_h);
    shape_provider_free_image(&img);
    return tex;
}

window_texture_t *ui_selection_make_hover(int w, int h, int corner_radius) {
    return make_tinted(w, h, corner_radius, UI_SELECTION_HOVER_ALPHA);
}

window_texture_t *ui_selection_make_selected(int w, int h, int corner_radius) {
    return make_tinted(w, h, corner_radius, UI_SELECTION_SELECTED_ALPHA);
}
