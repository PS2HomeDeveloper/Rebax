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
 * node_registry.c
 * ============================================================
 * No manually written data here at all - everything comes ready from
 * node_registry_generated.h (generated at build time from the @NODE line in each
 * src/nodes file (.c files)). Adding a node = a new .c file in the
 * src/nodes/ folder with a single @NODE line - it appears here automatically on the next build,
 * with no changes to this file.
 * ============================================================
 */

#include <stddef.h>

#include "dev_systems/nodes/runtime/node_registry.h"
#include "icon_names.h"
#include "node_registry_generated.h"

#define REGISTRY_COUNT NODE_REGISTRY_GENERATED_COUNT

const node_registry_entry_t *node_registry_get(node_type_t type) {
    for (int i = 0; i < REGISTRY_COUNT; i++) {
        if (g_node_registry_table[i].type == type) {
            return &g_node_registry_table[i];
        }
    }
    return NULL;
}

int node_registry_count(void) {
    return REGISTRY_COUNT;
}

const node_registry_entry_t *node_registry_get_by_index(int index) {
    if (index < 0 || index >= REGISTRY_COUNT) {
        return NULL;
    }
    return &g_node_registry_table[index];
}
