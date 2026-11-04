/*
 * ============================================================
 * shape_provider.c
 * ============================================================
 * The approach: for each pixel in the output image we compute the
 * "signed distance" to the shape edge (the standard math formula for a
 * rounded-rect). If the pixel is fully inside: solid color. Fully outside:
 * fully transparent. If very close to the edge (less than one pixel):
 * partially/gradually transparent - this is exactly what gives a visual
 * perception of "smoothness".
 *
 * Because the computation is mathematically exact (not copying pixels from
 * a pre-rendered image), the smoothness is always perfect at any size and
 * corner radius, without limitations.
 * ============================================================
 */

#include <stdlib.h>
#include <math.h>

#include "shape_provider.h"

/* The signed distance between a point and a rounded-rect centered at the origin.
 * Negative = inside the shape, positive = outside, zero = exactly on the edge.
 * px, py are the point coordinates relative to the rectangle center.
 * hw, hh are half-width and half-height. r is the corner radius. */
static double signed_distance_rounded_rect(double px, double py,
                                            double hw, double hh, double r) {
    double qx = fabs(px) - (hw - r);
    double qy = fabs(py) - (hh - r);
    double outside_x = qx > 0.0 ? qx : 0.0;
    double outside_y = qy > 0.0 ? qy : 0.0;
    double outside_dist = sqrt(outside_x * outside_x + outside_y * outside_y);
    double inside_dist = qx > qy ? qy : qx; /* min(qx, qy) */
    if (inside_dist > 0.0) inside_dist = 0.0;
    return outside_dist + inside_dist - r;
}

shape_image_t shape_provider_render_rect(int width, int height, int corner_radius,
                                          unsigned char r, unsigned char g, unsigned char b) {
    shape_image_t out = {0};

    if (width <= 0 || height <= 0) {
        return out;
    }

    /* The radius must not exceed half the smaller dimension, otherwise the shape will distort */
    double radius = (double)corner_radius;
    double max_radius = (width < height ? width : height) / 2.0;
    if (radius > max_radius) radius = max_radius;
    if (radius < 0.0) radius = 0.0;

    out.width = width;
    out.height = height;
    out.pixels = (unsigned char *)malloc((size_t)width * (size_t)height * 4);
    if (out.pixels == NULL) {
        out.width = out.height = 0;
        return out;
    }

    double hw = width  / 2.0;
    double hh = height / 2.0;

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            /* Pixel center (x+0.5, y+0.5) relative to the rectangle center */
            double px = (x + 0.5) - hw;
            double py = (y + 0.5) - hh;

            double dist = signed_distance_rounded_rect(px, py, hw, hh, radius);

            /* Pixel coverage (0 = fully transparent, 1 = fully opaque) - smooth
             * gradient over one pixel around the edge (dist = 0) */
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

void shape_provider_free_image(shape_image_t *img) {
    if (img != NULL && img->pixels != NULL) {
        free(img->pixels);
        img->pixels = NULL;
        img->width = img->height = 0;
    }
}
