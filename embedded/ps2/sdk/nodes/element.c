/*
 * ============================================================
 * element.c
 * ============================================================
 * Implementation of the Element node - the abstract base node, parent
 * for all engine nodes. Its role is purely organizational (grouping
 * other nodes under it in the tree), with no spatial meaning and no
 * runtime behavior - its four functions are intentionally empty.
 * ============================================================
 */

#include "node_interface.h"

/* No actual data (purely organizational node) - a single dummy field so
 * the struct isn't completely empty (not allowed in plain C) */
typedef struct {
    char _unused;
} element_instance_t;

static void element_init(void *self) {
    (void)self;
    /* Pure organizational node - requires no initialization */
}

static void element_update(void *self, float delta_time) {
    (void)self;
    (void)delta_time;
    /* No runtime behavior */
}

static void element_draw(void *self) {
    (void)self;
    /* No visual representation */
}

static void element_destroy(void *self) {
    (void)self;
    /* No resources to free */
}

/* Unified interface for the Element node - any general engine code uses
 * this variable to handle Element nodes without knowing their details.
 * No specific properties (properties = NULL) - purely organizational node */
const node_interface_t element_interface = {
    element_init,
    element_update,
    element_draw,
    element_destroy,
    NULL,
    0,
    sizeof(element_instance_t),
    NULL
};

/* @NODE type=NODE_TYPE_ELEMENT name=Element icon=Folder properties=NULL */
/* The above declaration is the sole source that defines Element to the
 * development editor - the build file reads it automatically on each
 * build and generates the editor view itself, with no manual edits
 * elsewhere. It must remain the last line in the file (after defining
 * the properties and interface) so it can read them before linking them */
