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

#ifndef PROJECT_CREATE_H
#define PROJECT_CREATE_H

#include "project/project_types.h"

typedef enum {
    PROJECT_CREATE_OK = 0,
    PROJECT_CREATE_ERROR_INVALID_INPUT,
    PROJECT_CREATE_ERROR_EXISTS,
    PROJECT_CREATE_ERROR_MKDIR,
    PROJECT_CREATE_ERROR_FILE
} project_create_result_t;

typedef struct {
    const char *name;
    const char *base_path;
    project_type_t type;
    dev_system_t dev_system;
} project_create_options_t;

project_create_result_t project_create(const project_create_options_t *options);

const char *project_create_error_text(project_create_result_t result);

#endif
