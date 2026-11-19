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

#ifndef SCENE_TREE_PANEL_H
#define SCENE_TREE_PANEL_H

/* This panel is responsible only for displaying the scene tree. Scene data lives in scene_data.h. */
void scene_tree_panel_update(int x, int y, int w, int h);
void scene_tree_panel_draw(int x, int y, int w, int h);
void scene_tree_panel_shutdown(void);

#endif /* SCENE_TREE_PANEL_H */
