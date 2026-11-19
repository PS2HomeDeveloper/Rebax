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

/*
 * ============================================================
 * app_state.h
 * ============================================================
 * Defines which "main screen" of the app is currently shown. Small standalone
 * library so any file that needs to request a screen change (for example
 * project_dialog after successful creation) doesn't need to know details of
 * that screen (editor_workspace, etc) - it simply requests a switch by name.
 * ============================================================
 */

#ifndef APP_STATE_H
#define APP_STATE_H

typedef enum {
    APP_SCREEN_PROJECT_CENTER, /* Projects list screen (start) */
    APP_SCREEN_EDITOR          /* Interface for developing the currently open project */
} app_screen_t;

/* Switches the current screen */
void app_state_set_screen(app_screen_t screen);

/* Returns the current screen */
app_screen_t app_state_get_screen(void);

#endif /* APP_STATE_H */
