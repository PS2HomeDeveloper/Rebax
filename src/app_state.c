/*
 * ============================================================
 * app_state.c
 * ============================================================
 */

#include "app_state.h"

static app_screen_t g_current_screen = APP_SCREEN_PROJECT_CENTER;

void app_state_set_screen(app_screen_t screen) {
    g_current_screen = screen;
}

app_screen_t app_state_get_screen(void) {
    return g_current_screen;
}
