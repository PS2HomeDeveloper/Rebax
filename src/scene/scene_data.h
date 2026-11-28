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

#ifndef SCENE_DATA_H
#define SCENE_DATA_H

#include "node_types.h"
#include "dev_systems/nodes/runtime/node_registry.h"

#define SCENE_DATA_MAX_NODES 128

void scene_data_add_node(node_type_t type, const char *name);
void scene_data_clear(void);
int scene_data_get_node_count(void);
node_type_t scene_data_get_type(int index);
const char *scene_data_get_name(int index);
node_property_value_t *scene_data_get_values(int index);
int scene_data_get_parent(int index);
int scene_data_has_children(int index);
int scene_data_is_expanded(int index);
void scene_data_toggle_expanded(int index);
int scene_data_build_visible(int *out_indices, int *out_depths, int max_items);
int scene_data_get_selected_index(void);
void scene_data_select_index(int index);
void scene_data_clear_selection(void);
void scene_data_add_to_selection(int index);
int scene_data_is_selected(int index);
void scene_data_set_float_property(int index, int property_index, float value);
int scene_data_has_transform(int index);
void scene_data_world_transform(int index, float *x, float *y, float *rotation, float *scale_x, float *scale_y);
void scene_data_set_world_position(int index, float x, float y);
void scene_data_rename(int index, const char *name);
void scene_data_delete(int index);
int scene_data_copy(int index);
int scene_data_cut(int index);
int scene_data_paste(int parent_index);
int scene_data_clipboard_has(void);
int scene_data_duplicate(int index);
void scene_data_get_path(int index, char *out, int out_size);
int scene_data_serialize(char *buffer, int buffer_size);
int scene_data_deserialize(const char *buffer);
void scene_data_shutdown(void);

#endif
