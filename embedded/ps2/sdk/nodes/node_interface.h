/*
 * ============================================================
 * node_interface.h
 * ============================================================
 * This header is compiled only by PS2 toolchains (EE: mips64r5900el-ps2-elf-gcc
 * or g++) - it has nothing to do with the engine runtime code (aarch64).
 * It defines the uniform shape any node must follow at PS2 runtime so that
 * generic engine code can operate on any node type without knowing its internals.
 *
 * Every new node added later (src/nodes/<name>.c) defines a node_interface_t
 * variable filled with its four functions, following the element.c pattern.
 * ============================================================
 */

#ifndef NODE_INTERFACE_H
#define NODE_INTERFACE_H

#include <stddef.h> /* size_t, offsetof - needed by instance_size and property_offsets below */

/* Property value type - determines how the properties panel will display it */
typedef enum {
    NODE_PROPERTY_TYPE_FLOAT,
    NODE_PROPERTY_TYPE_INT,
    NODE_PROPERTY_TYPE_STRING
} node_property_type_t;

/* A single property declared by the node - its name, type, and default value.
 * The properties panel reads this list automatically for any node without
 * knowing its internals - same philosophy as node_interface_t itself */
typedef struct {
    const char *name;
    node_property_type_t type;
    union {
        float f;
        int i;
        const char *s;
    } default_value;

    /* Set to 1 only if the property is NODE_PROPERTY_TYPE_STRING and represents
     * a path to a file inside the project's Assets/ (image, sound...). The
     * properties_panel will then show a "Browse" button (opens asset_browser)
     * instead of a free text field. Default is zero for all properties unless
     * explicitly set - completely safe for current nodes (Element/Element2D/Element3D)
     * with no changes required. */
    int is_asset_path;
} node_property_t;

typedef struct {
    /* Called once when the node is created in the scene */
    void (*init)(void *self);

    /* Called every frame during game runtime - delta_time in seconds since last frame */
    void (*update)(void *self, float delta_time);

    /* Called every frame after update, to draw the node (if it has any visual representation) */
    void (*draw)(void *self);

    /* Called once when the node is removed or the game closes */
    void (*destroy)(void *self);

    /* Node properties list (for the properties panel) - properties may be
     * NULL and property_count zero if the node has no custom properties */
    const node_property_t *properties;
    int property_count;

    /* Size in bytes of the node's real data structure (sizeof its internal
     * type - unknown here; each node defines it in its file). The scene loader
     * at export time uses this to know how many bytes to allocate per instance
     * of this node at real PS2 runtime - without it, the self pointer passed
     * to init/update/draw/destroy would be an untyped pointer with no known size. */
    size_t instance_size;

    /* Byte offsets of each property inside the node's data structure (offsetof) -
     * an array parallel to properties in the exact same order and with the same
     * property_count (NULL when property_count is zero, matching properties).
     * This is the real bridge between "the value the developer set in the editor"
     * and "its actual location in the node's memory" - without it there's no
     * way to apply property edits to any real place at PS2 runtime. */
    const size_t *property_offsets;
} node_interface_t;

#endif /* NODE_INTERFACE_H */
