/*
 * ============================================================
 * file_system_panel.h
 * ============================================================
 * Displays a real filesystem tree from disk - limited to the project's
 * Assets/ folder only (see current_project.h), with no access to
 * any other location on the user's machine.
 * ============================================================
 */

#ifndef FILE_SYSTEM_PANEL_H
#define FILE_SYSTEM_PANEL_H

void file_system_panel_update(int x, int y, int w, int h);
void file_system_panel_draw(int x, int y, int w, int h);
void file_system_panel_shutdown(void);

#endif /* FILE_SYSTEM_PANEL_H */
