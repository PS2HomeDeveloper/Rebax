/*
 * ============================================================
 * element_2d.c (nodes_editor)
 * ============================================================
 * Visual representation of Element2D in the editor - a small dot at its actual position (Position X/Y), and an additional selection marker (green plus) when it's currently selected in the scene tree.
 *
 * The ordering [0]=Position X, [1]=Position Y is fixed here because this file is itself the place that interprets the property order of Element2D - it does not relate to any future changes to other nodes' properties.
 * ============================================================
 */

#include "node_editor_interface.h"
#include "viewport_2d.h"
#include "window.h"

#define MARKER_HALF        3  /* Half the width of the small square that represents the node's position */
#define SELECT_CROSS_HALF  6  /* Half the arm length of the selection marker (the plus) */

void element2d_editor_draw(const node_property_value_t *values, int property_count,
                            int cam_x, int cam_y, int cam_w, int cam_h, int selected) {
    if (property_count < 2) return; /* Minimum: Position X and Y */

    int sx, sy;
    viewport_2d_project(values[0].f, values[1].f, cam_x, cam_y, cam_w, cam_h, &sx, &sy);

    /* A subtle white dot that always indicates the node's position - regardless of selection */
    window_fill_rect(sx - MARKER_HALF, sy - MARKER_HALF, MARKER_HALF * 2, MARKER_HALF * 2, 225, 225, 225);

    if (selected) {
        window_fill_rect(sx - SELECT_CROSS_HALF, sy - 1, SELECT_CROSS_HALF * 2, 2, 70, 220, 120);
        window_fill_rect(sx - 1, sy - SELECT_CROSS_HALF, 2, SELECT_CROSS_HALF * 2, 70, 220, 120);
    }
}

/* @NODE_EDITOR type=NODE_TYPE_ELEMENT_2D draw_2d=element2d_editor_draw */
