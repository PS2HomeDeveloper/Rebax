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

#ifndef ASSET_FILES_H
#define ASSET_FILES_H

#include <stddef.h>

typedef void (*asset_file_name_cb)(const char *name, void *user);

int asset_file_copy(const char *asset_path, const char *dest_path);
unsigned char *asset_file_read(const char *asset_path, size_t *out_size);
int asset_file_list(const char *dir_prefix, asset_file_name_cb callback, void *user);

int asset_file_list_tree(const char *dir_prefix, asset_file_name_cb callback, void *user);

#endif
