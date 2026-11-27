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

#ifndef CONTEXT_MENU_H
#define CONTEXT_MENU_H

#define CONTEXT_MENU_MAX_ITEMS 12

typedef struct {
    const char *label;
    int id;
    int enabled;
    int separator_before;
} context_menu_item_t;

typedef void (*context_menu_callback_t)(int id, void *user_data);

void context_menu_open(int x, int y, const context_menu_item_t *items, int count,
                       context_menu_callback_t callback, void *user_data);
int context_menu_is_open(void);
void context_menu_update(int window_w, int window_h);
void context_menu_draw(int window_w, int window_h);
void context_menu_shutdown(void);

#endif
