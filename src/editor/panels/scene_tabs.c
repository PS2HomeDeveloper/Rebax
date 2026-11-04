/*
 * ============================================================
 * scene_tabs.c
 * ============================================================
 * See scene_tabs.h for full documentation.
 * ============================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "scene_tabs.h"
#include "current_scene.h"
#include "scene_data.h"
#include "properties_panel.h"

/* Same as SCENE_BUFFER_MAX in current_scene.c exactly - one scene
 * exported as text at the maximum realistically expected size */
#define SCENE_BUFFER_MAX (256 * 1024)

typedef struct {
    char *buffer;    /* .rscene content in memory - NULL if this tab is currently active (its live state is in scene_tree_panel) */
    char *path;      /* A copy of the save path at the last switch-away - NULL means "not yet saved" */
    int dirty;
    char display_name[128];
} scene_tab_t;

static scene_tab_t g_tabs[SCENE_TABS_MAX];
static int g_tab_count = 0;
static int g_active_index = -1;

/* Stores the active tab's state in its private cache - called
 * before any switch-away. No-op if there is currently no active tab
 * (first call occurs before scene_tabs_init) */
static void park_active_tab(void) {
    if (g_active_index < 0) return;
    scene_tab_t *t = &g_tabs[g_active_index];

    static char scratch[SCENE_BUFFER_MAX];
    int written = scene_data_serialize(scratch, sizeof(scratch));
    if (written < 0) written = 0; /* Very rare case (scene larger than limit) - safe clear instead of crash */

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

/* Activates the tab at a given index - replaces the live
 * scene_tree_panel/current_scene content with this tab's stored content,
 * and frees its cache (it is now active elsewhere, so no stored copy is needed here) */
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
        return 0; /* The currently active tab is already empty and unmodified - no need for a second empty tab */
    }
    if (g_tab_count >= SCENE_TABS_MAX) return 0;

    park_active_tab();

    int new_index = g_tab_count++;
    memset(&g_tabs[new_index], 0, sizeof(scene_tab_t));

    current_scene_new(); /* Clears scene_tree_panel, initializes "empty" */
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
        /* Last tab - immediately create an empty one instead (at least one tab
         * remains open at all times) */
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
        g_active_index--; /* The active tab moved one position to the left */
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
