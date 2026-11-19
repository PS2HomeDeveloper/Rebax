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

#include "project/current_project.h"

#include <stddef.h>
#include <string.h>

#define MAX_PATH_LEN 1024

static char g_path[MAX_PATH_LEN] = {0};
static project_settings_t g_settings;
static int g_is_open = 0;

void current_project_clear(void) {
    g_path[0] = '\0';
    g_is_open = 0;
}

void current_project_set(const char *root_path, const project_settings_t *settings) {
    if (root_path == NULL || root_path[0] == '\0' || settings == NULL) {
        current_project_clear();
        return;
    }
    strncpy(g_path, root_path, MAX_PATH_LEN - 1);
    g_path[MAX_PATH_LEN - 1] = '\0';
    g_settings = *settings;
    g_is_open = 1;
}

const char *current_project_get_path(void) { return g_is_open ? g_path : NULL; }

const project_settings_t *current_project_get_settings(void) { return g_is_open ? &g_settings : NULL; }

int current_project_is_open(void) { return g_is_open; }
