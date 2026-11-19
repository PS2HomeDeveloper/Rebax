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
#include <stdlib.h>
#include "run/run_settings.h"
#include "filesystem/storage_paths.h"
#include "filesystem/rebax_fs.h"

#define MAX_PATHS 8

static int g_loaded = 0;
static char g_address[64] = "192.168.0.10";
static int g_configured = 0;
static char g_emulator[PROJECT_TYPE_COUNT][32];
static char g_path_id[MAX_PATHS][32];
static char g_path_value[MAX_PATHS][900];

static void file_path(char *out, int size) { snprintf(out, size, "%s/run_settings.cfg", rebax_editor_dir()); }

static void store_path(const char *emulator_id, const char *path);

static void load(void) {
    if (g_loaded) return;
    g_loaded = 1;
    char path[1100];
    file_path(path, sizeof(path));
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[1000];
    while (fgets(line, sizeof(line), f)) {
        size_t n = strlen(line);
        while (n && (line[n - 1] == '\n' || line[n - 1] == '\r')) line[--n] = '\0';
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        const char *key = line, *val = eq + 1;
        if (strcmp(key, "ps2_address") == 0 && run_settings_valid_address(val)) snprintf(g_address, sizeof(g_address), "%s", val);
        else if (strcmp(key, "ps2_configured") == 0) g_configured = atoi(val) ? 1 : 0;
        else if (strcmp(key, "emulator_ps2") == 0) snprintf(g_emulator[PROJECT_TYPE_PS2], sizeof(g_emulator[0]), "%s", val);
        else if (strcmp(key, "emulator_ps1") == 0) snprintf(g_emulator[PROJECT_TYPE_PS1], sizeof(g_emulator[0]), "%s", val);
        else if (strncmp(key, "path_", 5) == 0) store_path(key + 5, val);
    }
    fclose(f);
}

static void save(void) {
    rebax_fs_mkdir_p(rebax_editor_dir());
    char path[1100];
    file_path(path, sizeof(path));
    char text[MAX_PATHS * 960 + 512];
    int n = snprintf(text, sizeof(text), "ps2_address=%s\nps2_configured=%d\nemulator_ps2=%s\nemulator_ps1=%s\n",
                     g_address, g_configured, g_emulator[PROJECT_TYPE_PS2], g_emulator[PROJECT_TYPE_PS1]);
    for (int i = 0; i < MAX_PATHS && n < (int)sizeof(text) - 960; i++)
        if (g_path_id[i][0] && g_path_value[i][0]) n += snprintf(text + n, sizeof(text) - n, "path_%s=%s\n", g_path_id[i], g_path_value[i]);
    rebax_fs_write_atomic(path, text, (size_t)n);
}

int run_settings_valid_address(const char *address) {
    int parts = 0, value = 0, digits = 0;
    if (!address) return 0;
    for (const char *p = address;; p++) {
        if (*p >= '0' && *p <= '9') { value = value * 10 + (*p - '0'); digits++; if (digits > 3 || value > 255) return 0; }
        else if (*p == '.' || *p == '\0') {
            if (!digits) return 0;
            parts++; value = 0; digits = 0;
            if (*p == '\0') break;
        } else return 0;
    }
    return parts == 4;
}

const char *run_settings_ps2_address(void) { load(); return g_address; }

void run_settings_set_ps2_address(const char *address) {
    load();
    if (!run_settings_valid_address(address)) return;
    snprintf(g_address, sizeof(g_address), "%s", address);
    save();
}

int run_settings_ps2_configured(void) { load(); return g_configured; }

void run_settings_set_ps2_configured(int configured) { load(); g_configured = configured ? 1 : 0; save(); }

const char *run_settings_emulator_id(project_type_t system) {
    load();
    if (system < 0 || system >= PROJECT_TYPE_COUNT) return "";
    return g_emulator[system];
}

void run_settings_set_emulator_id(project_type_t system, const char *id) {
    load();
    if (system < 0 || system >= PROJECT_TYPE_COUNT) return;
    snprintf(g_emulator[system], sizeof(g_emulator[0]), "%s", id ? id : "");
    save();
}

const char *run_settings_emulator_path(const char *emulator_id) {
    load();
    for (int i = 0; i < MAX_PATHS; i++) if (g_path_id[i][0] && strcmp(g_path_id[i], emulator_id) == 0) return g_path_value[i];
    return "";
}

static void store_path(const char *emulator_id, const char *path) {
    int slot = -1;
    for (int i = 0; i < MAX_PATHS; i++) {
        if (g_path_id[i][0] && strcmp(g_path_id[i], emulator_id) == 0) { slot = i; break; }
        if (slot < 0 && !g_path_id[i][0]) slot = i;
    }
    if (slot < 0) return;
    snprintf(g_path_id[slot], sizeof(g_path_id[0]), "%s", emulator_id);
    snprintf(g_path_value[slot], sizeof(g_path_value[0]), "%s", path ? path : "");
}

void run_settings_set_emulator_path(const char *emulator_id, const char *path) {
    load();
    store_path(emulator_id, path);
    save();
}
