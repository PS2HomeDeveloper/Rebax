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

#include <stddef.h>

#include "dev_systems/nodes/editor/node_editor_registry.h"
#include "node_editor_registry_generated.h"

#define REGISTRY_COUNT NODE_EDITOR_REGISTRY_GENERATED_COUNT

const node_editor_registry_entry_t *node_editor_registry_get(node_type_t type) {
    for (int i = 0; i < REGISTRY_COUNT; i++) {
        if (g_node_editor_registry_table[i].type == type) {
            return &g_node_editor_registry_table[i];
        }
    }
    return NULL;
}
