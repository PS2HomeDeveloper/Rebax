#ifndef ASSET_FILES_H
#define ASSET_FILES_H

#include <stddef.h>

typedef void (*asset_file_name_cb)(const char *name, void *user);

int asset_file_copy(const char *asset_path, const char *dest_path);
unsigned char *asset_file_read(const char *asset_path, size_t *out_size);
int asset_file_list(const char *dir_prefix, asset_file_name_cb callback, void *user);

#endif
