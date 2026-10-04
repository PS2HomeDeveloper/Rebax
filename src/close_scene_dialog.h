/*
 * ============================================================
 * close_scene_dialog.h
 * ============================================================
 * Small popup dialog (Save / Don't Save / Cancel) shown when attempting to
 * close a scene tab with unsaved changes. Does not touch scene_tabs or
 * current_scene itself - it only returns the user's decision, and the caller
 * (editor_workspace.c) acts on it (opening a save dialog if needed, and
 * finally calling scene_tabs_close).
 * ============================================================
 */

#ifndef CLOSE_SCENE_DIALOG_H
#define CLOSE_SCENE_DIALOG_H

typedef enum {
    CLOSE_SCENE_RESULT_NONE,       /* The user has not chosen anything yet */
    CLOSE_SCENE_RESULT_SAVE,
    CLOSE_SCENE_RESULT_DONT_SAVE,
    CLOSE_SCENE_RESULT_CANCEL
} close_scene_result_t;

/* Opens the dialog for a specific tab (by index) - the caller is responsible
 * for switching the active tab to that index first (scene_tabs_switch_to)
 * before calling this function, so current_scene reflects the same tab's
 * contents if the user chooses "Save" */
void close_scene_dialog_open(int tab_index);

int close_scene_dialog_is_open(void);

/* The index of the tab associated with the currently open dialog - meaningless
 * if close_scene_dialog_is_open() == 0 */
int close_scene_dialog_get_tab_index(void);

void close_scene_dialog_update(int window_w, int window_h);
void close_scene_dialog_draw(int window_w, int window_h);

/* Returns the user's decision if any button of this dialog was pressed (read
 * once then automatically cleared to none), otherwise CLOSE_SCENE_RESULT_NONE.
 * Pressing "Save", "Don't Save" or "Cancel" immediately closes the dialog
 * itself (is_open returns 0 afterwards) regardless of what the caller does
 * with the result */
close_scene_result_t close_scene_dialog_consume_result(void);

void close_scene_dialog_shutdown(void);

#endif /* CLOSE_SCENE_DIALOG_H */
