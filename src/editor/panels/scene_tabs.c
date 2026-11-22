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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "editor/panels/scene_tabs.h"
#include "scene/current_scene.h"
#include "scene/scene_data.h"
#include "editor/panels/properties.h"
#include "project/current_project.h"
#include "filesystem/rebax_fs.h"

#define SCENE_BUFFER_MAX (256 * 1024)

typedef struct {
    char *buffer;
    char *path;
    int dirty;
    char display_name[128];
} scene_tab_t;

static scene_tab_t g_tabs[SCENE_TABS_MAX];
static int g_tab_count = 0;
static int g_active_index = -1;

static void park_active_tab(void) {
    if (g_active_index < 0) return;
    scene_tab_t *t = &g_tabs[g_active_index];

    static char scratch[SCENE_BUFFER_MAX];
    int written = scene_data_serialize(scratch, sizeof(scratch));
    if (written < 0) written = 0;

    free(t->buffer);
    t->buffer = malloc((size_t)written + 1);
    if (t->buffer != NULL) {
        memcpy(t->buffer, scratch, (size_t)written);
        t->buffer[written] = '\0';
    }

    free(t->path);
    const char *path = current_scene_get_path();
    t->path = (path != NULL) ? strdup(path) : NULL;

    t->dirty = current_scene_is_dirty();

    strncpy(t->display_name, current_scene_get_display_name(), sizeof(t->display_name) - 1);
    t->display_name[sizeof(t->display_name) - 1] = '\0';
}

static void activate_tab(int index) {
    scene_tab_t *t = &g_tabs[index];

    scene_data_clear();
    properties_panel_clear_selection();
    if (t->buffer != NULL && t->buffer[0] != '\0') {
        scene_data_deserialize(t->buffer);
    }
    current_scene_restore_state(t->path, t->dirty);

    free(t->buffer);
    t->buffer = NULL;

    g_active_index = index;
}

void scene_tabs_init(void) {
    g_tab_count = 1;
    g_active_index = 0;
    memset(&g_tabs[0], 0, sizeof(scene_tab_t));
    current_scene_new();
}

int scene_tabs_count(void) {
    return g_tab_count;
}

int scene_tabs_active_index(void) {
    return g_active_index;
}

const char *scene_tabs_get_display_name(int index) {
    if (index == g_active_index) return current_scene_get_display_name();
    if (index < 0 || index >= g_tab_count) return "";
    return g_tabs[index].display_name;
}

int scene_tabs_get_dirty(int index) {
    if (index == g_active_index) return current_scene_is_dirty();
    if (index < 0 || index >= g_tab_count) return 0;
    return g_tabs[index].dirty;
}

int scene_tabs_new(void) {
    if (scene_data_get_node_count() == 0 && !current_scene_is_dirty()) {
        return 0;
    }
    if (g_tab_count >= SCENE_TABS_MAX) return 0;

    park_active_tab();

    int new_index = g_tab_count++;
    memset(&g_tabs[new_index], 0, sizeof(scene_tab_t));

    current_scene_new();
    g_active_index = new_index;
    return 1;
}

void scene_tabs_switch_to(int index) {
    if (index < 0 || index >= g_tab_count || index == g_active_index) return;
    park_active_tab();
    activate_tab(index);
}

void scene_tabs_close(int index) {
    if (index < 0 || index >= g_tab_count) return;

    free(g_tabs[index].buffer);
    free(g_tabs[index].path);

    int was_active = (index == g_active_index);

    for (int i = index; i < g_tab_count - 1; i++) {
        g_tabs[i] = g_tabs[i + 1];
    }
    g_tab_count--;

    if (g_tab_count == 0) {
        g_tab_count = 1;
        g_active_index = 0;
        memset(&g_tabs[0], 0, sizeof(scene_tab_t));
        current_scene_new();
        return;
    }

    if (was_active) {
        int next_index = (index < g_tab_count) ? index : g_tab_count - 1;
        activate_tab(next_index);
    } else if (g_active_index > index) {
        g_active_index--;
    }
}

void scene_tabs_shutdown(void) {
    for (int i = 0; i < g_tab_count; i++) {
        free(g_tabs[i].buffer);
        free(g_tabs[i].path);
    }
    g_tab_count = 0;
    g_active_index = -1;
}

static void unique_scene_path(const char *display_name, char *out, size_t size) {
    char clean[128];
    size_t n = 0;
    for (const char *p = display_name; *p != '\0' && n < sizeof(clean) - 1; p++) {
        char c = *p;
        clean[n++] = ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-') ? c : '_';
    }
    clean[n] = '\0';
    if (clean[0] == '\0') snprintf(clean, sizeof(clean), "scene");
    const char *root = current_project_get_path();
    snprintf(out, size, "%s/assets/%s.rscene", root != NULL ? root : ".", clean);
    for (int i = 2; rebax_fs_exists(out) && i < 1000; i++) {
        snprintf(out, size, "%s/assets/%s_%d.rscene", root != NULL ? root : ".", clean, i);
    }
}

int scene_tabs_save_all(void) {
    int saved = 0;
    for (int i = 0; i < g_tab_count; i++) {
        if (i == g_active_index) {
            if (current_scene_get_path() != NULL) {
                if (current_scene_is_dirty() && current_scene_save()) saved++;
            } else if (scene_data_get_node_count() > 0) {
                char path[1600];
                unique_scene_path(current_scene_get_display_name(), path, sizeof(path));
                if (current_scene_save_as(path)) saved++;
            }
            continue;
        }
        scene_tab_t *t = &g_tabs[i];
        if (t->buffer == NULL || t->buffer[0] == '\0') continue;
        if (t->path == NULL) {
            if (strstr(t->buffer, "[node ") == NULL) continue;
            char path[1600];
            unique_scene_path(t->display_name, path, sizeof(path));
            t->path = strdup(path);
        } else if (!t->dirty) {
            continue;
        }
        if (t->path != NULL && rebax_fs_write_atomic(t->path, t->buffer, strlen(t->buffer))) {
            t->dirty = 0;
            saved++;
        }
    }
    return saved;
}
