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

#ifndef CURRENT_SCENE_H
#define CURRENT_SCENE_H

void current_scene_new(void);

const char *current_scene_get_path(void);

int current_scene_is_dirty(void);

void current_scene_mark_dirty(void);

const char *current_scene_get_display_name(void);

int current_scene_save_as(const char *path);

int current_scene_save(void);

int current_scene_load(const char *path);

void current_scene_restore_state(const char *path, int dirty);

#endif
