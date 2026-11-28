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
#include <ctype.h>
#include <math.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include "project/current_project.h"
#include "exporters/common/export_internal.h"
#include "project/current_project.h"

#define MAX_USED_TYPES EXPORT_MAX_USED_TYPES
#define MAX_IMAGE_PATHS EXPORT_MAX_IMAGE_PATHS
#define MAX_PROPS_PER_NODE EXPORT_MAX_PROPS_PER_NODE
#define MAX_NODES_PER_SCENE EXPORT_MAX_NODES_PER_SCENE
typedef export_rscene_kv_t rscene_kv_t;
typedef export_rscene_node_t rscene_node_t;

static void add_unique(char list[][64], int *count, int max, const char *value) {
    for (int i = 0; i < *count; i++) {
        if (strcmp(list[i], value) == 0) return;
    }
    if (*count >= max) return;
    strncpy(list[*count], value, 63);
    list[*count][63] = '\0';
    (*count)++;
}

const char *export_public_path(const char *path) {
    if (path == NULL) return "";
    const char *root = current_project_get_path();
    if (root != NULL && root[0] != '\0') {
        size_t length = strlen(root);
        if (strncmp(path, root, length) == 0 && (path[length] == '/' || path[length] == '\\')) {
            return path + length + 1;
        }
    }
    return path;
}

void export_scene_add_image_path(const char *value) {
    export_scene_add_image_path_with_raw(value, 0, 0, 0);
}

void export_scene_add_image_path_with_raw(const char *value, int raw_width,
                                          int raw_height, int raw_format) {
    if (value == NULL || value[0] == '\0') return;
    for (int i = 0; i < g_image_paths_count; i++) {
        if (strcmp(g_image_paths[i], value) != 0) continue;
        if (raw_width > 0 && raw_height > 0) {
            g_image_specs[i].raw_width = raw_width;
            g_image_specs[i].raw_height = raw_height;
            g_image_specs[i].raw_format = raw_format;
        }
        return;
    }
    if (g_image_paths_count >= MAX_IMAGE_PATHS) return;
    strncpy(g_image_paths[g_image_paths_count], value, 1023);
    g_image_paths[g_image_paths_count][1023] = '\0';
    strncpy(g_image_specs[g_image_paths_count].path, value,
            sizeof(g_image_specs[g_image_paths_count].path) - 1);
    g_image_specs[g_image_paths_count].path[
        sizeof(g_image_specs[g_image_paths_count].path) - 1] = '\0';
    g_image_specs[g_image_paths_count].raw_width = raw_width;
    g_image_specs[g_image_paths_count].raw_height = raw_height;
    g_image_specs[g_image_paths_count].raw_format = raw_format;
    g_image_paths_count++;
}

char *export_read_whole_file(const char *path, long *out_size) {
    FILE *f = fopen(path, "rb");
    if (f == NULL) return NULL;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (size < 0) { fclose(f); return NULL; }

    char *buf = malloc((size_t)size + 1);
    if (buf == NULL) { fclose(f); return NULL; }

    size_t read_bytes = fread(buf, 1, (size_t)size, f);
    fclose(f);
    buf[read_bytes] = '\0';
    if (out_size) *out_size = (long)read_bytes;
    return buf;
}

void export_scene_walk_rscene_files(const char *dir_path, void (*on_file)(const char *path)) {
    DIR *dir = opendir(dir_path);
    if (dir == NULL) return;

    char (*names)[256] = (char (*)[256])malloc(256 * 256);
    if (names == NULL) { closedir(dir); return; }
    size_t count = 0;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL && count < 256) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
        strncpy(names[count], entry->d_name, sizeof(names[count]) - 1);
        names[count][sizeof(names[count]) - 1] = '\0';
        count++;
    }
    closedir(dir);

    for (size_t i = 1; i < count; i++) {
        char key[256];
        memcpy(key, names[i], sizeof(key));
        size_t j = i;
        while (j > 0 && strcmp(key, names[j - 1]) < 0) {
            memcpy(names[j], names[j - 1], sizeof(names[j]));
            j--;
        }
        memcpy(names[j], key, sizeof(names[j]));
    }

    for (size_t i = 0; i < count; i++) {
        char full_path[1536];
        snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, names[i]);
        struct stat st;
        if (stat(full_path, &st) != 0) continue;
        if (S_ISDIR(st.st_mode)) {
            export_scene_walk_rscene_files(full_path, on_file);
        } else {
            size_t len = strlen(names[i]);
            if (len > 7 && strcmp(names[i] + len - 7, ".rscene") == 0) {
                on_file(full_path);
            }
        }
    }
    free(names);
}

#include "dev_systems/nodes/runtime/node_registry.h"
#include "stb_image.h"

export_rscene_node_t g_parsed_nodes[EXPORT_MAX_NODES_PER_SCENE];
int g_parsed_node_count = 0;
char g_declared_interfaces[EXPORT_MAX_USED_TYPES][64];
int g_declared_interfaces_count = 0;

static void parse_rscene_content(char *content) {
    g_parsed_node_count = 0;
    rscene_node_t *cur = NULL;

    char *line = strtok(content, "\n");
    while (line != NULL) {
        if (strncmp(line, "[node ", 6) == 0) {
            if (g_parsed_node_count < MAX_NODES_PER_SCENE) {
                cur = &g_parsed_nodes[g_parsed_node_count++];
                cur->type[0] = '\0';
                cur->name[0] = '\0';
                cur->prop_count = 0;
                cur->parent = -1;
            } else {
                cur = NULL;
            }
        } else if (cur != NULL) {
            if (strncmp(line, "type=", 5) == 0) {
                strncpy(cur->type, line + 5, sizeof(cur->type) - 1);
            } else if (strncmp(line, "parent=", 7) == 0) {
                cur->parent = atoi(line + 7);
            } else if (strncmp(line, "name=", 5) == 0) {
                strncpy(cur->name, line + 5, sizeof(cur->name) - 1);
            } else if (strncmp(line, "prop:", 5) == 0) {
                const char *eq = strchr(line + 5, '=');
                if (eq != NULL && cur->prop_count < MAX_PROPS_PER_NODE) {
                    rscene_kv_t *kv = &cur->props[cur->prop_count++];
                    size_t key_len = (size_t)(eq - (line + 5));
                    if (key_len >= sizeof(kv->key)) key_len = sizeof(kv->key) - 1;
                    memcpy(kv->key, line + 5, key_len);
                    kv->key[key_len] = '\0';
                    strncpy(kv->value, eq + 1, sizeof(kv->value) - 1);
                    kv->value[sizeof(kv->value) - 1] = '\0';
                }
            }
        }
        line = strtok(NULL, "\n");
    }
}

static const rscene_kv_t *find_prop(const rscene_node_t *node, const char *key) {
    for (int i = 0; i < node->prop_count; i++) {
        if (strcmp(node->props[i].key, key) == 0) return &node->props[i];
    }
    return NULL;
}

static float local_value(const rscene_node_t *node, const char *key, float fallback) {
    const rscene_kv_t *kv = find_prop(node, key);
    return kv != NULL ? (float)atof(kv->value) : fallback;
}

static int has_transform(const rscene_node_t *node) {
    const node_registry_entry_t *entry = NULL;
    for (int i = 0; i < node_registry_count(); i++) {
        const node_registry_entry_t *e = node_registry_get_by_index(i);
        if (e != NULL && strcmp(e->name, node->type) == 0) { entry = e; break; }
    }
    if (entry == NULL) return 0;
    for (int p = 0; p < entry->property_count; p++)
        if (strcmp(entry->properties[p].name, "Position X") == 0) return 1;
    return 0;
}

static void world_transform(int index, float *x, float *y, float *rot, float *sx, float *sy) {
    const rscene_node_t *node = &g_parsed_nodes[index];
    *x = local_value(node, "Position X", 0.0f);
    *y = local_value(node, "Position Y", 0.0f);
    *rot = local_value(node, "Rotation", 0.0f);
    *sx = local_value(node, "Scale X", 1.0f);
    *sy = local_value(node, "Scale Y", 1.0f);
    int guard = 0, parent = node->parent;
    while (parent >= 0 && parent < g_parsed_node_count && guard++ < 64) {
        const rscene_node_t *up = &g_parsed_nodes[parent];
        if (has_transform(up)) {
            float c = cosf(local_value(up, "Rotation", 0.0f)), s = sinf(local_value(up, "Rotation", 0.0f));
            float usx = local_value(up, "Scale X", 1.0f), usy = local_value(up, "Scale Y", 1.0f);
            float px = *x * usx, py = *y * usy;
            *x = local_value(up, "Position X", 0.0f) + px * c - py * s;
            *y = local_value(up, "Position Y", 0.0f) + px * s + py * c;
            *rot += local_value(up, "Rotation", 0.0f);
            *sx *= usx;
            *sy *= usy;
        }
        parent = up->parent;
    }
}

static const node_registry_entry_t *find_registry_entry_by_name(const char *name) {
    int count = node_registry_count();
    for (int i = 0; i < count; i++) {
        const node_registry_entry_t *entry = node_registry_get_by_index(i);
        if (entry != NULL && strcmp(entry->name, name) == 0) return entry;
    }
    return NULL;
}

static void interface_symbol_name(const char *type_name, char *out, size_t out_size) {
    size_t i = 0;
    for (; type_name[i] != '\0' && i < out_size - 11; i++) {
        out[i] = (char)tolower((unsigned char)type_name[i]);
    }
    out[i] = '\0';
    strncat(out, "_interface", out_size - strlen(out) - 1);
}

static float reference_screen_width(void) {
    const project_settings_t *settings = current_project_get_settings();
    return settings != NULL && settings->display_width > 0 ? (float)settings->display_width : 640.0f;
}
static float reference_screen_height(void) {
    const project_settings_t *settings = current_project_get_settings();
    return settings != NULL && settings->display_height > 0 ? (float)settings->display_height : 448.0f;
}

static void write_c_literal(FILE *f, node_property_type_t type, const char *string_value,
                             const node_property_t *default_prop, const char *property_name) {
    switch (type) {
        case NODE_PROPERTY_TYPE_FLOAT: {
            float v = (string_value != NULL) ? (float)atof(string_value)
                      : (default_prop != NULL ? default_prop->default_value.f : 0.0f);
            if (property_name != NULL && strcmp(property_name, "Position X") == 0) {
                v += reference_screen_width() / 2.0f;
            } else if (property_name != NULL && strcmp(property_name, "Position Y") == 0) {
                v += reference_screen_height() / 2.0f;
            }
            fprintf(f, "{ .f = %.6ff }", v);
            break;
        }
        case NODE_PROPERTY_TYPE_INT: {
            int v = (string_value != NULL) ? atoi(string_value)
                     : (default_prop != NULL ? default_prop->default_value.i : 0);
            fprintf(f, "{ .i = %d }", v);
            break;
        }
        case NODE_PROPERTY_TYPE_STRING: {
            const char *s = string_value != NULL ? string_value
                             : (default_prop != NULL && default_prop->default_value.s != NULL
                                ? default_prop->default_value.s : "");
            fputs("{ .s = \"", f);
            for (const char *p = export_public_path(s); *p != '\0'; p++) {
                if (*p == '"' || *p == '\\') fputc('\\', f);
                fputc(*p, f);
            }
            fputs("\" }", f);
            break;
        }
    }
}

static int ensure_interface_declared(FILE *f, const char *type_name) {
    char symbol[75];
    interface_symbol_name(type_name, symbol, sizeof(symbol));
    for (int i = 0; i < g_declared_interfaces_count; i++) {
        if (strcmp(g_declared_interfaces[i], symbol) == 0) return 1;
    }
    if (g_declared_interfaces_count >= MAX_USED_TYPES) return 0;
    fprintf(f, "extern const node_interface_t %s;\n", symbol);
    strncpy(g_declared_interfaces[g_declared_interfaces_count], symbol, 63);
    g_declared_interfaces_count++;
    return 1;
}

FILE *g_codegen_main = NULL;
FILE *g_codegen_table = NULL;

void export_scene_generate_from_file(const char *path) {
    if (g_codegen_main == NULL || g_codegen_table == NULL) return;

    long size = 0;
    char *content = export_read_whole_file(path, &size);
    if (content == NULL) {
        log_pushf("[exporter] WARNING: could not read scene file: %s", path);
        return;
    }
    log_pushf("[exporter] scene file: %s (%ld bytes)", path, size);

    const char *slash = strrchr(path, '/');
    const char *base = (slash != NULL) ? slash + 1 : path;
    char scene_name[128];
    strncpy(scene_name, base, sizeof(scene_name) - 1);
    scene_name[sizeof(scene_name) - 1] = '\0';
    char *dot = strrchr(scene_name, '.');
    if (dot != NULL) *dot = '\0';
    for (char *p = scene_name; *p != '\0'; p++) {
        if (!((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || (*p >= '0' && *p <= '9'))) {
            *p = '_';
        }
    }

    parse_rscene_content(content);

    if (g_parsed_node_count == 0) {
        log_push("[exporter] this scene has no nodes - it was probably never saved or is empty.");
        free(content);
        return;
    }
    log_pushf("[exporter] this scene has %d node(s).", g_parsed_node_count);

    int valid_node_indices[MAX_NODES_PER_SCENE];
    int valid_count = 0;

    for (int i = 0; i < g_parsed_node_count; i++) {
        rscene_node_t *node = &g_parsed_nodes[i];
        const node_registry_entry_t *entry = find_registry_entry_by_name(node->type);
        if (entry == NULL) {
            log_pushf("[exporter] WARNING: node type '%s' in scene '%s' not found in registry - skipped.",
                      node->type, scene_name);
            continue;
        }

        add_unique(g_used_types, &g_used_types_count, MAX_USED_TYPES, node->type);

        ensure_interface_declared(g_codegen_main, node->type);

        if (entry->property_count > 0) {
            fprintf(g_codegen_main, "static const node_property_value_t %s_%d_props[] = {\n",
                    scene_name, i);
            for (int p = 0; p < entry->property_count; p++) {
                const node_property_t *prop = &entry->properties[p];
                const rscene_kv_t *kv = find_prop(node, prop->name);
                char baked[64];
                const char *literal = kv != NULL ? kv->value : NULL;
                if (prop->type == NODE_PROPERTY_TYPE_FLOAT && node->parent >= 0 && has_transform(node)) {
                    float wx, wy, wr, wsx, wsy, wv = 0.0f;
                    int is_transform = 1;
                    world_transform(i, &wx, &wy, &wr, &wsx, &wsy);
                    if (strcmp(prop->name, "Position X") == 0) wv = wx;
                    else if (strcmp(prop->name, "Position Y") == 0) wv = wy;
                    else if (strcmp(prop->name, "Rotation") == 0) wv = wr;
                    else if (strcmp(prop->name, "Scale X") == 0) wv = wsx;
                    else if (strcmp(prop->name, "Scale Y") == 0) wv = wsy;
                    else is_transform = 0;
                    if (is_transform) { snprintf(baked, sizeof(baked), "%.6f", (double)wv); literal = baked; }
                }
                fprintf(g_codegen_main, "    ");
                write_c_literal(g_codegen_main, prop->type, literal, prop, prop->name);
                fprintf(g_codegen_main, ", /* %s */\n", prop->name);

                if (prop->is_asset_path && kv != NULL && kv->value[0] != '\0') {
                    const rscene_kv_t *rw = find_prop(node, "Raw Width");
                    const rscene_kv_t *rh = find_prop(node, "Raw Height");
                    const rscene_kv_t *rf = find_prop(node, "Raw Format");
                    export_scene_add_image_path_with_raw(
                        kv->value,
                        rw != NULL ? atoi(rw->value) : 0,
                        rh != NULL ? atoi(rh->value) : 0,
                        rf != NULL ? atoi(rf->value) : 0);
                }
            }
            fprintf(g_codegen_main, "};\n\n");
        }

        valid_node_indices[valid_count++] = i;
    }

    if (valid_count == 0) {
        free(content);
        return;
    }

    fprintf(g_codegen_main, "static const scene_node_entry_t %s_nodes[] = {\n", scene_name);
    for (int v = 0; v < valid_count; v++) {
        int i = valid_node_indices[v];
        rscene_node_t *node = &g_parsed_nodes[i];
        const node_registry_entry_t *entry = find_registry_entry_by_name(node->type);
        char symbol[75];
        interface_symbol_name(node->type, symbol, sizeof(symbol));
        if (entry->property_count > 0) {
            fprintf(g_codegen_main, "    { \"%s\", &%s, %s_%d_props },\n", node->name, symbol, scene_name, i);
        } else {
            fprintf(g_codegen_main, "    { \"%s\", &%s, NULL },\n", node->name, symbol);
        }
    }
    fprintf(g_codegen_main, "};\n\n");

    fprintf(g_codegen_table, "    { \"%s\", %s_nodes, %d },\n", scene_name, scene_name, valid_count);

    free(content);
}

void export_scene_reset(void) {
    g_parsed_node_count = 0;
    g_declared_interfaces_count = 0;
}
