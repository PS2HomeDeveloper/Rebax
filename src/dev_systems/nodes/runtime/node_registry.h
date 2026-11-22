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
 * node_registry.h
 * ============================================================
 * A "view" copy of the data for each node type, for the editor/engine side -
 * intentionally separate from the real node_interface_t in sdk/common/include/
 * (that one is compiled by the target toolchains only and never reaches editor code).
 *
 * Used by any UI that needs to display information about a node type: its name,
 * its icon, and its list of properties (for the properties panel).
 *
 * Its data is **generated automatically at build time** (node_registry_generated.h)
 * from the @NODE line in each sdk/<target>/nodes/src (.c files) - no manual edits here
 * or in node_registry.c. A new node = a new file in sdk/<target>/nodes/src containing
 * a single @NODE line; it appears automatically on the next build.
 * ============================================================
 */

#ifndef NODE_REGISTRY_H
#define NODE_REGISTRY_H

#include "node_types.h"
#include "node_interface.h"

typedef struct {
    node_type_t type;
    const char *name;
    int icon_id;
    const node_property_t *properties;
    int property_count;
} node_registry_entry_t;

typedef union {
    float f;
    int i;
    char *s;
} node_property_value_t;

const node_registry_entry_t *node_registry_get(node_type_t type);

int node_registry_count(void);

const node_registry_entry_t *node_registry_get_by_index(int index);

#endif
