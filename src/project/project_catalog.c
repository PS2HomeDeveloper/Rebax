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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <errno.h>
#if !defined(_WIN32)
#include <unistd.h>
#endif

#include "project/project_catalog.h"
#include "filesystem/storage_paths.h"
#include "filesystem/rebax_fs.h"
#include "project/project_file.h"

static project_catalog_entry_t g_entries[PROJECT_CATALOG_MAX];
static int g_count;
static int g_loaded;

static void catalog_path(char *out, size_t cap) {
    snprintf(out, cap, "%s/projects.list", rebax_editor_dir());
}

int project_catalog_is_valid(const char *project_path) {
    if (project_path == NULL || project_path[0] == '\0') return 0;
    project_settings_t settings;
    return project_file_load(project_path, &settings, NULL, 0);
}

static int save_catalog(void) {
    char path[1024];
    static char text[PROJECT_CATALOG_MAX * (PROJECT_CATALOG_PATH_MAX + PROJECT_CATALOG_NAME_MAX + 40)];
    catalog_path(path, sizeof(path));
    size_t used = 0;
    for (int i = 0; i < g_count; i++) {
        int n = snprintf(text + used, sizeof(text) - used, "%lld\t%s\t%s\n", g_entries[i].last_opened,
                         g_entries[i].name, g_entries[i].path);
        if (n < 0 || (size_t)n >= sizeof(text) - used) return 0;
        used += (size_t)n;
    }
    return rebax_fs_write_atomic(path, text, used);
}

int project_catalog_load(void) {
    if (g_loaded) return g_count;
    g_loaded = 1;
    g_count = 0;
    char path[1024], line[PROJECT_CATALOG_PATH_MAX + PROJECT_CATALOG_NAME_MAX + 80];
    catalog_path(path, sizeof(path));
    FILE *f = fopen(path, "r");
    if (f == NULL) return 0;
    while (g_count < PROJECT_CATALOG_MAX && fgets(line, sizeof(line), f) != NULL) {
        line[strcspn(line, "\r\n")] = '\0';
        char *first = strchr(line, '\t');
        if (first == NULL) continue;
        *first++ = '\0';
        char *second = strchr(first, '\t');
        if (second == NULL) continue;
        *second++ = '\0';
        if (*second == '\0' || !project_catalog_is_valid(second)) continue;
        project_catalog_entry_t *entry = &g_entries[g_count++];
        entry->last_opened = atoll(line);
        snprintf(entry->name, sizeof(entry->name), "%s", first);
        snprintf(entry->path, sizeof(entry->path), "%s", second);
    }
    fclose(f);
    return g_count;
}

int project_catalog_count(void) { project_catalog_load(); return g_count; }
const project_catalog_entry_t *project_catalog_get(int index) {
    project_catalog_load();
    return (index >= 0 && index < g_count) ? &g_entries[index] : NULL;
}

int project_catalog_add(const char *project_path) {
    project_catalog_load();
    project_settings_t settings;
    if (!project_file_load(project_path, &settings, NULL, 0)) return 0;
    char name[PROJECT_CATALOG_NAME_MAX];
    snprintf(name, sizeof(name), "%s", settings.name);
    int found = -1;
    for (int i = 0; i < g_count; i++) if (strcmp(g_entries[i].path, project_path) == 0) { found = i; break; }
    if (found < 0) {
        if (g_count >= PROJECT_CATALOG_MAX) g_count = PROJECT_CATALOG_MAX - 1;
        found = g_count++;
    }
    for (int i = found; i > 0; i--) g_entries[i] = g_entries[i - 1];
    snprintf(g_entries[0].path, sizeof(g_entries[0].path), "%s", project_path);
    snprintf(g_entries[0].name, sizeof(g_entries[0].name), "%s", name);
    g_entries[0].last_opened = (long long)time(NULL);
    return save_catalog();
}

int project_catalog_remove(int index) {
    project_catalog_load();
    if (index < 0 || index >= g_count) return 0;
    for (int i = index; i + 1 < g_count; i++) g_entries[i] = g_entries[i + 1];
    g_count--;
    return save_catalog();
}
