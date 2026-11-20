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

#include "exporters/common/node_source.h"
#include "filesystem/rebax_fs.h"
#include "exporters/common/export_internal.h"

#pragma GCC diagnostic ignored "-Wformat-truncation"

static const char *const NODE_SOURCE_DIRS[] = {
    "nodes/src", "api/src", "nodes/include", "api/include"
};
#define NODE_SOURCE_DIR_COUNT ((int)(sizeof(NODE_SOURCE_DIRS) / sizeof(NODE_SOURCE_DIRS[0])))

static char g_sdk_name[64] = "ps2";
static char g_runtime_root[1536];

void node_source_use(const char *sdk_name, const char *sdk_root) {
    snprintf(g_sdk_name, sizeof(g_sdk_name), "%s", sdk_name);
    snprintf(g_runtime_root, sizeof(g_runtime_root), "%s/rebax", sdk_root);
}

int node_source_is_modern(void) {
    return strcmp(g_sdk_name, "modern") == 0;
}

int node_source_is_ps1(void) {
    return strcmp(g_sdk_name, "ps1") == 0;
}

#include <dirent.h>

static int find_file(const char *name, char *out, size_t cap) {
    for (int d = 0; d < NODE_SOURCE_DIR_COUNT; d++) {
        snprintf(out, cap, "%s/%s/%s/%s", g_runtime_root, g_sdk_name, NODE_SOURCE_DIRS[d], name);
        if (rebax_fs_exists(out)) return 1;
        snprintf(out, cap, "%s/common/%s/%s", g_runtime_root, NODE_SOURCE_DIRS[d] + (strncmp(NODE_SOURCE_DIRS[d], "nodes/", 6) == 0 ? 6 : 4), name);
        if (rebax_fs_exists(out)) return 1;
    }
    return 0;
}

int node_source_list(node_source_name_cb callback, void *user) {
    char dir_path[1600];
    snprintf(dir_path, sizeof(dir_path), "%s/%s/%s", g_runtime_root, g_sdk_name, NODE_SOURCE_DIRS[0]);
    DIR *dir = opendir(dir_path);
    if (dir == NULL) return 0;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) callback(entry->d_name, user);
    closedir(dir);
    return 1;
}

int node_source_copy(const char *name, const char *dest_path) {
    char src[1600];
    if (!find_file(name, src, sizeof(src))) return 0;
    return rebax_fs_copy_file(src, dest_path);
}

char *node_source_read(const char *name, long *out_size) {
    char src[1600];
    if (!find_file(name, src, sizeof(src))) return NULL;
    return export_read_whole_file(src, out_size);
}
