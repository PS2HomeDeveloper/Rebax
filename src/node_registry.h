/*
 * ============================================================
 * node_registry.h
 * ============================================================
 * A "view" copy of the data for each node type, for the engine side (aarch64) -
 * intentionally separate from the real node_interface_t in embedded/ps2/sdk/nodes/
 * (that one is compiled with the PS2 compiler only and never reaches engine code).
 *
 * Used by any UI that needs to display information about a node type: its name,
 * its icon, and its list of properties (for the properties panel).
 *
 * Its data is **generated automatically at build time** (node_registry_generated.h)
 * from the @NODE line in each embedded/ps2/sdk/nodes (.c files) - no manual edits here
 * or in node_registry.c. A new node = a new file in embedded/ps2/sdk/nodes containing
 * a single @NODE line; it appears automatically on the next build.
 * ============================================================
 */

#ifndef NODE_REGISTRY_H
#define NODE_REGISTRY_H

#include "node_types.h"
#include "../embedded/ps2/sdk/nodes/node_interface.h" /* Shared node property types; implementations ship as embedded resources. */

typedef struct {
    node_type_t type;
    const char *name;
    int icon_id; /* One of the ICON_* constants from icon_names.h */
    const node_property_t *properties;
    int property_count;
} node_registry_entry_t;

/* The actual live property value for a specific node instance in the scene - exactly the same
 * format as node_property_t's default_value, but deliberately separate: default_value is read-only
 * (the type schema), while this one actually changes when edited from the properties panel. An array
 * of these (length property_count) is stored for each node in the scene - completely generic, so
 * having more or fewer properties for a node type works with the same code without any changes here */
typedef union {
    float f;
    int i;
    char *s; /* Not const (unlike default_value.s) - fully owned by each node instance,
              * allocated and freed independently */
} node_property_value_t;

/* Returns the display data for a given node type. NULL if the type is not registered */
const node_registry_entry_t *node_registry_get(node_type_t type);

/* Number of all registered node types - for any UI that needs to list them all (e.g. Add Node window) */
int node_registry_count(void);

/* Returns display data by index (0 to count-1) - instead of requiring knowledge of node_type_t beforehand */
const node_registry_entry_t *node_registry_get_by_index(int index);

#endif /* NODE_REGISTRY_H */
