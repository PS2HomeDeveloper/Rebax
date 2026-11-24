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
#include "filesystem/resources.h"
#include "filesystem/storage_paths.h"

#define MAX_LANGUAGES 64
#define MAX_ENTRIES 4096
#define HASH_SIZE 16384

typedef struct { char code[16]; char name[48]; int builtin; } language_t;
typedef struct { char *key; char *value; } entry_t;

static const char *const g_builtin_codes[] = { "ar" };

static language_t g_languages[MAX_LANGUAGES];
static int g_language_count;
static entry_t g_entries[MAX_ENTRIES];
static int g_entry_count;
static short g_hash[HASH_SIZE];
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

static unsigned hash_text(const char *s) {
    unsigned h = 2166136261u;
    for (; *s; s++) { h ^= (unsigned char)*s; h *= 16777619u; }
    return h;
}

static void hash_insert(int index) {
    unsigned slot = hash_text(g_entries[index].key) & (HASH_SIZE - 1);
    while (g_hash[slot] != 0) {
        if (strcmp(g_entries[g_hash[slot] - 1].key, g_entries[index].key) == 0) {
            g_hash[slot] = (short)(index + 1);
            return;
        }
        slot = (slot + 1) & (HASH_SIZE - 1);
    }
    g_hash[slot] = (short)(index + 1);
}

static void clear_entries(void) {
    for (int i = 0; i < g_entry_count; i++) { free(g_entries[i].key); free(g_entries[i].value); }
    g_entry_count = 0;
    memset(g_hash, 0, sizeof(g_hash));
}

static void language_dir(char *out, size_t size) {
    snprintf(out, size, "%s/languages", rebax_root_dir());
}

static unsigned char *read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (f == NULL) return NULL;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (size < 0) { fclose(f); return NULL; }
    unsigned char *data = (unsigned char *)malloc((size_t)size + 1);
    if (data == NULL) { fclose(f); return NULL; }
    size_t got = fread(data, 1, (size_t)size, f);
    fclose(f);
    data[got] = '\0';
    return data;
}

static void parse_text(char *text, char *name, size_t name_size, int load_entries) {
    char *p = text;
    if ((unsigned char)p[0] == 0xEF && (unsigned char)p[1] == 0xBB && (unsigned char)p[2] == 0xBF) p += 3;
    while (*p != '\0') {
        char *line = p;
        char *end = strchr(p, '\n');
        if (end != NULL) { *end = '\0'; p = end + 1; } else p += strlen(p);
        line[strcspn(line, "\r")] = '\0';
        if (line[0] == '\0') continue;
        if (line[0] == '@') {
            if (name != NULL && strncmp(line, "@name=", 6) == 0) snprintf(name, name_size, "%s", line + 6);
            continue;
        }
        if (!load_entries || g_entry_count >= MAX_ENTRIES) continue;
        char *eq = strstr(line, "=>");
        if (eq == NULL) continue;
        *eq = '\0';
        char *value = eq + 2;
        unescape(line);
        unescape(value);
        g_entries[g_entry_count].key = strdup(line);
        g_entries[g_entry_count].value = strdup(value);
        hash_insert(g_entry_count);
        g_entry_count++;
    }
}

static unsigned char *read_language(const language_t *lang, int user_first) {
    char dir[1024], path[1100];
    unsigned char *data = NULL;
    language_dir(dir, sizeof(dir));
    snprintf(path, sizeof(path), "%s/%s.lang", dir, lang->code);
    if (user_first) data = read_file(path);
    if (data == NULL && lang->builtin) {
        char rel[64];
        snprintf(rel, sizeof(rel), "languages/%s.lang", lang->code);
        data = resource_file_read(rel, NULL);
    }
    return data;
}

static int find_language(const char *code) {
    for (int i = 0; i < g_language_count; i++) if (strcmp(g_languages[i].code, code) == 0) return i;
    return -1;
}

static void add_language(const char *code, int builtin, const char *fallback_name) {
    if (strcmp(code, "en") == 0 || g_language_count >= MAX_LANGUAGES) return;
    int index = find_language(code);
    if (index < 0) {
        index = g_language_count++;
        snprintf(g_languages[index].code, sizeof(g_languages[index].code), "%s", code);
        snprintf(g_languages[index].name, sizeof(g_languages[index].name), "%s", fallback_name != NULL ? fallback_name : code);
        g_languages[index].builtin = 0;
    }
    if (builtin) g_languages[index].builtin = 1;
    unsigned char *data = read_language(&g_languages[index], 1);
    if (data != NULL) {
        char name[48] = "";
        parse_text((char *)data, name, sizeof(name), 0);
        if (name[0] != '\0') snprintf(g_languages[index].name, sizeof(g_languages[index].name), "%s", name);
        free(data);
    }
}

static void scan_callback(const char *file_path, void *user) {
    (void)user;
    const char *ext = rebax_fs_extension(file_path);
    if (ext == NULL || strcmp(ext, "lang") != 0) return;
    char code[16];
    rebax_fs_basename_no_ext(file_path, code, sizeof(code));
    add_language(code, 0, NULL);
}

static void load_catalog(const char *code) {
    clear_entries();
    if (strcmp(code, "en") == 0) return;
    int index = find_language(code);
    if (index < 0) return;
    unsigned char *data = read_language(&g_languages[index], 1);
    if (data == NULL) return;
    parse_text((char *)data, NULL, 0, 1);
    free(data);
}

void i18n_init(void) {
    char dir[1024];
    g_language_count = 0;
    snprintf(g_languages[0].code, sizeof(g_languages[0].code), "en");
    snprintf(g_languages[0].name, sizeof(g_languages[0].name), "English");
    g_languages[0].builtin = 1;
    g_language_count = 1;
    for (size_t i = 0; i < sizeof(g_builtin_codes) / sizeof(g_builtin_codes[0]); i++) add_language(g_builtin_codes[i], 1, NULL);
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
    if (g_entry_count == 0 || text == NULL || text[0] == '\0') return text;
    unsigned slot = hash_text(text) & (HASH_SIZE - 1);
    while (g_hash[slot] != 0) {
        const entry_t *e = &g_entries[g_hash[slot] - 1];
        if (strcmp(e->key, text) == 0) return e->value;
        slot = (slot + 1) & (HASH_SIZE - 1);
    }
    return text;
}

int i18n_language_count(void) { return g_language_count; }
const char *i18n_language_code(int index) { return (index >= 0 && index < g_language_count) ? g_languages[index].code : ""; }
const char *i18n_language_name(int index) { return (index >= 0 && index < g_language_count) ? g_languages[index].name : ""; }
const char *i18n_current_language(void) { return g_current; }

int i18n_set_language(const char *code) {
    if (find_language(code) < 0) return 0;
    snprintf(g_current, sizeof(g_current), "%s", code);
    load_catalog(code);
    return 1;
}
