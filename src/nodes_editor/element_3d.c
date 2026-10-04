/*
 * ============================================================
 * element_3d.c (nodes_editor)
 * ============================================================
 * Visual representation of Element3D in the editor - the exact same idea as element_2d.c, but via viewport_3d_project (3 coordinates, and checking "in front of the camera" before drawing).
 *
 * The order [0]=Position X, [1]=Position Y, [2]=Position Z is fixed here for the same reason as in element_2d.c - this file itself is the source of the property order for Element3D.
 * ============================================================
 */

#include "node_editor_interface.h"
#include "viewport_3d.h"
#include "window.h"

#define MARKER_HALF        3
#define SELECT_CROSS_HALF  6

void element3d_editor_draw(const node_property_value_t *values, int property_count,
                            int cam_x, int cam_y, int cam_w, int cam_h, int selected) {
    if (property_count < 3) return; /* Position X, Y and Z */

    int sx, sy;
    if (!viewport_3d_project(values[0].f, values[1].f, values[2].f,
                              cam_x, cam_y, cam_w, cam_h, &sx, &sy)) {
        return; /* Behind the camera - not drawable */
    }

    window_fill_rect(sx - MARKER_HALF, sy - MARKER_HALF, MARKER_HALF * 2, MARKER_HALF * 2, 225, 225, 225);

    if (selected) {
        window_fill_rect(sx - SELECT_CROSS_HALF, sy - 1, SELECT_CROSS_HALF * 2, 2, 70, 220, 120);
        window_fill_rect(sx - 1, sy - SELECT_CROSS_HALF, 2, SELECT_CROSS_HALF * 2, 70, 220, 120);
    }
}

/* @NODE_EDITOR type=NODE_TYPE_ELEMENT_3D draw_3d=element3d_editor_draw */
