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

#ifndef ADD_NODE_DIALOG_H
#define ADD_NODE_DIALOG_H

void add_node_dialog_open(void);
int  add_node_dialog_is_open(void);
void add_node_dialog_update(int window_w, int window_h);
void add_node_dialog_draw(int window_w, int window_h);
void add_node_dialog_shutdown(void);

#endif
