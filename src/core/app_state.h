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

#ifndef APP_STATE_H
#define APP_STATE_H

typedef enum {
    APP_SCREEN_PROJECT_CENTER,
    APP_SCREEN_EDITOR,
    APP_SCREEN_SETTINGS
} app_screen_t;

void app_state_set_screen(app_screen_t screen);

app_screen_t app_state_get_screen(void);

void app_state_open_settings(app_screen_t return_to);

void app_state_close_settings(void);

#endif
