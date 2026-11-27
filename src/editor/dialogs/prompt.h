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

#ifndef PROMPT_DIALOG_H
#define PROMPT_DIALOG_H

typedef void (*prompt_callback_t)(const char *text, void *user_data);

void prompt_input_open(const char *title, const char *initial, const char *ok_label,
                       prompt_callback_t callback, void *user_data);
void prompt_confirm_open(const char *title, const char *message, const char *ok_label, int danger,
                         prompt_callback_t callback, void *user_data);
int prompt_is_open(void);
void prompt_update(int window_w, int window_h);
void prompt_draw(int window_w, int window_h);
void prompt_shutdown(void);

#endif
