/*
 * ============================================================
 * element_2d.c
 * ============================================================
 * First node with actual spatial meaning - 2D position/rotation/scale.
 * Currently a preliminary structure for testing (export logic) - no
 * actual rendering yet.
 * ============================================================
 */

#include "node_interface.h"

/* The real Element2D data - this layout is what instance_size actually
 * allocates in bytes when the scene loads on the PS2, and what the
 * offsetof() below points to for its fields exactly */
typedef struct {
    float position_x;
    float position_y;
    float rotation;
    float scale_x;
    float scale_y;
} element2d_instance_t;

static void element2d_init(void *self) {
    (void)self;
}

static void element2d_update(void *self, float delta_time) {
    (void)self;
    (void)delta_time;
}

static void element2d_draw(void *self) {
    (void)self;
    /* No actual rendering yet - preliminary structure for testing */
}

static void element2d_destroy(void *self) {
    (void)self;
}

static const node_property_t element2d_properties[] = {
    { "Position X", NODE_PROPERTY_TYPE_FLOAT, { .f = 0.0f }, 0 },
    { "Position Y", NODE_PROPERTY_TYPE_FLOAT, { .f = 0.0f }, 0 },
    { "Rotation",   NODE_PROPERTY_TYPE_FLOAT, { .f = 0.0f }, 0 },
    { "Scale X",    NODE_PROPERTY_TYPE_FLOAT, { .f = 1.0f }, 0 },
    { "Scale Y",    NODE_PROPERTY_TYPE_FLOAT, { .f = 1.0f }, 0 }
};

/* The property offsets above inside element2d_instance_t - in exactly
 * the same order (index-to-index). The scene loader at export time uses
 * them to write each property's value to its real location in memory -
 * the development editor (aarch64) neither reads nor needs this array
 * at all (which is why it's completely separate from
 * element2d_properties above, not an extra field in it) */
static const size_t element2d_offsets[] = {
    offsetof(element2d_instance_t, position_x),
    offsetof(element2d_instance_t, position_y),
    offsetof(element2d_instance_t, rotation),
    offsetof(element2d_instance_t, scale_x),
    offsetof(element2d_instance_t, scale_y)
};

const node_interface_t element2d_interface = {
    element2d_init,
    element2d_update,
    element2d_draw,
    element2d_destroy,
    element2d_properties,
    sizeof(element2d_properties) / sizeof(element2d_properties[0]),
    sizeof(element2d_instance_t),
    element2d_offsets
};

/* @NODE type=NODE_TYPE_ELEMENT_2D name=Element2D icon=element_2d properties=element2d_properties */
/* The above declaration is the only source that defines Element2D to
 * the development editor - the build file reads it automatically on
 * each build (especially the property array element2d_properties above
 * - copied verbatim with no manual duplication elsewhere). It must
 * remain the last line in the file */
