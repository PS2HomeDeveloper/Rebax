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
 * Licensed under the MIT License.
 * See the LICENSE file for details.
 */

#include <stdlib.h>
#include <string.h>
#include "scene_runtime.h"

void *node_instantiate(const node_interface_t *iface, const node_property_value_t *values) {
    if (iface == NULL || iface->instance_size == 0 ||
        (iface->property_count > 0 && (iface->properties == NULL || iface->property_offsets == NULL || values == NULL))) return NULL;
    void *instance = calloc(1, iface->instance_size);
    if (instance == NULL) return NULL;
    for (int i = 0; i < iface->property_count; i++) {
        size_t offset = iface->property_offsets[i];
        node_property_type_t type = iface->properties[i].type;
        if (type == NODE_PROPERTY_TYPE_FLOAT) {
            if (offset > iface->instance_size || sizeof(float) > iface->instance_size - offset) goto fail;
            *(float *)((char *)instance + offset) = values[i].f;
        } else if (type == NODE_PROPERTY_TYPE_INT) {
            if (offset > iface->instance_size || sizeof(int) > iface->instance_size - offset) goto fail;
            *(int *)((char *)instance + offset) = values[i].i;
        } else if (type == NODE_PROPERTY_TYPE_STRING) {
            if (offset > iface->instance_size || sizeof(const char *) > iface->instance_size - offset) goto fail;
            *(const char **)((char *)instance + offset) = values[i].s;
        } else goto fail;
    }
    if (iface->init != NULL) iface->init(instance);
    return instance;
fail:
    free(instance);
    return NULL;
}
