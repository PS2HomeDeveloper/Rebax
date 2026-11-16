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

#ifndef PROJECT_CATALOG_H
#define PROJECT_CATALOG_H

#define PROJECT_CATALOG_MAX 64
#define PROJECT_CATALOG_PATH_MAX 600
#define PROJECT_CATALOG_NAME_MAX 128

typedef struct {
    char path[PROJECT_CATALOG_PATH_MAX];
    char name[PROJECT_CATALOG_NAME_MAX];
    long long last_opened;
} project_catalog_entry_t;

int project_catalog_load(void);
int project_catalog_count(void);
const project_catalog_entry_t *project_catalog_get(int index);
int project_catalog_add(const char *project_path);
int project_catalog_remove(int index);
int project_catalog_is_valid(const char *project_path);

#endif
