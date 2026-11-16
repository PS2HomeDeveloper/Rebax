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
 * Licensed under the MIT License.
 * See the LICENSE file for details.
 */

#include "editor/core/project_session.h"

#include <stdio.h>
#include <string.h>

#include "editor/panels/file_system.h"
#include "editor/panels/scene_tabs.h"
#include "project/current_project.h"
#include "project/editor_state.h"
#include "project/project_catalog.h"
#include "project/project_file.h"

static void set_error(char *error, int cap, const char *text) {
    if (error != NULL && cap > 0) snprintf(error, (size_t)cap, "%s", text);
}

int project_session_open(const char *root, char *error, int error_cap) {
    project_settings_t settings;
    if (!project_file_load(root, &settings, error, error_cap)) return 0;

    const char *current = current_project_get_path();
    if (current != NULL && strcmp(current, root) == 0) {
        project_catalog_add(root);
        return 1;
    }

    for (int i = 0; i < scene_tabs_count(); i++) {
        if (scene_tabs_get_dirty(i)) {
            set_error(error, error_cap, "Save or close modified scenes before switching projects.");
            return 0;
        }
    }

    char last_scene[1200];
    int has_last = editor_state_load_last_scene(root, last_scene, sizeof(last_scene));

    current_project_set(root, &settings);
    scene_tabs_shutdown();
    scene_tabs_init();
    if (has_last) file_system_panel_open_scene(last_scene);
    project_catalog_add(root);
    return 1;
}

void project_session_close(void) {
    scene_tabs_shutdown();
    scene_tabs_init();
    current_project_clear();
}
