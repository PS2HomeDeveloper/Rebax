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
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef RUN_CONTROLS_H
#define RUN_CONTROLS_H

#define RUN_CONTROLS_HEIGHT 38

void run_controls_update(int window_w);
int run_controls_menu_is_open(void);
void run_controls_draw(int window_w);
void run_controls_draw_menu(int window_w);
void run_controls_shutdown(void);

#endif
