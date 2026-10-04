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

#include "node_registry.h"
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
