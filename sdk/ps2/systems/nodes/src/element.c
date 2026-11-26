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

#include "node_interface.h"

typedef struct {
    char _unused;
} element_instance_t;

static void element_init(void *self) {
    (void)self;
}

static void element_update(void *self, float delta_time) {
    (void)self;
    (void)delta_time;
}

static void element_draw(void *self) {
    (void)self;
}

static void element_destroy(void *self) {
    (void)self;
}

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
