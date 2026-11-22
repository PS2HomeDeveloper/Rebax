/*
 *  ____  _____ ____    _    __  __
 * |  _ \| ____| __ )  / \   \ \/ /
 * | |_) |  _| |  _ \ / _ \   \  / 
 * |  _ <| |___| |_) / ___ \  /  \ 
 * |_| \_\_____|____/_/   \_\/_/\_\
 *
 * Rebax Engine
 * Copyright (c) 2026 PS1HomeDeveloper
 *
 * Licensed under the MIT License.
 * See the LICENSE file for details.
 */

#include "node_interface.h"

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
