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

#ifndef SCENE_TABS_H
#define SCENE_TABS_H

#define SCENE_TABS_MAX 16

void scene_tabs_init(void);

int scene_tabs_count(void);

int scene_tabs_active_index(void);

const char *scene_tabs_get_display_name(int index);

int scene_tabs_get_dirty(int index);

int scene_tabs_new(void);

void scene_tabs_switch_to(int index);

void scene_tabs_close(int index);

void scene_tabs_shutdown(void);

int scene_tabs_save_all(void);

#endif
