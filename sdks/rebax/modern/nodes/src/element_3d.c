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
 * Licensed under the MIT License.
 * See the LICENSE file for details.
 */

#include "node_interface.h"

typedef struct {
    float position_x, position_y, position_z;
    float rotation_x, rotation_y, rotation_z;
    float scale_x, scale_y, scale_z;
} element3d_instance_t;
static void element3d_init(void *self) { (void)self; }
static void element3d_update(void *self, float delta_time) { (void)self; (void)delta_time; }
static void element3d_draw(void *self) { (void)self; }
static void element3d_destroy(void *self) { (void)self; }
static const node_property_t element3d_properties[] = {
    { "Position X", NODE_PROPERTY_TYPE_FLOAT, { .f = 0.0f }, 0 },
    { "Position Y", NODE_PROPERTY_TYPE_FLOAT, { .f = 0.0f }, 0 },
    { "Position Z", NODE_PROPERTY_TYPE_FLOAT, { .f = 0.0f }, 0 },
    { "Rotation X", NODE_PROPERTY_TYPE_FLOAT, { .f = 0.0f }, 0 },
    { "Rotation Y", NODE_PROPERTY_TYPE_FLOAT, { .f = 0.0f }, 0 },
    { "Rotation Z", NODE_PROPERTY_TYPE_FLOAT, { .f = 0.0f }, 0 },
    { "Scale X", NODE_PROPERTY_TYPE_FLOAT, { .f = 1.0f }, 0 },
    { "Scale Y", NODE_PROPERTY_TYPE_FLOAT, { .f = 1.0f }, 0 },
    { "Scale Z", NODE_PROPERTY_TYPE_FLOAT, { .f = 1.0f }, 0 }
};
static const size_t element3d_offsets[] = {
    offsetof(element3d_instance_t, position_x), offsetof(element3d_instance_t, position_y),
    offsetof(element3d_instance_t, position_z), offsetof(element3d_instance_t, rotation_x),
    offsetof(element3d_instance_t, rotation_y), offsetof(element3d_instance_t, rotation_z),
    offsetof(element3d_instance_t, scale_x), offsetof(element3d_instance_t, scale_y),
    offsetof(element3d_instance_t, scale_z)
};
const node_interface_t element3d_interface = {
    element3d_init, element3d_update, element3d_draw, element3d_destroy,
    element3d_properties, (int)(sizeof(element3d_properties) / sizeof(element3d_properties[0])),
    sizeof(element3d_instance_t), element3d_offsets
};
/* @NODE type=NODE_TYPE_ELEMENT_3D name=Element3D icon=element_3d properties=element3d_properties */
