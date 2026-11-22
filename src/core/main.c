/*
 *  ____  _____ ____    _    __  __
 * |  _ \| ____| __ )  / \   \ \/ /
 * | |_) |  _| |  _ \ / _ \   \  / 
 * |  _ <| |___| |_) / ___ \  /  \ 
 * |_| \_\_____|____/_/   \_\/_/\_\
 *
 * Rebax Engine
 * Copyright (c) 2026 PS2HomeDeveloper
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <stdio.h>

#include "platform/common/window.h"
#include "editor/panels/project_center.h"
#include "editor/ui/draw_kit.h"
#include "editor/dialogs/project.h"
#include "editor/dialogs/packages.h"
#include "editor/ui/loading_screen.h"
#include "editor/core/workspace.h"
#include "core/app_state.h"
#include "filesystem/storage_paths.h"
#include "editor/panels/file_manager.h"
#include "editor/ui/dialog.h"
#include "core/engine_settings.h"
#include "core/i18n.h"
#include "editor/panels/settings_page.h"
#include "platform/android/storage.h"

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    if (!window_init("Rebax Engine", 900, 560)) {
        return 1;
    }

    printf("Rebax_Engine started successfully.\n");

    window_set_ui_scale_percent(engine_settings_get()->ui_scale_percent);
    i18n_init();

    android_storage_request_if_needed();

    int rebax_setup_needed = rebax_paths_is_setup_needed();
    if (rebax_setup_needed) {
        loading_screen_show_indefinite();
        rebax_paths_setup_start();
    }

    while (!window_should_close()) {
        window_poll_events();

        if (rebax_setup_needed && !rebax_paths_setup_done()) {
            loading_screen_update();
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

        if (loading_screen_just_finished()) {
            app_state_set_screen(APP_SCREEN_EDITOR);
        }

        if (loading_screen_is_visible()) {
            loading_screen_draw(window_get_width(), window_get_height());
        } else if (app_state_get_screen() == APP_SCREEN_EDITOR) {
            if (!file_manager_is_open() && !packages_dialog_is_open()) {
                editor_workspace_update(window_get_width(), window_get_height());
            }
            editor_workspace_draw(window_get_width(), window_get_height());
            if (packages_dialog_is_open()) {
                packages_dialog_update(window_get_width(), window_get_height());
                packages_dialog_draw(window_get_width(), window_get_height());
            }
        } else if (app_state_get_screen() == APP_SCREEN_SETTINGS) {
            settings_page_update_and_draw(window_get_width(), window_get_height());
            if (settings_page_wants_close()) app_state_set_screen(APP_SCREEN_PROJECT_CENTER);
        } else {
            window_clear(UI_COLOR_BLACK_MUTED.r, UI_COLOR_BLACK_MUTED.g, UI_COLOR_BLACK_MUTED.b);
            ui_project_center_draw(window_get_width(), window_get_height());

            if (project_dialog_is_open()) {
                if (!file_manager_is_open()) {
                    project_dialog_update(window_get_width(), window_get_height());
                }
                project_dialog_draw(window_get_width(), window_get_height());
            }

            if (packages_dialog_is_open()) {
                packages_dialog_update(window_get_width(), window_get_height());
                packages_dialog_draw(window_get_width(), window_get_height());
            }
        }

        if (file_manager_is_open()) {
            file_manager_update(window_get_width(), window_get_height());
            file_manager_draw(window_get_width(), window_get_height());
        }

        window_present();
    }

    editor_workspace_shutdown();
    project_dialog_shutdown();
    packages_dialog_shutdown();
    ui_project_center_shutdown();
    file_manager_shutdown();
    ui_kit_shutdown();
    ui_dialog_shutdown();
    i18n_shutdown();
    window_shutdown();
    return 0;
}
