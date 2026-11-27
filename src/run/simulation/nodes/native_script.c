/* Native scripts are intentionally not executed by editor simulation. */
#include "run/simulation/sim_world.h"
void simulation_native_script_update(sim_world_t *world, int node_index, float delta_time) {
    (void)world; (void)node_index; (void)delta_time;
}
/* @NODE_SIM type=NODE_TYPE_NATIVE_SCRIPT update=simulation_native_script_update draw=NULL */
