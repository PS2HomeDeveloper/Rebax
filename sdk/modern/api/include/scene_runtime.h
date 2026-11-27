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

#ifndef SCENE_RUNTIME_H
#define SCENE_RUNTIME_H

#include "node_interface.h"

typedef union {
    float f;
    int i;
    const char *s;
} node_property_value_t;

typedef struct {
    const char *name;
    const node_interface_t *iface;
    const node_property_value_t *values;
} scene_node_entry_t;

typedef struct {
    const char *name;
    const scene_node_entry_t *nodes;
    int node_count;
} scene_table_entry_t;

extern const scene_table_entry_t g_scenes[];
extern const int g_scene_count;

void *node_instantiate(const node_interface_t *iface, const node_property_value_t *values);

#endif
