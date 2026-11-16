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

/*
 * ============================================================
 * file_system_panel.h
 * ============================================================
 * Displays a real filesystem tree from disk - limited to the project's
 * Assets/ folder only (see current_project.h), with no access to
 * any other location on the user's machine.
 * ============================================================
 */

#ifndef FILE_SYSTEM_PANEL_H
#define FILE_SYSTEM_PANEL_H

void file_system_panel_update(int x, int y, int w, int h);
void file_system_panel_draw(int x, int y, int w, int h);
void file_system_panel_shutdown(void);

void file_system_panel_open_scene(const char *path);

#endif /* FILE_SYSTEM_PANEL_H */
