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

#ifndef CLOSE_SCENE_DIALOG_H
#define CLOSE_SCENE_DIALOG_H

typedef enum {
    CLOSE_SCENE_RESULT_NONE,
    CLOSE_SCENE_RESULT_SAVE,
    CLOSE_SCENE_RESULT_DONT_SAVE,
    CLOSE_SCENE_RESULT_CANCEL
} close_scene_result_t;

void close_scene_dialog_open(int tab_index);

int close_scene_dialog_is_open(void);

int close_scene_dialog_get_tab_index(void);

void close_scene_dialog_update(int window_w, int window_h);
void close_scene_dialog_draw(int window_w, int window_h);

close_scene_result_t close_scene_dialog_consume_result(void);

void close_scene_dialog_shutdown(void);

#endif
