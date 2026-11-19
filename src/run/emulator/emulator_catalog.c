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
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "run/emulator/emulator_catalog.h"
#include "run/run_settings.h"
#include "run/run_mode.h"
#include "filesystem/rebax_fs.h"

static const emulator_info_t g_catalog[] = {
    { "pcsx2", "PCSX2", PROJECT_TYPE_PS2, "pcsx2", 1, 0 },
    { "aethersx2", "AetherSX2 / NetherSX2", PROJECT_TYPE_PS2, "sx2", 0, 1 },
    { "duckstation", "DuckStation", PROJECT_TYPE_PS1, "duckstation", 1, 1 },
    { "pcsx-redux", "PCSX-Redux", PROJECT_TYPE_PS1, "pcsx-redux", 1, 0 }
};

static const char *g_auto_paths[] = {
#if defined(_WIN32)
    "C:/Program Files/PCSX2/pcsx2-qt.exe",
    "C:/Program Files/DuckStation/duckstation-qt-x64-ReleaseLTCG.exe",
#elif defined(__APPLE__)
    "/Applications/PCSX2.app/Contents/MacOS/PCSX2",
    "/Applications/DuckStation.app/Contents/MacOS/DuckStation",
#else
    "/usr/bin/pcsx2-qt",
    "/usr/bin/pcsx2",
    "/usr/local/bin/pcsx2-qt",
    "/usr/bin/duckstation-qt",
    "/usr/local/bin/duckstation-qt",
#endif
    NULL
};

int emulator_catalog_count(void) { return (int)(sizeof(g_catalog) / sizeof(g_catalog[0])); }

const emulator_info_t *emulator_catalog_get(int index) {
    if (index < 0 || index >= emulator_catalog_count()) return NULL;
    return &g_catalog[index];
}

const emulator_info_t *emulator_catalog_find(const char *id) {
    if (!id || !id[0]) return NULL;
    for (int i = 0; i < emulator_catalog_count(); i++) if (strcmp(g_catalog[i].id, id) == 0) return &g_catalog[i];
    return NULL;
}

int emulator_catalog_supported_here(const emulator_info_t *emu) {
    return run_host_is_desktop() ? emu->desktop : emu->android;
}

static int contains_ci(const char *text, const char *token) {
    size_t n = strlen(token);
    for (; *text; text++) {
        size_t i = 0;
        while (i < n && text[i] && tolower((unsigned char)text[i]) == tolower((unsigned char)token[i])) i++;
        if (i == n) return 1;
    }
    return 0;
}

int emulator_path_matches(const emulator_info_t *emu, const char *path) {
    char base[260];
    rebax_fs_basename(path, base, sizeof(base));
    return contains_ci(base, emu->file_token);
}

const char *emulator_executable(const emulator_info_t *emu) {
    const char *saved = run_settings_emulator_path(emu->id);
    if (!run_host_is_desktop()) return saved;
    if (saved[0] && rebax_fs_exists(saved)) return saved;
    for (int i = 0; g_auto_paths[i]; i++)
        if (contains_ci(g_auto_paths[i], emu->file_token) && rebax_fs_exists(g_auto_paths[i])) return g_auto_paths[i];
    return "";
}

int emulator_catalog_is_installed(const emulator_info_t *emu) { return emulator_executable(emu)[0] != '\0'; }

void emulator_set_executable(const emulator_info_t *emu, const char *path) { run_settings_set_emulator_path(emu->id, path); }

const char *emulator_selected_id(project_type_t system) { return run_settings_emulator_id(system); }

void emulator_select(project_type_t system, const char *id) { run_settings_set_emulator_id(system, id); }

int emulator_selection_valid(project_type_t system) {
    const emulator_info_t *emu = emulator_catalog_find(emulator_selected_id(system));
    return emu && emu->system == system && emulator_catalog_is_installed(emu);
}
