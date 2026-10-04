/*
 * ============================================================
 * node_editor_interface.h
 * ============================================================
 * "The counterpart" to nodes/node_interface.h - that one defines how the
 * node actually runs when the game runs on the PS2 (code compiled with a
 * completely separate PS2 compiler). This defines how the same node type
 * is drawn/represented in the development editor itself (aarch64) - a
 * visual editing representation only, with no relation to the real game
 * logic or performance.
 *
 * Each src/nodes_editor/<name>.c file defines a single draw function (for
 * either a 2D or 3D world, not both), and declares it with an @NODE_EDITOR
 * line - it is discovered automatically at build time
 * (node_editor_registry_generated.h), with no manual listing.
 * ============================================================
 */

#ifndef NODE_EDITOR_INTERFACE_H
#define NODE_EDITOR_INTERFACE_H

#include "node_registry.h" /* node_property_value_t */

/* Draws the representation of a node type in the editor's 2D world.
 * values/property_count: the actual live values for this exact instance,
 *   in the same order as properties[] in the node_registry table for that
 *   type - each file interprets them according to its own ordering (it is
 *   the original source of that ordering).
 * cam_x/y/w/h: the current 2D camera rectangle - passed directly to
 *   viewport_2d_project to convert any world coordinate to an actual
 *   screen position.
 * selected: 1 if this node is currently selected in the scene tree (to
 *   draw an additional selection marker) */
typedef void (*node_editor_draw_2d_fn)(const node_property_value_t *values, int property_count,
                                        int cam_x, int cam_y, int cam_w, int cam_h, int selected);

/* Same idea exactly for the 3D world - via viewport_3d_project */
typedef void (*node_editor_draw_3d_fn)(const node_property_value_t *values, int property_count,
                                        int cam_x, int cam_y, int cam_w, int cam_h, int selected);

#endif /* NODE_EDITOR_INTERFACE_H */
