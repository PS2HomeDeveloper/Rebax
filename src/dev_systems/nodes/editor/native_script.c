/*
 * Rebax Native Script editor node.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */
#include "dev_systems/nodes/editor/node_editor_interface.h"
#include "editor/ui/theme.h"
#include "platform/common/window.h"
void native_script_editor_draw(const node_property_value_t *values, int property_count,
                               int cam_x, int cam_y, int cam_w, int cam_h, int selected) {
    (void)values; (void)property_count; (void)cam_x; (void)cam_y; (void)cam_w; (void)cam_h;
    window_fill_rect(8, 8, selected ? 14 : 10, selected ? 14 : 10, UI_RGB(UI_COLOR_SUCCESS));
}
/* @NODE_EDITOR type=NODE_TYPE_NATIVE_SCRIPT draw_2d=native_script_editor_draw */
