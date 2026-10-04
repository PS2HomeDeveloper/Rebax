/*
 * ============================================================
 * shape_provider.h
 * ============================================================
 * Provides shapes (rounded-rects) to any other library that requests them -
 * buttons, text fields, panels, or anything that needs smooth edges.
 *
 * Depends on no external asset (no image, no file) - the shape is computed
 * mathematically on demand (SDF: signed distance field), at any size, color
 * and corner smoothness requested, with perfect quality regardless of scale.
 *
 * This is the only file in the project responsible for generating smooth-edged shapes.
 * ============================================================
 */

#ifndef SHAPE_PROVIDER_H
#define SHAPE_PROVIDER_H

/* RGBA pixel image ready for drawing (32 bits per pixel) */
typedef struct {
    unsigned char *pixels; /* Raw data, length width * height * 4 bytes */
    int width;
    int height;
} shape_image_t;

/* Produces a rounded-rect at the requested size, color and corner smoothness
 * (corner_radius in pixels - larger values produce rounder corners; zero means sharp corners).
 * Requires no prior initialization or external resource - ready to use from the first call.
 * The result is a new memory allocation - it must be freed later with
 * shape_provider_free_image after use. */
shape_image_t shape_provider_render_rect(int width, int height, int corner_radius,
                                          unsigned char r, unsigned char g, unsigned char b);

/* Frees the memory allocated for an image produced by shape_provider_render_rect */
void shape_provider_free_image(shape_image_t *img);

#endif /* SHAPE_PROVIDER_H */
