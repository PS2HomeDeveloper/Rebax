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
#ifndef SIM_PROFILE_H
#define SIM_PROFILE_H
#include "project/project_types.h"
typedef struct {
    char id[24];
    char label[64];
    int width, height;
    int alternate_width, alternate_height;
    int frame_rate;
    int nearest_filter;
    int integer_scale;
    int memory_mb, texture_memory_mb, max_texture_size;
    int safe_area;
} sim_profile_t;
void sim_profile_defaults(sim_profile_t *profile, project_type_t type);
int sim_profile_load(sim_profile_t *profile, project_type_t type);
#endif
