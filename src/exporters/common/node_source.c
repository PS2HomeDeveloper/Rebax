#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "exporters/common/node_source.h"
#include "filesystem/storage_paths.h"
#include "filesystem/rebax_fs.h"
#include "exporters/common/export_internal.h"

#pragma GCC diagnostic ignored "-Wformat-truncation"

static const char *const NODE_SOURCE_DIRS[] = {
    "nodes/src", "api/src", "nodes/include", "api/include"
};
#define NODE_SOURCE_DIR_COUNT ((int)(sizeof(NODE_SOURCE_DIRS) / sizeof(NODE_SOURCE_DIRS[0])))

static char g_sdk_name[64] = "ps2";

void node_source_use(const char *sdk_name) {
    snprintf(g_sdk_name, sizeof(g_sdk_name), "%s", sdk_name);
}

#if defined(__ANDROID__)
#include "filesystem/assets.h"

static void asset_path(char *out, size_t cap, int dir, const char *name) {
    snprintf(out, cap, "sdks/rebax/%s/%s/%s", g_sdk_name, NODE_SOURCE_DIRS[dir], name);
}

static void common_asset_path(char *out, size_t cap, int dir, const char *name) {
    snprintf(out, cap, "sdks/rebax/common/%s/%s", NODE_SOURCE_DIRS[dir] + (strncmp(NODE_SOURCE_DIRS[dir], "nodes/", 6) == 0 ? 6 : 4), name);
}

int node_source_list(node_source_name_cb callback, void *user) {
    char prefix[1600];
    snprintf(prefix, sizeof(prefix), "sdks/rebax/%s/%s/", g_sdk_name, NODE_SOURCE_DIRS[0]);
    return asset_file_list(prefix, callback, user);
}

int node_source_copy(const char *name, const char *dest_path) {
    char asset[1600];
    for (int d = 0; d < NODE_SOURCE_DIR_COUNT; d++) {
        asset_path(asset, sizeof(asset), d, name);
        if (asset_file_copy(asset, dest_path)) return 1;
        common_asset_path(asset, sizeof(asset), d, name);
        if (asset_file_copy(asset, dest_path)) return 1;
    }
    return 0;
}

char *node_source_read(const char *name, long *out_size) {
    char asset[1600];
    for (int d = 0; d < NODE_SOURCE_DIR_COUNT; d++) {
        size_t size = 0;
        asset_path(asset, sizeof(asset), d, name);
        char *data = (char *)asset_file_read(asset, &size);
        if (!data) {
            common_asset_path(asset, sizeof(asset), d, name);
            data = (char *)asset_file_read(asset, &size);
        }
        if (data) {
            if (out_size) *out_size = (long)size;
            return data;
        }
    }
    return NULL;
}

#else
#include <dirent.h>

static int find_file(const char *name, char *out, size_t cap) {
    for (int d = 0; d < NODE_SOURCE_DIR_COUNT; d++) {
        snprintf(out, cap, "%s/%s/%s/%s", rebax_node_resources_dir(), g_sdk_name, NODE_SOURCE_DIRS[d], name);
        if (rebax_fs_exists(out)) return 1;
        snprintf(out, cap, "%s/common/%s/%s", rebax_node_resources_dir(), NODE_SOURCE_DIRS[d] + (strncmp(NODE_SOURCE_DIRS[d], "nodes/", 6) == 0 ? 6 : 4), name);
        if (rebax_fs_exists(out)) return 1;
    }
    return 0;
}

int node_source_list(node_source_name_cb callback, void *user) {
    char dir_path[1600];
    snprintf(dir_path, sizeof(dir_path), "%s/%s/%s", rebax_node_resources_dir(), g_sdk_name, NODE_SOURCE_DIRS[0]);
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
#endif
