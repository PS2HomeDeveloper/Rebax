/*
 * ============================================================
 * current_scene.h
 * ============================================================
 * State for the currently open scene - the file path on disk (or none if
 * it hasn't been saved yet), and the "has unsaved changes" flag. Built on
 * top of scene_data_serialize/deserialize directly (the same real data,
 * no intermediary copy) - this file adds an "where to save" and "changed"
 * layer on top of the existing read/write logic.
 *
 * Same philosophy as current_project.h exactly, for scenes instead of the project.
 * ============================================================
 */

#ifndef CURRENT_SCENE_H
#define CURRENT_SCENE_H

/* Initializes a new empty scene in memory (clears scene_data completely)
 * - no path, no modifications. Called when opening the development interface
 * for the first time, or when creating a new scene tab with the scene bar ("+") */
void current_scene_new(void);

/* The current file path on disk - NULL if the scene has never been saved
 * (a new in-memory-only scene) */
const char *current_scene_get_path(void);

/* Are there unsaved changes? (adding a node, changing a real property...) */
int current_scene_is_dirty(void);

/* Called from anywhere that actually modifies the scene - raises the
 * "dirty" flag. scene_data and properties_panel call it automatically on any
 * real change (node added, or a property value actually changed) - no other
 * code needs to call it manually */
void current_scene_mark_dirty(void);

/* The display name shown on the scene tab - "empty" if the new scene has no
 * nodes at all, the name of the first added node if a new scene has content but
 * hasn't been saved yet, or the file name (without extension or path) if
 * previously saved. Pointer valid for direct use in rendering (not owned by
 * the caller, do not modify) */
const char *current_scene_get_display_name(void);

/* Saves the current scene to a new explicit path - updates
 * current_scene_get_path() and clears the dirty flag on success. Returns 1 on
 * success, 0 on failure (path not writable, insufficient space...) */
int current_scene_save_as(const char *path);

/* Saves to the current path (current_scene_get_path()) - returns 0
 * immediately if the scene hasn't been named a path yet (use save_as first) */
int current_scene_save(void);

/* Loads a scene from a path - replaces the entire content of scene_data
 * (same internal scene_data_deserialize clearing), updates
 * current_scene_get_path() to that path, and clears dirty on success. Returns
 * 1 on success, 0 on failure (file missing, or incompatible content) */
int current_scene_load(const char *path);

/* Replaces the "where to save" and "has changes" state directly, without
 * touching scene_data itself or the disk at all - used only by the scene tab
 * switching system (scene_tabs.c) when switching between in-memory open scenes
 * (the node content itself is transferred separately via
 * scene_data_serialize/deserialize). path=NULL means "not saved yet" */
void current_scene_restore_state(const char *path, int dirty);

#endif /* CURRENT_SCENE_H */
