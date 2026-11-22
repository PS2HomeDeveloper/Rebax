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

/*
 * ============================================================
 * node_editor_interface.h
 * ============================================================
 * "The counterpart" to nodes/node_interface.h - that one defines how the
 * node actually runs inside the exported game (code compiled by the target
 * platform's own toolchain). This defines how the same node type
 * is drawn/represented in the development editor itself - a
 * visual editing representation only, with no relation to the real game
 * logic or performance.
 *
 * Each src/editor/nodes/<name>.c file defines a single draw function (for
 * either a 2D or 3D world, not both), and declares it with an @NODE_EDITOR
 * line - it is discovered automatically at build time
 * (node_editor_registry_generated.h), with no manual listing.
 * ============================================================
 */

#ifndef NODE_EDITOR_INTERFACE_H
#define NODE_EDITOR_INTERFACE_H

#include "dev_systems/nodes/runtime/node_registry.h"

typedef void (*node_editor_draw_2d_fn)(const node_property_value_t *values, int property_count,
                                        int cam_x, int cam_y, int cam_w, int cam_h, int selected);

typedef void (*node_editor_draw_3d_fn)(const node_property_value_t *values, int property_count,
                                        int cam_x, int cam_y, int cam_w, int cam_h, int selected);

#endif
