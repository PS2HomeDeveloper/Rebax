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
