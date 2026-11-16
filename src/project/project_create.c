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

#include "project/project_create.h"

#include <stdio.h>
#include <string.h>

#include "filesystem/rebax_fs.h"
#include "project/project_file.h"

#define DEFAULT_SCENE "scene_version=1\nnode_count=0\n\n"
#define DEFAULT_SCENE_FILE "main.rscene"

project_create_result_t project_create(const project_create_options_t *o) {
    if (o == NULL || o->base_path == NULL || o->base_path[0] == '\0' || project_name_error(o->name) != NULL)
        return PROJECT_CREATE_ERROR_INVALID_INPUT;
    if ((unsigned)o->type >= PROJECT_TYPE_COUNT || !dev_system_is_available(o->dev_system))
        return PROJECT_CREATE_ERROR_INVALID_INPUT;

    char root[1100], path[1200];
    snprintf(root, sizeof(root), "%s/%s", o->base_path, o->name);

    snprintf(path, sizeof(path), "%s/%s", root, PROJECT_FILE_NAME);
    if (rebax_fs_exists(path)) return PROJECT_CREATE_ERROR_EXISTS;

    if (!rebax_fs_mkdir_p(root)) return PROJECT_CREATE_ERROR_MKDIR;
    project_path_assets(root, path, sizeof(path));
    if (!rebax_fs_mkdir_p(path)) return PROJECT_CREATE_ERROR_MKDIR;
    project_path_meta(root, path, sizeof(path));
    if (!rebax_fs_mkdir_p(path)) return PROJECT_CREATE_ERROR_MKDIR;

    snprintf(path, sizeof(path), "%s/%s/%s", root, PROJECT_ASSETS_DIR, DEFAULT_SCENE_FILE);
    if (!rebax_fs_exists(path) && !rebax_fs_write_atomic(path, DEFAULT_SCENE, strlen(DEFAULT_SCENE)))
        return PROJECT_CREATE_ERROR_FILE;

    project_settings_t settings;
    project_settings_defaults(&settings, o->name, o->type, o->dev_system);
    if (!project_file_save(root, &settings)) return PROJECT_CREATE_ERROR_FILE;
    return PROJECT_CREATE_OK;
}

const char *project_create_error_text(project_create_result_t result) {
    switch (result) {
        case PROJECT_CREATE_OK: return "";
        case PROJECT_CREATE_ERROR_INVALID_INPUT: return "The project name or location is not valid.";
        case PROJECT_CREATE_ERROR_EXISTS: return "A project already exists in this location.";
        case PROJECT_CREATE_ERROR_MKDIR: return "Could not create the project folders.";
        default: return "Could not write the project files.";
    }
}
