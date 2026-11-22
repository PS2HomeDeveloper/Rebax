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

#include "core/i18n.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/engine_settings.h"
#include "filesystem/rebax_fs.h"
#include "filesystem/storage_paths.h"

#define MAX_LANGUAGES 64
#define MAX_ENTRIES 4096

typedef struct { char code[16]; char name[48]; } language_t;
typedef struct { char *key; char *value; } entry_t;

static language_t g_languages[MAX_LANGUAGES];
static int g_language_count;
static entry_t g_entries[MAX_ENTRIES];
static int g_entry_count;
static char g_current[16] = "en";

static void unescape(char *s) {
    char *w = s;
    for (char *r = s; *r; r++) {
        if (*r == '\\' && r[1] == 'n') { *w++ = '\n'; r++; }
        else if (*r == '\\' && r[1] == '\\') { *w++ = '\\'; r++; }
        else *w++ = *r;
    }
    *w = '\0';
}

static void clear_entries(void) {
    for (int i = 0; i < g_entry_count; i++) { free(g_entries[i].key); free(g_entries[i].value); }
    g_entry_count = 0;
}

static void language_dir(char *out, size_t size) {
    snprintf(out, size, "%s/languages", rebax_root_dir());
}

static int read_name(const char *path, char *name, size_t size) {
    FILE *f = fopen(path, "rb");
    if (f == NULL) return 0;
    char line[512];
    int found = 0;
    while (fgets(line, sizeof(line), f) != NULL) {
        if (strncmp(line, "@name=", 6) == 0) {
            line[strcspn(line, "\r\n")] = '\0';
            snprintf(name, size, "%s", line + 6);
            found = 1;
            break;
        }
    }
    fclose(f);
    return found;
}

static void scan_callback(const char *file_path, void *user) {
    (void)user;
    const char *ext = rebax_fs_extension(file_path);
    if (ext == NULL || strcmp(ext, "lang") != 0 || g_language_count >= MAX_LANGUAGES) return;
    language_t *lang = &g_languages[g_language_count];
    rebax_fs_basename_no_ext(file_path, lang->code, sizeof(lang->code));
    if (strcmp(lang->code, "en") == 0) return;
    if (!read_name(file_path, lang->name, sizeof(lang->name))) snprintf(lang->name, sizeof(lang->name), "%s", lang->code);
    g_language_count++;
}

static void load_catalog(const char *code) {
    clear_entries();
    if (strcmp(code, "en") == 0) return;
    char dir[1024], path[1100];
    language_dir(dir, sizeof(dir));
    snprintf(path, sizeof(path), "%s/%s.lang", dir, code);
    FILE *f = fopen(path, "rb");
    if (f == NULL) return;
    char line[2048];
    while (fgets(line, sizeof(line), f) != NULL && g_entry_count < MAX_ENTRIES) {
        line[strcspn(line, "\r\n")] = '\0';
        if (line[0] == '@' || line[0] == '\0') continue;
        char *eq = strstr(line, "=>");
        if (eq == NULL) continue;
        *eq = '\0';
        char *value = eq + 2;
        unescape(line);
        unescape(value);
        g_entries[g_entry_count].key = strdup(line);
        g_entries[g_entry_count].value = strdup(value);
        g_entry_count++;
    }
    fclose(f);
}

void i18n_init(void) {
    char dir[1024];
    g_language_count = 0;
    snprintf(g_languages[0].code, sizeof(g_languages[0].code), "en");
    snprintf(g_languages[0].name, sizeof(g_languages[0].name), "English");
    g_language_count = 1;
    language_dir(dir, sizeof(dir));
    rebax_fs_mkdir_p(dir);
    rebax_fs_walk_files(dir, scan_callback, NULL);
    const char *wanted = engine_settings_get()->language;
    if (!i18n_set_language(wanted)) i18n_set_language("en");
}

void i18n_shutdown(void) {
    clear_entries();
}

const char *tr(const char *text) {
    for (int i = 0; i < g_entry_count; i++) {
        if (strcmp(g_entries[i].key, text) == 0) return g_entries[i].value;
    }
    return text;
}

int i18n_language_count(void) { return g_language_count; }
const char *i18n_language_code(int index) { return (index >= 0 && index < g_language_count) ? g_languages[index].code : ""; }
const char *i18n_language_name(int index) { return (index >= 0 && index < g_language_count) ? g_languages[index].name : ""; }
const char *i18n_current_language(void) { return g_current; }

int i18n_set_language(const char *code) {
    for (int i = 0; i < g_language_count; i++) {
        if (strcmp(g_languages[i].code, code) == 0) {
            snprintf(g_current, sizeof(g_current), "%s", code);
            load_catalog(code);
            return 1;
        }
    }
    return 0;
}
