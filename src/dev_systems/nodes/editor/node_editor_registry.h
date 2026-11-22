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

/*
 * ============================================================
 * node_editor_registry.h
 * ============================================================
 * Links each node type to its visual draw function in the editor (if
 * present) - its data is **automatically generated at build time**
 * (node_editor_registry_generated.h) from the @NODE_EDITOR line in each
 * src/nodes_editor (.c) file, following the exact same philosophy as
 * node_registry.h. Do not edit it manually nor node_editor_registry.c.
 * ============================================================
 */

#ifndef NODE_EDITOR_REGISTRY_H
#define NODE_EDITOR_REGISTRY_H

#include "node_types.h"
#include "dev_systems/nodes/editor/node_editor_interface.h"

typedef struct {
    node_type_t type;
    node_editor_draw_2d_fn draw_2d;
    node_editor_draw_3d_fn draw_3d;
} node_editor_registry_entry_t;

const node_editor_registry_entry_t *node_editor_registry_get(node_type_t type);

#endif
