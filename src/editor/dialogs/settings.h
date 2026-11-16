/*
 *  ____  _____ ____    _    __  __
 * |  _ \| ____| __ )  / \   \ \/ /
 * | |_) | _| | _ \ / _ \   \  /
 * |  _ <| |___| |_) / ___ \  /  \
 * |_| \_\_____|____/_/   \_\/_/\_\
 *
 * Rebax Engine
 * Copyright (c) 2026 PS2HomeDeveloper
 *
 * Licensed under the MIT License.
 * See the LICENSE file for details.
 */
#ifndef PROJECT_SETTINGS_DIALOG_H
#define PROJECT_SETTINGS_DIALOG_H

void project_settings_dialog_open(void);
int project_settings_dialog_is_open(void);
void project_settings_dialog_update(int window_w, int window_h);
void project_settings_dialog_draw(int window_w, int window_h);
void project_settings_dialog_shutdown(void);

#endif
