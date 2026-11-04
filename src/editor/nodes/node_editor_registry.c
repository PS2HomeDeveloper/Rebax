/*
 * ============================================================
 * node_editor_registry.c
 * ============================================================
 * No manually written data here at all - the same philosophy as
 * node_registry.c exactly, but for draw functions instead of properties.
 * ============================================================
 */

#include <stddef.h>

#include "node_editor_registry.h"
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
