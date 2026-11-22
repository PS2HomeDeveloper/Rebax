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

#include "core/engine_settings.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "filesystem/rebax_fs.h"
#include "filesystem/storage_paths.h"

static engine_settings_t g_settings = {0, "en"};
static int g_loaded;

static void settings_path(char *out, size_t size) {
    snprintf(out, size, "%s/settings.cfg", rebax_root_dir());
}

void engine_settings_load(void) {
    char path[1024];
    g_settings.ui_scale_percent = 0;
    snprintf(g_settings.language, sizeof(g_settings.language), "en");
    g_loaded = 1;
    settings_path(path, sizeof(path));
    FILE *f = fopen(path, "rb");
    if (f == NULL) return;
    char line[256];
    while (fgets(line, sizeof(line), f) != NULL) {
        char *eq = strchr(line, '=');
        if (eq == NULL) continue;
        *eq = '\0';
        char *value = eq + 1;
        value[strcspn(value, "\r\n")] = '\0';
        if (strcmp(line, "ui_scale") == 0) {
            int v = atoi(value);
            g_settings.ui_scale_percent = (v >= 50 && v <= 300) ? v : 0;
        } else if (strcmp(line, "language") == 0 && value[0] != '\0') {
            snprintf(g_settings.language, sizeof(g_settings.language), "%s", value);
        }
    }
    fclose(f);
}

void engine_settings_save(void) {
    char path[1024], data[256];
    settings_path(path, sizeof(path));
    int n = snprintf(data, sizeof(data), "ui_scale=%d\nlanguage=%s\n", g_settings.ui_scale_percent, g_settings.language);
    rebax_fs_mkdir_p(rebax_root_dir());
    rebax_fs_write_atomic(path, data, (size_t)n);
}

const engine_settings_t *engine_settings_get(void) {
    if (!g_loaded) engine_settings_load();
    return &g_settings;
}

void engine_settings_set_ui_scale(int percent) {
    engine_settings_get();
    g_settings.ui_scale_percent = percent;
    engine_settings_save();
}

void engine_settings_set_language(const char *code) {
    engine_settings_get();
    snprintf(g_settings.language, sizeof(g_settings.language), "%s", code);
    engine_settings_save();
}
