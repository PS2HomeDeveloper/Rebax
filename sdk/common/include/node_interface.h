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

#ifndef NODE_INTERFACE_H
#define NODE_INTERFACE_H

#include <stddef.h>

typedef enum {
    NODE_PROPERTY_TYPE_FLOAT,
    NODE_PROPERTY_TYPE_INT,
    NODE_PROPERTY_TYPE_STRING
} node_property_type_t;

typedef struct {
    const char *name;
    node_property_type_t type;
    union {
        float f;
        int i;
        const char *s;
    } default_value;

    int is_asset_path;
} node_property_t;

typedef struct {
    void (*init)(void *self);

    void (*update)(void *self, float delta_time);

    void (*draw)(void *self);

    void (*destroy)(void *self);

    const node_property_t *properties;
    int property_count;

    size_t instance_size;

    const size_t *property_offsets;
} node_interface_t;

#endif
