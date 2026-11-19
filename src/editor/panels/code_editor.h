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

/* code_editor.h - embedded code editor for C/C++ files and headers. */
#ifndef CODE_EDITOR_H
#define CODE_EDITOR_H

/* Called only when the Script tab is the active tab. */
void code_editor_update(int x, int y, int w, int h);
void code_editor_draw(int x, int y, int w, int h);
void code_editor_set_active(int active);
int code_editor_open_path(const char *path);
void code_editor_shutdown(void);

#endif /* CODE_EDITOR_H */
