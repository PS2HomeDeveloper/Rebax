#include "run/simulation/sim_world.h"

void simulation_sprite2d_update(sim_world_t *world, int node_index,
                                float delta_time) {
    (void)world;
    (void)node_index;
    (void)delta_time;
}

void simulation_sprite2d_draw(sim_world_t *world, int node_index) {
    sim_world_draw_sprite(world, node_index);
}

/* @NODE_SIM type=NODE_TYPE_SPRITE_2D update=simulation_sprite2d_update draw=simulation_sprite2d_draw */
