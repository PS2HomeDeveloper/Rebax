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

#ifndef RUN_CONTROLS_H
#define RUN_CONTROLS_H

#define RUN_CONTROLS_HEIGHT 38

void run_controls_update(int window_w);
int run_controls_menu_is_open(void);
void run_controls_draw(int window_w);
void run_controls_draw_menu(int window_w);
void run_controls_shutdown(void);

#endif
