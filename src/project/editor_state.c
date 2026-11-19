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

#include "project/editor_state.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "filesystem/rebax_fs.h"
#include "project/current_project.h"
#include "project/project_file.h"

#define STATE_FILE "editor.rebax"

static void state_path(const char *root, char *out, size_t cap) {
    snprintf(out, cap, "%s/%s/%s", root, PROJECT_META_DIR, STATE_FILE);
}

static int ends_with_rscene(const char *s) {
    size_t n = strlen(s);
    return n > 7 && strcmp(s + n - 7, ".rscene") == 0;
}

int editor_state_load_last_scene(const char *root, char *out, int cap) {
    char path[1100], text[2048];
    state_path(root, path, sizeof(path));
    FILE *f = fopen(path, "rb");
    if (f == NULL) return 0;
    size_t got = fread(text, 1, sizeof(text) - 1, f);
    fclose(f);
    text[got] = '\0';

    char *save = NULL;
    for (char *line = strtok_r(text, "\r\n", &save); line != NULL; line = strtok_r(NULL, "\r\n", &save)) {
        if (strncmp(line, "last_scene=", 11) != 0) continue;
        const char *rel = line + 11;
        size_t prefix = strlen(PROJECT_ASSETS_DIR "/");
        if (strncmp(rel, PROJECT_ASSETS_DIR "/", prefix) != 0 || !ends_with_rscene(rel)) return 0;
        if (strstr(rel, "..") != NULL || strchr(rel, '\\') != NULL || rel[0] == '/') return 0;
        char full[1200];
        snprintf(full, sizeof(full), "%s/%s", root, rel);
        struct stat st;
        if (stat(full, &st) != 0 || !S_ISREG(st.st_mode)) return 0;
        if (snprintf(out, (size_t)cap, "%s", full) >= cap) return 0;
        return 1;
    }
    return 0;
}

void editor_state_note_scene(const char *scene_path) {
    const char *root = current_project_get_path();
    if (root == NULL || scene_path == NULL) return;
    size_t root_len = strlen(root);
    if (strncmp(scene_path, root, root_len) != 0 || scene_path[root_len] != '/') return;
    const char *rel = scene_path + root_len + 1;
    size_t prefix = strlen(PROJECT_ASSETS_DIR "/");
    if (strncmp(rel, PROJECT_ASSETS_DIR "/", prefix) != 0 || !ends_with_rscene(rel)) return;

    char meta[1100];
    project_path_meta(root, meta, sizeof(meta));
    rebax_fs_mkdir_p(meta);

    char text[1200], path[1100];
    int n = snprintf(text, sizeof(text), "format=1\nlast_scene=%s\n", rel);
    if (n < 0 || n >= (int)sizeof(text)) return;
    state_path(root, path, sizeof(path));
    rebax_fs_write_atomic(path, text, (size_t)n);
}
