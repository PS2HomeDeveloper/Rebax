/*
 * ============================================================
 * file_manager.h
 * ============================================================
 * "General file manager" for the engine - a professional popup for selecting a file
 * or folder, used by any other engine code that needs this task (the
 * "Import Image" property on a Sprite node, the "Browse" button when creating a project, etc.)
 * without having to write any file-browsing logic itself - one call and it's done.
 *
 * ------------------------------------------------------------
 * Available root modes for browsing (file_manager_root_t):
 * ------------------------------------------------------------
 *   FILE_MANAGER_ROOT_ASSETS:
 *     Root = the project's Assets/ folder (current_project.h) - shown to the user as "Assets://".
 *     It is impossible to go above this root; the path bar at the top of the window
 *     shows the current path and updates whenever the user enters a new folder,
 *     but it cannot go above this root in any way.
 *     This mode is required for any node property that needs a resource from the
 *     same project (a Sprite image, an audio file, etc.) - the resource must be
 *     inside the project itself to be exportable later with the game.
 *
 *   FILE_MANAGER_ROOT_DEVICE:
 *     Root = the entire device root ("/") - with no restriction. Used for any
 *     case that needs a path outside the project entirely (example: the "Browse"
 *     button when creating a new project, to choose where the project folder itself
 *     will be created on the device).
 *
 * ------------------------------------------------------------
 * Selection modes (file_manager_mode_t):
 * ------------------------------------------------------------
 *   FILE_MANAGER_MODE_PICK_FILE:
 *     The user selects a single file. Pass a list of acceptable extensions
 *     (extensions) to show only matching files (folders are always shown
 *     as well, for navigation) - or pass NULL to show all files with no filtering.
 *     Clicking a file selects it (highlight), and the "Select" button at the
 *     bottom of the window confirms the current selection only.
 *
 *   FILE_MANAGER_MODE_PICK_FOLDER:
 *     The user selects a folder (no selectable files are shown - files are shown
 *     faded for context, and they cannot be navigated). The user navigates
 *     folders by clicking them, and the "Select" button chooses the currently
 *     open folder exactly (same behavior as standard folder selection dialogs).
 *
 * ------------------------------------------------------------
 * Full usage example (the "Import Image" property on a future Sprite node):
 * ------------------------------------------------------------
 *   static void on_image_picked(const char *path, void *user_data) {
 *       if (path == NULL) return; // the user canceled the selection
 *       my_sprite_set_texture_path((my_sprite_t *)user_data, path);
 *   }
 *   ...
 *   static const char *img_exts[] = { "png", "jpg", "jpeg", "bmp" };
 *   file_manager_open(FILE_MANAGER_ROOT_ASSETS, FILE_MANAGER_MODE_PICK_FILE,
 *                       img_exts, 4, on_image_picked, my_sprite_ptr);
 * ============================================================
 */

#ifndef FILE_MANAGER_H
#define FILE_MANAGER_H

typedef enum {
    FILE_MANAGER_ROOT_ASSETS, /* Confined to the open project's Assets:// */
    FILE_MANAGER_ROOT_DEVICE  /* Entire device root ("/"), no restriction */
} file_manager_root_t;

typedef enum {
    FILE_MANAGER_MODE_PICK_FILE,   /* Pick a single file (optionally limited by extensions) */
    FILE_MANAGER_MODE_PICK_FOLDER, /* Pick a folder only - files shown for context but not selectable */
    FILE_MANAGER_MODE_SAVE_FILE    /* Same as PICK_FOLDER browsing + an editable filename field at the bottom -
                                      * opened via file_manager_open_save (not file_manager_open) */
} file_manager_mode_t;

/* Called exactly once when the window closes:
 *   picked_path: the full selected path when "Select" is pressed, or NULL
 *                if the operation was canceled (Cancel button or the X close) -
 *                you must always check for NULL before using the path.
 *   user_data:   the same user pointer you passed to file_manager_open
 *                unchanged - use it to know "which exact item" this call
 *                was for if you have more than one button opening
 *                the browser for different purposes */
typedef void (*file_manager_callback_t)(const char *picked_path, void *user_data);

/* Opens a fully featured file browser window (path bar, search, icon grid
 * with real image thumbnails for pictures, Cancel/Select buttons, and an X close button).
 *
 *   root:             browsing root (see explanation above)
 *   mode:              pick file or folder
 *   extensions:        array of extensions without the dot (e.g. "png", "jpg") -
 *                      compared case-insensitively. Pass NULL
 *                      (and extension_count = 0) to show all files without
 *                      filtering, or when using PICK_FOLDER (ignored).
 *   extension_count:   length of the extensions array
 *   callback:          response callback - practically mandatory (without it
 *                      opening the window is pointless)
 *   user_data:         void pointer passed through to callback, or NULL
 *
 * Note: if root=ASSETS and there is no project currently open
 * (current_project_is_open() == 0), callback is called immediately with NULL
 * and the window is never opened - there is no Assets:// without an open project */
void file_manager_open(file_manager_root_t root, file_manager_mode_t mode,
                         const char **extensions, int extension_count,
                         file_manager_callback_t callback, void *user_data);

/* "Save As" window - browse folders (identical behavior to PICK_FOLDER exactly:
 * files are shown dimmed for context and cannot be selected) + an editable
 * filename field at the bottom of the window, initialized with default_filename
 * (including its extension, e.g. "Player.rscene") - the user may edit it
 * freely before confirming. The window title is "Save As", and the confirm
 * button text is "Save" instead of "Select". If the user presses "Save":
 * callback is called with the full path = the current browsed folder + "/" +
 * the filename field text exactly as entered (no automatic extension fixing
 * or correction - the user is responsible for typing it correctly; the
 * default name already provides the extension). If "Cancel": it is called
 * with NULL, following the same logic as file_manager_open */
void file_manager_open_save(file_manager_root_t root, const char *default_filename,
                              file_manager_callback_t callback, void *user_data);

/* Is the window currently open? - used by the calling code (or from
 * main.c if you want to draw it above everything else at a higher layer)
 * to know whether this frame should update and draw it */
int file_manager_is_open(void);

/* Updates the window state (navigation, search, scrolling, button presses) - called every
 * frame immediately after window_poll_events, only if the window is open.
 * Automatically calls the registered callback on final selection or cancellation,
 * then closes the window itself - no external code needs to check the result
 * or close it manually */
void file_manager_update(int window_w, int window_h);

/* Draws the window above everything else - called every frame after drawing the rest
 * of the UI, only if the window is open */
void file_manager_draw(int window_w, int window_h);

/* Frees all internal resources (thumbnails, fonts, buttons) - called once on program shutdown */
void file_manager_shutdown(void);

#endif /* FILE_MANAGER_H */
