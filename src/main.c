/*
 * ============================================================
 * Rebax_Engine - main.c
 * ============================================================
 * This file is only the orchestrator.
 * It contains no real logic itself - its sole purpose is to:
 *   1. include library headers (each library = separate .c + .h files)
 *   2. call each library in the correct place (init / render loop / export)
 *
 * Project rule: comments in Arabic only, any printed output in English
 * only and only when necessary.
 * ============================================================
 */

#include <stdio.h>

#include "window.h"
#include "ui_project_center.h"
#include "project_dialog.h"
#include "loading_screen.h"
#include "editor_workspace.h"
#include "app_state.h"
#include "rebax_paths.h" /* Rebax working directory + first-run setup for tools and node resources */
#include "file_manager.h" /* Built-in file manager - opened from anywhere in the engine, drawn on top of everything */
#include "ui_dialog.h"
#include "android_storage.h"

/* ------------------------------------------------------------
 * In the future: additional library includes will be added here, e.g.:
 * #include "export.h"
 * #include "project_manager.h"
 * ------------------------------------------------------------ */

int main(void) {
    /* Previously smaller size to fit mobile screens inside Termux:X11 -
     * window will be resizable later anyway */
    if (!window_init("Rebax_Engine", 900, 560)) {
        return 1;
    }

    printf("Rebax_Engine started successfully.\n");

    android_storage_request_if_needed();

    /* First run - extract the PS2Dev environment and node resources to the Rebax folder
     * (see rebax_paths.h). Loading screen without auto-hide until the real operation
     * completes - it may take significant time (ps2dev archive is large), we won't
     * freeze the UI (not completely blocking) but there's nothing to show besides this
     * screen until it's done */
    int rebax_setup_needed = rebax_paths_is_setup_needed();
    if (rebax_setup_needed) {
        loading_screen_show_indefinite();
        rebax_paths_setup_start();
    }

    while (!window_should_close()) {
        window_poll_events();

        if (rebax_setup_needed && !rebax_paths_setup_done()) {
            loading_screen_update();
            /* Present the window first; extracting the ps2dev archive may take long,
             * and if it starts before window_present() the window appears empty. */
            loading_screen_draw(window_get_width(), window_get_height());
            window_present();
            rebax_paths_setup_update();
            if (rebax_paths_setup_done()) {
                if (rebax_paths_setup_failed()) {
                    fprintf(stderr, "[rebax] WARNING: first-run setup failed - "
                                    "export won't work until this is resolved.\n");
                }
                loading_screen_hide();
            }
            continue;
        }

        loading_screen_update();

        /* When the loading screen auto-hides, switch to the development UI -
         * this connection is temporary (every load currently ends at the dev UI),
         * later it will vary depending on the load reason (create/open/import project) */
        if (loading_screen_just_finished()) {
            app_state_set_screen(APP_SCREEN_EDITOR);
        }

        if (loading_screen_is_visible()) {
            /* The loading screen takes over the entire display - we draw no other screen
             * underneath while it's visible */
            loading_screen_draw(window_get_width(), window_get_height());
        } else if (app_state_get_screen() == APP_SCREEN_EDITOR) {
            /* Do not update the editor UI (click/drag/camera) while the file browser above it is open - the user interacts with only one window at a time, but it remains rendered underneath (dimmed) */
            if (!file_manager_is_open()) {
                editor_workspace_update(window_get_width(), window_get_height());
            }
            editor_workspace_draw(window_get_width(), window_get_height());
        } else {
            window_clear(UI_COLOR_BLACK_MUTED.r, UI_COLOR_BLACK_MUTED.g, UI_COLOR_BLACK_MUTED.b);
            ui_project_center_draw(window_get_width(), window_get_height());

            /* Project creation window - updated and drawn above everything, but only when open (same update-disable logic when the file browser overlays it - e.g., its Browse button opens the browser) */
            if (project_dialog_is_open()) {
                if (!file_manager_is_open()) {
                    project_dialog_update(window_get_width(), window_get_height());
                }
                project_dialog_draw(window_get_width(), window_get_height());
            }
        }

        /* Built-in file browser (file_manager) - always on top of everything else,
         * regardless of the current screen, because it's opened from
         * anywhere in the engine that needs to pick a file or folder (node
         * property, Browse button in the project creation window, etc.) */
        if (file_manager_is_open()) {
            file_manager_update(window_get_width(), window_get_height());
            file_manager_draw(window_get_width(), window_get_height());
        }

        window_present();
    }

    editor_workspace_shutdown();
    project_dialog_shutdown();
    ui_project_center_shutdown();
    file_manager_shutdown();
    ui_dialog_shutdown();
    window_shutdown();
    return 0;
}
