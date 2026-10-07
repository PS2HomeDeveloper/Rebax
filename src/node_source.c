#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "node_source.h"
#include "rebax_paths.h"
#include "rebax_fs.h"
#include "export_internal.h"

#if defined(__ANDROID__)
#include "asset_files.h"
#define NODE_ASSET_DIR "ps2/sdk/nodes"

int node_source_list(node_source_name_cb callback, void *user) {
    return asset_file_list(NODE_ASSET_DIR "/", callback, user);
}

int node_source_copy(const char *name, const char *dest_path) {
    char asset[1600];
    snprintf(asset, sizeof(asset), NODE_ASSET_DIR "/%s", name);
    return asset_file_copy(asset, dest_path);
}

char *node_source_read(const char *name, long *out_size) {
    char asset[1600];
    size_t size = 0;
    snprintf(asset, sizeof(asset), NODE_ASSET_DIR "/%s", name);
    char *data = (char *)asset_file_read(asset, &size);
    if (data && out_size) *out_size = (long)size;
    return data;
}

#else
#include <dirent.h>
#pragma GCC diagnostic ignored "-Wformat-truncation"

int node_source_list(node_source_name_cb callback, void *user) {
    DIR *dir = opendir(rebax_node_resources_dir());
    if (dir == NULL) return 0;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) callback(entry->d_name, user);
    closedir(dir);
    return 1;
}

int node_source_copy(const char *name, const char *dest_path) {
    char src[1600];
    snprintf(src, sizeof(src), "%s/%s", rebax_node_resources_dir(), name);
    return rebax_fs_copy_file(src, dest_path);
}

char *node_source_read(const char *name, long *out_size) {
    char src[1600];
    snprintf(src, sizeof(src), "%s/%s", rebax_node_resources_dir(), name);
    return export_read_whole_file(src, out_size);
}
#endif
