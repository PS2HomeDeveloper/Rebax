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

#include "project/project_file.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "filesystem/rebax_fs.h"

#define ENGINE_VERSION "1.0.0"
#define FILE_MAX 8192

static int is_dir(const char *path) {
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

static void set_error(char *error, int cap, const char *text) {
    if (error != NULL && cap > 0) snprintf(error, (size_t)cap, "%s", text);
}

void project_settings_defaults(project_settings_t *settings, const char *name, project_type_t type, dev_system_t dev_system) {
    memset(settings, 0, sizeof(*settings));
    settings->format = PROJECT_FORMAT_VERSION;
    snprintf(settings->name, sizeof(settings->name), "%s", name != NULL ? name : "");
    snprintf(settings->engine_version, sizeof(settings->engine_version), "%s", ENGINE_VERSION);
    settings->type = type;
    settings->dev_system = dev_system;
    settings->export_method = EXPORT_METHOD_COMPILER;
    settings->platforms = project_type_default_platforms(type);
    settings->display_width = 640;
    settings->display_height = 448;
}

const char *project_name_error(const char *name) {
    if (name == NULL || name[0] == '\0') return "Enter a project name.";
    size_t len = strlen(name);
    if (len > PROJECT_NAME_MAX) return "The project name is too long.";
    if (name[0] == ' ' || name[len - 1] == ' ' || name[len - 1] == '.' || name[0] == '.')
        return "The name cannot start or end with a space or a dot.";
    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)name[i];
        if (c < 32 || strchr("/\\:*?\"<>|", c) != NULL) return "The name contains a character that is not allowed.";
    }
    return NULL;
}

int project_path_assets(const char *root, char *out, int cap) {
    return snprintf(out, (size_t)cap, "%s/%s", root, PROJECT_ASSETS_DIR) < cap;
}

int project_path_meta(const char *root, char *out, int cap) {
    return snprintf(out, (size_t)cap, "%s/%s", root, PROJECT_META_DIR) < cap;
}

int project_file_save(const char *root, const project_settings_t *s) {
    char path[1100], platforms[96], text[1024];
    snprintf(path, sizeof(path), "%s/%s", root, PROJECT_FILE_NAME);
    project_platforms_format(s->platforms, platforms, sizeof(platforms));
    int n = snprintf(text, sizeof(text),
                     "project_format=%d\nname=%s\nengine_version=%s\nproject_type=%s\ndev_system=%s\nexport_method=%s\nplatforms=%s\ndisplay_width=%d\ndisplay_height=%d\n",
                     s->format, s->name, s->engine_version, project_type_id(s->type), dev_system_id(s->dev_system),
                     export_method_id(s->export_method), platforms, s->display_width, s->display_height);
    if (n < 0 || n >= (int)sizeof(text)) return 0;
    return rebax_fs_write_atomic(path, text, (size_t)n);
}

static void folder_name(const char *root, char *out, size_t cap) {
    const char *slash = strrchr(root, '/');
    const char *back = strrchr(root, '\\');
    if (back != NULL && (slash == NULL || back > slash)) slash = back;
    snprintf(out, cap, "%s", slash != NULL ? slash + 1 : root);
}

int project_file_load(const char *root, project_settings_t *s, char *error, int error_cap) {
    if (root == NULL || !is_dir(root)) { set_error(error, error_cap, "The project folder is unavailable."); return 0; }

    char path[1100];
    snprintf(path, sizeof(path), "%s/%s", root, PROJECT_FILE_NAME);
    FILE *f = fopen(path, "rb");
    if (f == NULL) { set_error(error, error_cap, "project.rebax is missing in this folder."); return 0; }
    char *text = malloc(FILE_MAX + 1);
    if (text == NULL) { fclose(f); set_error(error, error_cap, "Out of memory."); return 0; }
    size_t got = fread(text, 1, FILE_MAX + 1, f);
    fclose(f);
    if (got > FILE_MAX) { free(text); set_error(error, error_cap, "project.rebax is not a valid project file."); return 0; }
    text[got] = '\0';

    project_settings_defaults(s, "", PROJECT_TYPE_MODERN, DEV_SYSTEM_NODES);
    s->format = 0;
    int saw_type = 0, bad = 0;
    char *save = NULL;
    for (char *line = strtok_r(text, "\r\n", &save); line != NULL; line = strtok_r(NULL, "\r\n", &save)) {
        char *eq = strchr(line, '=');
        if (eq == NULL) continue;
        *eq = '\0';
        const char *key = line, *value = eq + 1;
        if (strcmp(key, "project_format") == 0) s->format = atoi(value);
        else if (strcmp(key, "name") == 0) snprintf(s->name, sizeof(s->name), "%s", value);
        else if (strcmp(key, "engine_version") == 0) snprintf(s->engine_version, sizeof(s->engine_version), "%s", value);
        else if (strcmp(key, "project_type") == 0) { if (!project_type_parse(value, &s->type)) bad = 1; saw_type = 1; }
        else if (strcmp(key, "dev_system") == 0) { if (!dev_system_parse(value, &s->dev_system)) bad = 1; }
        else if (strcmp(key, "export_method") == 0) { if (!export_method_parse(value, &s->export_method)) bad = 1; }
        else if (strcmp(key, "platforms") == 0) s->platforms = project_platforms_parse(value);
        else if (strcmp(key, "display_width") == 0) s->display_width = atoi(value);
        else if (strcmp(key, "display_height") == 0) s->display_height = atoi(value);
    }
    free(text);

    if (bad) { set_error(error, error_cap, "project.rebax contains an unknown value."); return 0; }
    if (s->format > PROJECT_FORMAT_VERSION) { set_error(error, error_cap, "This project was made by a newer version of Rebax Engine."); return 0; }
    if (s->format != PROJECT_FORMAT_VERSION || !saw_type) { set_error(error, error_cap, "This is not a valid Rebax project."); return 0; }
    if (project_name_error(s->name) != NULL) folder_name(root, s->name, sizeof(s->name));
    if (project_name_error(s->name) != NULL) snprintf(s->name, sizeof(s->name), "Project");
    if (s->display_width <= 0 || s->display_width > 16384) s->display_width = 640;
    if (s->display_height <= 0 || s->display_height > 16384) s->display_height = 448;
    if (!dev_system_is_available(s->dev_system)) { set_error(error, error_cap, "This project uses a development system that is not available yet."); return 0; }

    char assets[1100];
    project_path_assets(root, assets, sizeof(assets));
    if (!is_dir(assets)) { set_error(error, error_cap, "The assets folder is missing in this project."); return 0; }
    return 1;
}
