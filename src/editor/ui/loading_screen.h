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

#ifndef LOADING_SCREEN_H
#define LOADING_SCREEN_H

void loading_screen_show(void);

void loading_screen_show_indefinite(void);

void loading_screen_hide(void);

int loading_screen_is_visible(void);

int loading_screen_just_finished(void);

void loading_screen_update(void);

void loading_screen_draw(int window_w, int window_h);

#endif
