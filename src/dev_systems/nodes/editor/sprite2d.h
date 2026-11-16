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

#ifndef REBAX_SPRITE2D_EDITOR_H
#define REBAX_SPRITE2D_EDITOR_H
#include "dev_systems/nodes/runtime/node_registry.h"
int sprite2d_editor_hit_test(const node_property_value_t *values, int property_count,
                              int screen_x, int screen_y, int pivot_x, int pivot_y,
                              float viewport_zoom);
#endif
