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
 * Licensed under the MIT License.
 * See the LICENSE file for details.
 */

#ifndef SCENE_TREE_PANEL_H
#define SCENE_TREE_PANEL_H

/* This panel is responsible only for displaying the scene tree. Scene data lives in scene_data.h. */
void scene_tree_panel_update(int x, int y, int w, int h);
void scene_tree_panel_draw(int x, int y, int w, int h);
void scene_tree_panel_shutdown(void);

#endif /* SCENE_TREE_PANEL_H */
