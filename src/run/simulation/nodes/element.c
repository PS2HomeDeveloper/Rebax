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
#include "run/simulation/sim_world.h"

void simulation_element_update(sim_world_t *world, int node_index,
                               float delta_time) {
    (void)world;
    (void)node_index;
    (void)delta_time;
}
/* @NODE_SIM type=NODE_TYPE_ELEMENT update=simulation_element_update draw=NULL */
