/*
 * ============================================================
 * scene_tabs.h
 * ============================================================
 * Manages multiple scenes open in memory simultaneously (scene tabs) -
 * the currently "active" tab is the one whose state is live in scene_data
 * and current_scene (same as before this module). Any other inactive
 * tab has its state stored in an in-memory cache (via
 * scene_data_serialize) - no actual disk writes during tab switches,
 * only on explicit save.
 *
 * At least one tab is always kept open - closing the last tab immediately
 * creates an empty ("empty") tab instead.
 * ============================================================
 */

#ifndef SCENE_TABS_H
#define SCENE_TABS_H

#define SCENE_TABS_MAX 16

/* Called once at editor startup - creates the first empty tab
 * ("empty") and activates it */
void scene_tabs_init(void);

/* Number of tabs currently open (always ≥1) */
int scene_tabs_count(void);

/* Index of the currently active tab (actually shown in scene_data/
 * properties_panel/viewport) */
int scene_tabs_active_index(void);

/* The display name for a given tab (need not be active) - exactly the
 * same format as current_scene_get_display_name ("empty", the first node's name,
 * or the saved file name) */
const char *scene_tabs_get_display_name(int index);

/* 1 if there are unsaved changes in this specific tab */
int scene_tabs_get_dirty(int index);

/* Creates a new empty ("empty") scene tab and activates it immediately -
 * rejects (returns 0, no effect) if the current active tab is already empty
 * and unmodified (no need for two empty tabs), or if SCENE_TABS_MAX is reached.
 * Returns 1 if actually created */
int scene_tabs_new(void);

/* Switches the active tab to a given index - stores the current tab's
 * state to memory first (serialize), then loads the new tab's state. No-op
 * if index is out of range or the same as the currently active tab */
void scene_tabs_switch_to(int index);

/* Permanently closes a given tab immediately - no save prompt here (that
 * is the UI's responsibility: it must show a save confirmation *before*
 * calling this function, and call current_scene_save() itself if the user
 * chose "Save"). If it was the last remaining tab, a new empty tab is
 * created instead immediately (at least one tab remains open at all times) */
void scene_tabs_close(int index);

void scene_tabs_shutdown(void);

#endif /* SCENE_TABS_H */
