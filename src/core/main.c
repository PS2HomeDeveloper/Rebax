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
#include <string.h>
#include <SDL_timer.h>

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
#include "editor/ui/windows.h"
#include "core/engine_settings.h"
#include "core/i18n.h"
#include "editor/panels/settings_page.h"
#include "platform/android/storage.h"
#include "project/project_file.h"
#include "project/current_project.h"
#include "exporters/compiler/modern/modern_exporter.h"
#include "exporters/compiler/ps1/ps1_exporter.h"
#include "exporters/compiler/ps2/ps2_exporter.h"

static int headless_command(int argc, char **argv) {
    if (argc < 2 || (strcmp(argv[1], "export") != 0 && strcmp(argv[1], "check") != 0)) return -1;
    if (argc < 4) { fprintf(stderr, "Usage: %s export <project_dir> <platform> <architecture|all> <output_dir> [release]\n", argv[0]); return 2; }
    project_settings_t settings; char error[256];
    if (!project_file_load(argv[2], &settings, error, sizeof(error))) { fprintf(stderr, "[rebax] check failed: %s\n", error); return 1; }
    current_project_set(argv[2], &settings);
    if (strcmp(argv[1], "check") == 0) { printf("Rebax project is valid: %s (%s)\n", settings.name, project_type_id(settings.type)); current_project_clear(); return 0; }
    if (argc < 6) { fprintf(stderr, "[rebax] export requires output_dir.\n"); current_project_clear(); return 2; }
    const char *platform = argv[3], *arch = argv[4], *out = argv[5];
    int release = argc > 6 && strcmp(argv[6], "release") == 0;
    int started = 0;
    if (!strcmp(platform, "modern") || !strcmp(platform, "linux") || !strcmp(platform, "windows") || !strcmp(platform, "android") || !strcmp(platform, "macos") || !strcmp(platform, "ios")) {
        if (strcmp(platform, "modern") == 0) platform = "linux";
        if (!strcmp(arch, "all")) {
            static const char *all_linux[] = {"arm64", "arm", "x86", "x86_64"};
            static const char *all_android[] = {"arm64-v8a", "armeabi-v7a", "x86", "x86_64"};
            const char *const *list = !strcmp(platform, "android") ? all_android : all_linux;
            int count = !strcmp(platform, "android") ? 4 : 4;
            started = modern_export_start_multi(settings.name, release, out, platform, list, count);
        } else started = modern_export_start(settings.name, release, out, platform, arch);
        while (modern_export_get_status() == MODERN_EXPORT_RUNNING) { modern_export_update(); SDL_Delay(10); }
        int ok = modern_export_get_status() == MODERN_EXPORT_SUCCESS; current_project_clear(); return started && ok ? 0 : 1;
    }
    if (!strcmp(platform, "ps1")) started = ps1_export_start(settings.name, release, out);
    else if (!strcmp(platform, "ps2")) started = ps2_export_start(settings.name, release, out);
    else { fprintf(stderr, "[rebax] unsupported platform: %s\n", platform); current_project_clear(); return 2; }
    while (started && ((platform[2] == '1' && ps1_export_get_status() == PS1_EXPORT_STATUS_RUNNING) || (platform[2] == '2' && ps2_export_get_status() == PS2_EXPORT_STATUS_RUNNING))) { if (platform[2] == '1') ps1_export_update(); else ps2_export_update(); SDL_Delay(10); }
    int ok = (platform[2] == '1' ? ps1_export_get_status() == PS1_EXPORT_STATUS_SUCCESS : ps2_export_get_status() == PS2_EXPORT_STATUS_SUCCESS);
    current_project_clear(); return started && ok ? 0 : 1;
}

int main(int argc, char **argv) {
    int headless = headless_command(argc, argv);
    if (headless >= 0) return headless;
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
            if (ui_windows_any_open()) ui_windows_update(window_get_width(), window_get_height());
            else editor_workspace_update(window_get_width(), window_get_height());
            editor_workspace_poll();
            editor_workspace_draw(window_get_width(), window_get_height());
        } else if (app_state_get_screen() == APP_SCREEN_SETTINGS) {
            settings_page_update_and_draw(window_get_width(), window_get_height());
            if (settings_page_wants_close()) app_state_close_settings();
        } else {
            window_clear(UI_COLOR_BLACK_MUTED.r, UI_COLOR_BLACK_MUTED.g, UI_COLOR_BLACK_MUTED.b);
            ui_project_center_draw(window_get_width(), window_get_height());
            if (ui_windows_any_open()) ui_windows_update(window_get_width(), window_get_height());
        }

        ui_windows_draw(window_get_width(), window_get_height());

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
