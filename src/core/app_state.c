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
 * app_state.c
 * ============================================================
 */

#include "core/app_state.h"

static app_screen_t g_current_screen = APP_SCREEN_PROJECT_CENTER;

void app_state_set_screen(app_screen_t screen) {
    g_current_screen = screen;
}

app_screen_t app_state_get_screen(void) {
    return g_current_screen;
}
