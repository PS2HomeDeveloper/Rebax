/*
 * ============================================================
 * project_dialog.h
 * ============================================================
 * Small centered popup dialog shown when pressing the "Create" button -
 * contains fields (project name, project path), a Browse button for the
 * path, and Cancel/Create buttons at the bottom.
 *
 * Currently: only the Cancel button works (closes the dialog). The Create
 * button is drawn but has no behavior yet - it will be enabled later when
 * project_create.c is implemented.
 * ============================================================
 */

#ifndef PROJECT_DIALOG_H
#define PROJECT_DIALOG_H

/* Opens the dialog (becomes visible and accepts input) */
void project_dialog_open(void);

/* Is the dialog currently open? - used from main.c to know whether to draw it */
int project_dialog_is_open(void);

/* Updates the dialog state (typing, button presses) - called every frame immediately after window_poll_events, only if the dialog is open */
void project_dialog_update(int window_w, int window_h);

/* Renders the dialog on top of everything else - called every frame after drawing the rest of the UI, only if the dialog is open */
void project_dialog_draw(int window_w, int window_h);

/* Frees all internal resources - called once at program shutdown */
void project_dialog_shutdown(void);

#endif /* PROJECT_DIALOG_H */
