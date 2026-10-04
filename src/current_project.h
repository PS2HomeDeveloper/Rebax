/*
 * ============================================================
 * current_project.h
 * ============================================================
 * Stores the path of the currently open project in program memory (RAM) -
 * not a disk file, just runtime state. Filled once after successful project
 * creation (project_create), and any panel that needs to know "where is the
 * open project?" (the filesystem, later save/load scenes) asks here.
 * ============================================================
 */

#ifndef CURRENT_PROJECT_H
#define CURRENT_PROJECT_H

/* Sets the root path of the currently open project (the full path to the
 * project folder itself, not the Assets subfolder) */
void current_project_set_path(const char *root_path);

/* Returns the path of the currently open project, or NULL if no project is open */
const char *current_project_get_path(void);

/* Returns 1 if a project is currently open, 0 otherwise */
int current_project_is_open(void);

#endif /* CURRENT_PROJECT_H */
