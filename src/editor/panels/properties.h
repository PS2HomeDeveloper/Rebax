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

#ifndef PROPERTIES_PANEL_H
#define PROPERTIES_PANEL_H

void properties_panel_set_selected(int node_index);

void properties_panel_clear_selection(void);

void properties_panel_update(int x, int y, int w, int h);

void properties_panel_draw(int x, int y, int w, int h);
void properties_panel_shutdown(void);

#endif
