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

#ifndef PROJECT_FILE_H
#define PROJECT_FILE_H

#include "project/project_types.h"

#define PROJECT_FILE_NAME "project.rebax"
#define PROJECT_ASSETS_DIR "assets"
#define PROJECT_META_DIR ".rebax"
#define PROJECT_FORMAT_VERSION 1
#define PROJECT_NAME_MAX 64

typedef struct {
    int format;
    char name[PROJECT_NAME_MAX + 1];
    char engine_version[24];
    project_type_t type;
    dev_system_t dev_system;
    export_method_t export_method;
    unsigned platforms;
    int display_width;
    int display_height;
} project_settings_t;

void project_settings_defaults(project_settings_t *settings, const char *name, project_type_t type, dev_system_t dev_system);

const char *project_name_error(const char *name);

int project_file_save(const char *root, const project_settings_t *settings);

int project_file_load(const char *root, project_settings_t *settings, char *error, int error_cap);

int project_path_assets(const char *root, char *out, int cap);
int project_path_meta(const char *root, char *out, int cap);

#endif
