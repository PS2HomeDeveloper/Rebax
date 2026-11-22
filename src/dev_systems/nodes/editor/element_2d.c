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

#include "editor/ui/theme.h"
#include "dev_systems/nodes/editor/node_editor_interface.h"
#include "editor/viewport/2d.h"
#include "platform/common/window.h"

#define MARKER_HALF        3
#define SELECT_CROSS_HALF  6

void element2d_editor_draw(const node_property_value_t *values, int property_count,
                            int cam_x, int cam_y, int cam_w, int cam_h, int selected) {
    if (property_count < 2) return;

    int sx, sy;
    viewport_2d_project(values[0].f, values[1].f, cam_x, cam_y, cam_w, cam_h, &sx, &sy);

    window_fill_rect(sx - MARKER_HALF, sy - MARKER_HALF, MARKER_HALF * 2, MARKER_HALF * 2, UI_RGB(UI_COLOR_TEXT_PRIMARY));

    if (selected) {
        window_fill_rect(sx - SELECT_CROSS_HALF, sy - 1, SELECT_CROSS_HALF * 2, 2, UI_RGB(UI_COLOR_SUCCESS));
        window_fill_rect(sx - 1, sy - SELECT_CROSS_HALF, 2, SELECT_CROSS_HALF * 2, UI_RGB(UI_COLOR_SUCCESS));
    }
}

/* @NODE_EDITOR type=NODE_TYPE_ELEMENT_2D draw_2d=element2d_editor_draw */
