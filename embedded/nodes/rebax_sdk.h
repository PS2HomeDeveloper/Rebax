/*
 * rebax_sdk.h - public Native Rebax SDK for exported PS2 applications.
 *
 * This header is intentionally independent from the editor. Include it from
 * project-owned C/C++ files to create and control native Rebax nodes.
 */
#ifndef REBAX_SDK_H
#define REBAX_SDK_H

#include "node_interface.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    REBAX_NODE_ELEMENT = 0,
    REBAX_NODE_ELEMENT_2D,
    REBAX_NODE_ELEMENT_3D,
    REBAX_NODE_SPRITE_2D
} rebax_node_type_t;

typedef enum {
    REBAX_OK = 0,
    REBAX_ERROR_INVALID_ARGUMENT = -1,
    REBAX_ERROR_NOT_FOUND = -2,
    REBAX_ERROR_WRONG_PROPERTY_TYPE = -3,
    REBAX_ERROR_OUT_OF_MEMORY = -4,
    REBAX_ERROR_UNSUPPORTED_NODE = -5
} rebax_result_t;

typedef struct rebax_node rebax_node_t;

/* Type discovery and creation. Names are the same names shown in the editor. */
const char *rebax_node_type_name(rebax_node_type_t type);
rebax_result_t rebax_node_create(rebax_node_type_t type, rebax_node_t **out_node);
rebax_result_t rebax_node_create_by_name(const char *name, rebax_node_t **out_node);

/* Property discovery. Indices are stable within one node's public property list. */
int rebax_node_property_count(const rebax_node_t *node);
const char *rebax_node_property_name(const rebax_node_t *node, int index);
node_property_type_t rebax_node_property_type(const rebax_node_t *node, int index);
int rebax_node_find_property(const rebax_node_t *node, const char *name);

/* Typed property access by name. Strings are copied into SDK-owned memory. */
rebax_result_t rebax_node_set_float(rebax_node_t *node, const char *name, float value);
rebax_result_t rebax_node_get_float(const rebax_node_t *node, const char *name, float *out_value);
rebax_result_t rebax_node_set_int(rebax_node_t *node, const char *name, int value);
rebax_result_t rebax_node_get_int(const rebax_node_t *node, const char *name, int *out_value);
rebax_result_t rebax_node_set_string(rebax_node_t *node, const char *name, const char *value);
const char *rebax_node_get_string(const rebax_node_t *node, const char *name);

/* Native node lifecycle. update/draw are safe when their callback is absent. */
void rebax_node_update(rebax_node_t *node, float delta_time);
void rebax_node_draw(rebax_node_t *node);
void rebax_node_destroy(rebax_node_t *node);
void rebax_node_free(rebax_node_t *node);

#ifdef __cplusplus
}
#endif

#endif /* REBAX_SDK_H */
