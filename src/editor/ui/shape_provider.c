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

#include <stdlib.h>
#include <math.h>

#include "editor/ui/shape_provider.h"

static double signed_distance_rounded_rect(double px, double py,
                                            double hw, double hh, double r) {
    double qx = fabs(px) - (hw - r);
    double qy = fabs(py) - (hh - r);
    double outside_x = qx > 0.0 ? qx : 0.0;
    double outside_y = qy > 0.0 ? qy : 0.0;
    double outside_dist = sqrt(outside_x * outside_x + outside_y * outside_y);
    double inside_dist = qx > qy ? qy : qx;
    if (inside_dist > 0.0) inside_dist = 0.0;
    return outside_dist + inside_dist - r;
}

static float g_scale = 1.0f;

void shape_provider_set_scale(float scale) {
    g_scale = scale < 0.5f ? 0.5f : scale;
}

float shape_provider_get_scale(void) {
    return g_scale;
}

int shape_provider_to_px(int logical) {
    int v = (int)((float)logical * g_scale + 0.5f);
    return v < 1 && logical > 0 ? 1 : v;
}

shape_image_t shape_provider_render_rect_px(int width, int height, double radius,
                                             unsigned char r, unsigned char g, unsigned char b) {
    shape_image_t out = {0};

    if (width <= 0 || height <= 0) {
        return out;
    }

    double max_radius = (width < height ? width : height) / 2.0;
    if (radius > max_radius) radius = max_radius;
    if (radius < 0.0) radius = 0.0;

    out.px_w = width;
    out.px_h = height;
    out.pixels = (unsigned char *)malloc((size_t)width * (size_t)height * 4);
    if (out.pixels == NULL) {
        out.px_w = out.px_h = 0;
        return out;
    }

    double hw = width  / 2.0;
    double hh = height / 2.0;

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            double px = (x + 0.5) - hw;
            double py = (y + 0.5) - hh;
            double dist = signed_distance_rounded_rect(px, py, hw, hh, radius);
            double coverage = 0.5 - dist;
            if (coverage < 0.0) coverage = 0.0;
            if (coverage > 1.0) coverage = 1.0;
            unsigned char *px_out = &out.pixels[(y * width + x) * 4];
            px_out[0] = r;
            px_out[1] = g;
            px_out[2] = b;
            px_out[3] = (unsigned char)(coverage * 255.0);
        }
    }

    return out;
}

shape_image_t shape_provider_render_rect(int width, int height, int corner_radius,
                                          unsigned char r, unsigned char g, unsigned char b) {
    if (width <= 0 || height <= 0) {
        shape_image_t empty = {0};
        return empty;
    }
    shape_image_t out = shape_provider_render_rect_px(shape_provider_to_px(width), shape_provider_to_px(height),
                                                       (double)corner_radius * g_scale, r, g, b);
    if (out.pixels != NULL) {
        out.width = width;
        out.height = height;
    }
    return out;
}

void shape_provider_free_image(shape_image_t *img) {
    if (img != NULL && img->pixels != NULL) {
        free(img->pixels);
        img->pixels = NULL;
        img->width = img->height = 0;
        img->px_w = img->px_h = 0;
    }
}
