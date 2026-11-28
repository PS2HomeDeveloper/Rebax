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

#include <string.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>

#include "scene/scene_data.h"
#include "dev_systems/nodes/runtime/node_registry.h"
#include "scene/current_scene.h"

#define SCENE_NAME_MAX 32

typedef struct {
    node_type_t type;
    char name[SCENE_NAME_MAX];
    int parent_index;
    int expanded;
    node_property_value_t *values;
    int value_count;
} scene_node_t;

static scene_node_t g_nodes[SCENE_DATA_MAX_NODES];
static int g_node_count = 0;
static int g_selected_index = -1;
static unsigned char g_selected_nodes[SCENE_DATA_MAX_NODES];

static void scene_data_free_node(scene_node_t *node) {
    if (node->values != NULL) {
        const node_registry_entry_t *entry = node_registry_get(node->type);
        for (int p = 0; p < node->value_count; p++) {
            if (entry != NULL && entry->properties[p].type == NODE_PROPERTY_TYPE_STRING) free(node->values[p].s);
        }
        free(node->values);
    }
    memset(node, 0, sizeof(*node));
}

static scene_node_t *scene_data_create_raw(node_type_t type, const char *name, int parent_index) {
    if (g_node_count >= SCENE_DATA_MAX_NODES) return NULL;
    scene_node_t *node = &g_nodes[g_node_count];
    memset(node, 0, sizeof(*node));
    node->type = type;
    strncpy(node->name, name ? name : "", sizeof(node->name) - 1);
    node->parent_index = parent_index;
    const node_registry_entry_t *entry = node_registry_get(type);
    node->value_count = (entry != NULL) ? entry->property_count : 0;
    if (node->value_count > 0) {
        node->values = calloc((size_t)node->value_count, sizeof(*node->values));
        if (node->values == NULL) { node->value_count = 0; return NULL; }
        for (int i = 0; i < node->value_count; i++) {
            const node_property_t *prop = &entry->properties[i];
            switch (prop->type) {
                case NODE_PROPERTY_TYPE_FLOAT: node->values[i].f = prop->default_value.f; break;
                case NODE_PROPERTY_TYPE_INT: node->values[i].i = prop->default_value.i; break;
                case NODE_PROPERTY_TYPE_STRING: node->values[i].s = strdup(prop->default_value.s ? prop->default_value.s : ""); break;
            }
        }
    }
    if (parent_index >= 0 && parent_index < g_node_count) g_nodes[parent_index].expanded = 1;
    g_node_count++;
    return node;
}

void scene_data_add_node(node_type_t type, const char *name) {
    int parent = (g_node_count == 0) ? -1 :
                 ((g_selected_index >= 0 && g_selected_index < g_node_count) ? g_selected_index : 0);
    if (scene_data_create_raw(type, name, parent) != NULL) current_scene_mark_dirty();
}

void scene_data_clear(void) {
    for (int i = 0; i < g_node_count; i++) scene_data_free_node(&g_nodes[i]);
    g_node_count = 0;
    g_selected_index = -1;
    memset(g_selected_nodes, 0, sizeof(g_selected_nodes));
}

int scene_data_get_node_count(void) { return g_node_count; }
node_type_t scene_data_get_type(int index) { return (index >= 0 && index < g_node_count) ? g_nodes[index].type : NODE_TYPE_ELEMENT; }
const char *scene_data_get_name(int index) { return (index >= 0 && index < g_node_count) ? g_nodes[index].name : NULL; }
node_property_value_t *scene_data_get_values(int index) { return (index >= 0 && index < g_node_count) ? g_nodes[index].values : NULL; }
int scene_data_get_parent(int index) { return (index >= 0 && index < g_node_count) ? g_nodes[index].parent_index : -1; }

int scene_data_has_children(int index) {
    if (index < 0 || index >= g_node_count) return 0;
    for (int i = 0; i < g_node_count; i++) if (g_nodes[i].parent_index == index) return 1;
    return 0;
}
int scene_data_is_expanded(int index) { return (index >= 0 && index < g_node_count) ? g_nodes[index].expanded : 0; }
void scene_data_toggle_expanded(int index) { if (scene_data_has_children(index)) g_nodes[index].expanded = !g_nodes[index].expanded; }

int scene_data_build_visible(int *out_indices, int *out_depths, int max_items) {
    if (out_indices == NULL || out_depths == NULL || max_items <= 0) return 0;
    int count = 0;
    for (int i = 0; i < g_node_count && count < max_items; i++) {
        if (g_nodes[i].parent_index == -1) { out_indices[count] = i; out_depths[count++] = 0; }
    }
    for (int pass = 0; pass < SCENE_DATA_MAX_NODES; pass++) {
        int inserted = 0;
        for (int vi = 0; vi < count; vi++) {
            int parent = out_indices[vi];
            if (!g_nodes[parent].expanded) continue;
            int already = (vi + 1 < count && g_nodes[out_indices[vi + 1]].parent_index == parent);
            if (already) continue;
            int children[SCENE_DATA_MAX_NODES];
            int child_count = 0;
            for (int n = 0; n < g_node_count && child_count < SCENE_DATA_MAX_NODES; n++) {
                if (g_nodes[n].parent_index == parent) children[child_count++] = n;
            }
            if (child_count == 0) continue;
            if (child_count > max_items - count) child_count = max_items - count;
            for (int k = count - 1; k > vi; k--) { out_indices[k + child_count] = out_indices[k]; out_depths[k + child_count] = out_depths[k]; }
            for (int c = 0; c < child_count; c++) { out_indices[vi + 1 + c] = children[c]; out_depths[vi + 1 + c] = out_depths[vi] + 1; }
            count += child_count;
            inserted = 1;
            break;
        }
        if (!inserted) break;
    }
    return count;
}

int scene_data_get_selected_index(void) { return g_selected_index; }
void scene_data_clear_selection(void) {
    memset(g_selected_nodes, 0, sizeof(g_selected_nodes));
    g_selected_index = -1;
}
void scene_data_select_index(int index) {
    scene_data_clear_selection();
    if (index >= 0 && index < g_node_count) {
        g_selected_index = index;
        g_selected_nodes[index] = 1;
    }
}
void scene_data_add_to_selection(int index) {
    if (index < 0 || index >= g_node_count) return;
    g_selected_nodes[index] = 1;
    g_selected_index = index;
}
int scene_data_is_selected(int index) {
    return index >= 0 && index < g_node_count && g_selected_nodes[index] != 0;
}

void scene_data_set_float_property(int index, int property_index, float value) {
    if (index < 0 || index >= g_node_count || property_index < 0 || property_index >= g_nodes[index].value_count) return;
    const node_registry_entry_t *entry = node_registry_get(g_nodes[index].type);
    if (entry == NULL || entry->properties[property_index].type != NODE_PROPERTY_TYPE_FLOAT || g_nodes[index].values[property_index].f == value) return;
    g_nodes[index].values[property_index].f = value;
    current_scene_mark_dirty();
}

#include <math.h>

int scene_data_has_transform(int index) {
    if (index < 0 || index >= g_node_count) return 0;
    const node_registry_entry_t *entry = node_registry_get(g_nodes[index].type);
    return entry != NULL && entry->property_count >= 5 && g_nodes[index].values != NULL &&
           entry->properties[0].type == NODE_PROPERTY_TYPE_FLOAT && entry->properties[1].type == NODE_PROPERTY_TYPE_FLOAT &&
           strcmp(entry->properties[0].name, "Position X") == 0;
}

void scene_data_world_transform(int index, float *x, float *y, float *rotation, float *scale_x, float *scale_y) {
    float px = 0.0f, py = 0.0f, rot = 0.0f, sx = 1.0f, sy = 1.0f;
    if (scene_data_has_transform(index)) {
        const node_property_value_t *v = g_nodes[index].values;
        px = v[0].f; py = v[1].f; rot = v[2].f; sx = v[3].f; sy = v[4].f;
        int parent = g_nodes[index].parent_index, guard = 0;
        while (parent >= 0 && parent < g_node_count && guard++ < SCENE_DATA_MAX_NODES) {
            if (scene_data_has_transform(parent)) {
                const node_property_value_t *u = g_nodes[parent].values;
                float c = cosf(u[2].f), s = sinf(u[2].f);
                float ax = px * u[3].f, ay = py * u[4].f;
                px = u[0].f + ax * c - ay * s;
                py = u[1].f + ax * s + ay * c;
                rot += u[2].f; sx *= u[3].f; sy *= u[4].f;
            }
            parent = g_nodes[parent].parent_index;
        }
    }
    if (x) *x = px;
    if (y) *y = py;
    if (rotation) *rotation = rot;
    if (scale_x) *scale_x = sx;
    if (scale_y) *scale_y = sy;
}

void scene_data_set_world_position(int index, float x, float y) {
    if (!scene_data_has_transform(index)) return;
    float lx = x, ly = y;
    int parent = g_nodes[index].parent_index;
    while (parent >= 0 && parent < g_node_count && !scene_data_has_transform(parent)) parent = g_nodes[parent].parent_index;
    if (parent >= 0 && parent < g_node_count) {
        float wx, wy, wr, wsx, wsy;
        scene_data_world_transform(parent, &wx, &wy, &wr, &wsx, &wsy);
        float dx = x - wx, dy = y - wy;
        float c = cosf(-wr), s = sinf(-wr);
        float rx = dx * c - dy * s, ry = dx * s + dy * c;
        lx = fabsf(wsx) > 0.0001f ? rx / wsx : rx;
        ly = fabsf(wsy) > 0.0001f ? ry / wsy : ry;
    }
    scene_data_set_float_property(index, 0, lx);
    scene_data_set_float_property(index, 1, ly);
}

void scene_data_rename(int index, const char *name) {
    if (index < 0 || index >= g_node_count || name == NULL || name[0] == '\0') return;
    char unique[SCENE_NAME_MAX];
    snprintf(unique, sizeof(unique), "%s", name);
    for (int n = 2; n < 1000; n++) {
        int clash = 0;
        for (int i = 0; i < g_node_count; i++)
            if (i != index && g_nodes[i].parent_index == g_nodes[index].parent_index && strcmp(g_nodes[i].name, unique) == 0) clash = 1;
        if (!clash) break;
        char suffix[12];
        snprintf(suffix, sizeof(suffix), "%d", n);
        snprintf(unique, sizeof(unique), "%.*s%s", (int)(SCENE_NAME_MAX - 1 - strlen(suffix)), name, suffix);
    }
    snprintf(g_nodes[index].name, sizeof(g_nodes[index].name), "%s", unique);
    current_scene_mark_dirty();
}

void scene_data_get_path(int index, char *out, int out_size) {
    if (out == NULL || out_size <= 0) return;
    out[0] = '\0';
    int chain[SCENE_DATA_MAX_NODES], depth = 0;
    while (index >= 0 && index < g_node_count && depth < SCENE_DATA_MAX_NODES) { chain[depth++] = index; index = g_nodes[index].parent_index; }
    for (int i = depth - 1; i >= 0; i--) {
        size_t used = strlen(out);
        snprintf(out + used, (size_t)out_size - used, "%s%s", i == depth - 1 ? "" : "/", g_nodes[chain[i]].name);
    }
}

static int collect_subtree(int root, int *list) {
    int count = 0;
    list[count++] = root;
    for (int i = 0; i < count; i++)
        for (int n = 0; n < g_node_count && count < SCENE_DATA_MAX_NODES; n++)
            if (g_nodes[n].parent_index == list[i] && n != list[i]) list[count++] = n;
    return count;
}

void scene_data_delete(int index) {
    if (index < 0 || index >= g_node_count) return;
    int list[SCENE_DATA_MAX_NODES];
    int count = collect_subtree(index, list);
    int remap[SCENE_DATA_MAX_NODES], doomed[SCENE_DATA_MAX_NODES] = {0};
    for (int i = 0; i < count; i++) doomed[list[i]] = 1;
    scene_node_t kept[SCENE_DATA_MAX_NODES];
    unsigned char kept_selected[SCENE_DATA_MAX_NODES];
    int kept_count = 0;
    for (int i = 0; i < g_node_count; i++) {
        if (doomed[i]) { scene_data_free_node(&g_nodes[i]); remap[i] = -1; continue; }
        remap[i] = kept_count;
        kept_selected[kept_count] = g_selected_nodes[i];
        kept[kept_count++] = g_nodes[i];
    }
    for (int i = 0; i < kept_count; i++)
        if (kept[i].parent_index >= 0) kept[i].parent_index = kept[i].parent_index < g_node_count ? remap[kept[i].parent_index] : -1;
    memset(g_nodes, 0, sizeof(g_nodes));
    memset(g_selected_nodes, 0, sizeof(g_selected_nodes));
    for (int i = 0; i < kept_count; i++) { g_nodes[i] = kept[i]; g_selected_nodes[i] = kept_selected[i]; }
    g_node_count = kept_count;
    g_selected_index = -1;
    current_scene_mark_dirty();
}

typedef struct { node_type_t type; char name[SCENE_NAME_MAX]; int parent; node_property_value_t *values; int count; } clip_node_t;
static clip_node_t g_clip[SCENE_DATA_MAX_NODES];
static int g_clip_count = 0;

static void clip_clear(void) {
    for (int i = 0; i < g_clip_count; i++) {
        const node_registry_entry_t *entry = node_registry_get(g_clip[i].type);
        if (g_clip[i].values != NULL) {
            for (int p = 0; p < g_clip[i].count; p++)
                if (entry != NULL && entry->properties[p].type == NODE_PROPERTY_TYPE_STRING) free(g_clip[i].values[p].s);
            free(g_clip[i].values);
        }
    }
    memset(g_clip, 0, sizeof(g_clip));
    g_clip_count = 0;
}

int scene_data_clipboard_has(void) { return g_clip_count > 0; }

int scene_data_copy(int index) {
    if (index < 0 || index >= g_node_count) return 0;
    int list[SCENE_DATA_MAX_NODES];
    int count = collect_subtree(index, list);
    clip_clear();
    for (int i = 0; i < count; i++) {
        const scene_node_t *src = &g_nodes[list[i]];
        const node_registry_entry_t *entry = node_registry_get(src->type);
        clip_node_t *dst = &g_clip[g_clip_count++];
        dst->type = src->type;
        snprintf(dst->name, sizeof(dst->name), "%s", src->name);
        dst->parent = -1;
        for (int k = 0; k < i; k++) if (list[k] == src->parent_index) dst->parent = k;
        dst->count = src->value_count;
        if (src->value_count > 0) {
            dst->values = calloc((size_t)src->value_count, sizeof(*dst->values));
            if (dst->values == NULL) { dst->count = 0; continue; }
            for (int p = 0; p < src->value_count; p++) {
                dst->values[p] = src->values[p];
                if (entry != NULL && entry->properties[p].type == NODE_PROPERTY_TYPE_STRING)
                    dst->values[p].s = strdup(src->values[p].s ? src->values[p].s : "");
            }
        }
    }
    return 1;
}

int scene_data_cut(int index) {
    if (!scene_data_copy(index)) return 0;
    scene_data_delete(index);
    return 1;
}

int scene_data_paste(int parent_index) {
    if (g_clip_count == 0 || g_node_count + g_clip_count > SCENE_DATA_MAX_NODES) return 0;
    if (parent_index < 0 || parent_index >= g_node_count) parent_index = g_node_count > 0 ? 0 : -1;
    int first = g_node_count;
    for (int i = 0; i < g_clip_count; i++) {
        int parent = g_clip[i].parent >= 0 ? first + g_clip[i].parent : parent_index;
        scene_node_t *node = scene_data_create_raw(g_clip[i].type, g_clip[i].name, parent);
        if (node == NULL) return 0;
        const node_registry_entry_t *entry = node_registry_get(node->type);
        for (int p = 0; p < node->value_count && p < g_clip[i].count; p++) {
            if (entry != NULL && entry->properties[p].type == NODE_PROPERTY_TYPE_STRING) {
                free(node->values[p].s);
                node->values[p].s = strdup(g_clip[i].values[p].s ? g_clip[i].values[p].s : "");
            } else node->values[p] = g_clip[i].values[p];
        }
    }
    scene_data_rename(first, g_nodes[first].name);
    scene_data_select_index(first);
    current_scene_mark_dirty();
    return 1;
}

int scene_data_duplicate(int index) {
    if (index < 0 || index >= g_node_count || !scene_data_copy(index)) return 0;
    return scene_data_paste(g_nodes[index].parent_index);
}

static const node_registry_entry_t *scene_data_find_type(const char *name) {
    for (int i = 0; i < node_registry_count(); i++) {
        const node_registry_entry_t *entry = node_registry_get_by_index(i);
        if (entry != NULL && strcmp(entry->name, name) == 0) return entry;
    }
    return NULL;
}

int scene_data_serialize(char *buffer, int buffer_size) {
    int pos = 0, written;
#define APPEND(...) do { written = snprintf(buffer + pos, (pos < buffer_size) ? (size_t)(buffer_size - pos) : 0, __VA_ARGS__); if (written < 0) return -1; pos += written; } while (0)
    APPEND("scene_version=1\n");
    APPEND("node_count=%d\n\n", g_node_count);
    for (int i = 0; i < g_node_count; i++) {
        const node_registry_entry_t *entry = node_registry_get(g_nodes[i].type);
        APPEND("[node %d]\n", i);
        APPEND("type=%s\n", entry ? entry->name : "");
        APPEND("name=%s\n", g_nodes[i].name);
        APPEND("parent=%d\n", g_nodes[i].parent_index);
        if (entry != NULL) for (int p = 0; p < g_nodes[i].value_count; p++) {
            const node_property_t *prop = &entry->properties[p];
            int different = 0;
            if (prop->type == NODE_PROPERTY_TYPE_FLOAT) different = g_nodes[i].values[p].f != prop->default_value.f;
            else if (prop->type == NODE_PROPERTY_TYPE_INT) different = g_nodes[i].values[p].i != prop->default_value.i;
            else different = strcmp(g_nodes[i].values[p].s ? g_nodes[i].values[p].s : "", prop->default_value.s ? prop->default_value.s : "") != 0;
            if (!different) continue;
            if (prop->type == NODE_PROPERTY_TYPE_FLOAT) APPEND("prop:%s=%.6f\n", prop->name, g_nodes[i].values[p].f);
            else if (prop->type == NODE_PROPERTY_TYPE_INT) APPEND("prop:%s=%d\n", prop->name, g_nodes[i].values[p].i);
            else APPEND("prop:%s=%s\n", prop->name, g_nodes[i].values[p].s ? g_nodes[i].values[p].s : "");
        }
        APPEND("\n");
    }
#undef APPEND
    return (pos < buffer_size) ? pos : -1;
}

int scene_data_deserialize(const char *buffer) {
    if (buffer == NULL) return 0;
    scene_data_clear();
    const char *p = buffer;
    int current = -1;
    char line[1024];
    while (*p != '\0') {
        int len = 0;
        while (p[len] != '\0' && p[len] != '\n' && len < (int)sizeof(line) - 1) len++;
        memcpy(line, p, (size_t)len); line[len] = '\0'; p += len; if (*p == '\n') p++;
        if (line[0] == '\0' || strncmp(line, "scene_version=", 14) == 0 || strncmp(line, "node_count=", 11) == 0) continue;
        if (line[0] == '[') { current++; continue; }
        char *equals = strchr(line, '='); if (equals == NULL) continue;
        *equals = '\0'; const char *key = line; const char *value = equals + 1;
        if (strcmp(key, "type") == 0) {
            const node_registry_entry_t *entry = scene_data_find_type(value);
            if (entry == NULL || scene_data_create_raw(entry->type, "", -1) == NULL) { scene_data_clear(); return 0; }
        } else if (current < 0 || current >= g_node_count) { scene_data_clear(); return 0; }
        else if (strcmp(key, "name") == 0) {
            strncpy(g_nodes[current].name, value, sizeof(g_nodes[current].name) - 1); g_nodes[current].name[sizeof(g_nodes[current].name) - 1] = '\0';
        } else if (strcmp(key, "parent") == 0) {
            g_nodes[current].parent_index = atoi(value);
            if (g_nodes[current].parent_index >= 0 && g_nodes[current].parent_index < g_node_count) g_nodes[g_nodes[current].parent_index].expanded = 1;
        } else if (strncmp(key, "prop:", 5) == 0) {
            scene_node_t *node = &g_nodes[current]; const node_registry_entry_t *entry = node_registry_get(node->type);
            if (entry != NULL) for (int pi = 0; pi < node->value_count; pi++) if (strcmp(entry->properties[pi].name, key + 5) == 0) {
                if (entry->properties[pi].type == NODE_PROPERTY_TYPE_FLOAT) node->values[pi].f = (float)atof(value);
                else if (entry->properties[pi].type == NODE_PROPERTY_TYPE_INT) node->values[pi].i = atoi(value);
                else { free(node->values[pi].s); node->values[pi].s = strdup(value); }
                break;
            }
        }
    }
    g_selected_index = -1;
    return 1;
}

void scene_data_shutdown(void) { scene_data_clear(); }
