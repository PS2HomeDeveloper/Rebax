/*
 *  ____  _____ ____    _    __  __
 * |  _ \| ____| __ )  / \   \ \/ /
 * | |_) | _| | _ \ / _ \   \  /
 * |  _ <| |___| |_) / ___ \  /  \
 * |_| \_\_____|____/_/   \_\/_/\_\
 *
 * Rebax Engine
 * Copyright (c) 2026 PS2HomeDeveloper
 *
 * Licensed under the MIT License.
 * See the LICENSE file for details.
 */

#ifndef EMULATOR_CATALOG_H
#define EMULATOR_CATALOG_H

#include "project/project_types.h"

typedef struct {
    const char *id;
    const char *name;
    project_type_t system;
    const char *file_token;
    int desktop;
    int android;
} emulator_info_t;

int emulator_catalog_count(void);
const emulator_info_t *emulator_catalog_get(int index);
const emulator_info_t *emulator_catalog_find(const char *id);

int emulator_catalog_supported_here(const emulator_info_t *emu);

const char *emulator_executable(const emulator_info_t *emu);
int emulator_catalog_is_installed(const emulator_info_t *emu);
int emulator_path_matches(const emulator_info_t *emu, const char *path);
void emulator_set_executable(const emulator_info_t *emu, const char *path);

const char *emulator_selected_id(project_type_t system);
void emulator_select(project_type_t system, const char *id);
int emulator_selection_valid(project_type_t system);

#endif
