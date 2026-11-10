/*
 * ============================================================
 * element_3d.c
 * ============================================================
 * Same idea as Element2D but with three-dimensional spatial meaning -
 * position/rotation/scale on X/Y/Z axes. Currently a preliminary
 * structure for testing (export logic) - no actual rendering yet.
 * ============================================================
 */

#include "node_interface.h"

/* The real Element3D data - same idea as element2d_instance_t */
typedef struct {
    float position_x;
    float position_y;
    float position_z;
    float rotation_x;
    float rotation_y;
    float rotation_z;
    float scale_x;
    float scale_y;
    float scale_z;
} element3d_instance_t;

static void element3d_init(void *self) {
    (void)self;
}

static void element3d_update(void *self, float delta_time) {
    (void)self;
    (void)delta_time;
}

static void element3d_draw(void *self) {
    (void)self;
    /* No actual rendering yet - preliminary structure for testing */
}

static void element3d_destroy(void *self) {
    (void)self;
}

static const node_property_t element3d_properties[] = {
    { "Position X",  NODE_PROPERTY_TYPE_FLOAT, { .f = 0.0f }, 0 },
    { "Position Y",  NODE_PROPERTY_TYPE_FLOAT, { .f = 0.0f }, 0 },
    { "Position Z",  NODE_PROPERTY_TYPE_FLOAT, { .f = 0.0f }, 0 },
    { "Rotation X",  NODE_PROPERTY_TYPE_FLOAT, { .f = 0.0f }, 0 },
    { "Rotation Y",  NODE_PROPERTY_TYPE_FLOAT, { .f = 0.0f }, 0 },
    { "Rotation Z",  NODE_PROPERTY_TYPE_FLOAT, { .f = 0.0f }, 0 },
    { "Scale X",     NODE_PROPERTY_TYPE_FLOAT, { .f = 1.0f }, 0 },
    { "Scale Y",     NODE_PROPERTY_TYPE_FLOAT, { .f = 1.0f }, 0 },
    { "Scale Z",     NODE_PROPERTY_TYPE_FLOAT, { .f = 1.0f }, 0 }
};

/* The property offsets above inside element3d_instance_t - in exactly
 * the same order, completely separate from element3d_properties (same
 * principle as Element2D) */
static const size_t element3d_offsets[] = {
    offsetof(element3d_instance_t, position_x),
    offsetof(element3d_instance_t, position_y),
    offsetof(element3d_instance_t, position_z),
    offsetof(element3d_instance_t, rotation_x),
    offsetof(element3d_instance_t, rotation_y),
    offsetof(element3d_instance_t, rotation_z),
    offsetof(element3d_instance_t, scale_x),
    offsetof(element3d_instance_t, scale_y),
    offsetof(element3d_instance_t, scale_z)
};

const node_interface_t element3d_interface = {
    element3d_init,
    element3d_update,
    element3d_draw,
    element3d_destroy,
    element3d_properties,
    sizeof(element3d_properties) / sizeof(element3d_properties[0]),
    sizeof(element3d_instance_t),
    element3d_offsets
};

/* @NODE type=NODE_TYPE_ELEMENT_3D name=Element3D icon=element_3d properties=element3d_properties */
/* The above declaration is the only source that defines Element3D to
 * the development editor - it must remain the last line in the file */
