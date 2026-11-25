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
 * License, v. 2.0. If a copy of the MPL was not distributed with
 * this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */
#ifndef SIM_WORLD_H
#define SIM_WORLD_H

#include "scene/scene_data.h"

typedef struct {
    float x;
    float y;
    float rotation;
    float scale_x;
    float scale_y;
} sim_transform_t;

typedef struct sim_world sim_world_t;
typedef void (*sim_world_frame_hook_t)(sim_world_t *world, float delta_time,
                                       void *user_data);

sim_world_t *sim_world_create(void);
void sim_world_destroy(sim_world_t *world);
void sim_world_set_limits(sim_world_t *world, int texture_memory_mb,
                          int max_texture_size);
int sim_world_build_from_scene(sim_world_t *world);
void sim_world_update(sim_world_t *world, float delta_time);
void sim_world_draw(sim_world_t *world);
void sim_world_draw_sprite(sim_world_t *world, int node_index);
int sim_world_register_frame_hook(sim_world_t *world,
                                  sim_world_frame_hook_t callback,
                                  void *user_data);
void sim_world_unregister_frame_hook(sim_world_t *world,
                                     sim_world_frame_hook_t callback,
                                     void *user_data);

int sim_world_get_node_count(const sim_world_t *world);
int sim_world_get_transform(const sim_world_t *world, int index,
                            sim_transform_t *out);
int sim_world_get_property(const sim_world_t *world, int index,
                           const char *name, node_property_value_t *out);
int sim_world_set_property(sim_world_t *world, int index, const char *name,
                           node_property_value_t value);
const char *sim_world_warning(const sim_world_t *world);
int sim_world_warning_count(const sim_world_t *world);
const char *sim_world_warning_at(const sim_world_t *world, int index);

#endif
