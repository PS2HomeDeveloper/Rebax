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

#ifndef REBAX_FS_H
#define REBAX_FS_H

#include <stddef.h>

int rebax_fs_mkdir_p(const char *path);

int rebax_fs_remove_recursive(const char *path);

int rebax_fs_copy_file(const char *src_path, const char *dst_path);
int rebax_fs_move(const char *src_path, const char *dst_path);

int rebax_fs_exists(const char *path);

int rebax_fs_is_dir(const char *path);

typedef void (*rebax_fs_walk_callback_t)(const char *file_path, void *user_data);
void rebax_fs_walk_files(const char *dir_path, rebax_fs_walk_callback_t callback, void *user_data);

void rebax_fs_basename(const char *path, char *out, int out_size);

void rebax_fs_basename_no_ext(const char *path, char *out, int out_size);

const char *rebax_fs_extension(const char *path);

int rebax_fs_write_atomic(const char *path, const void *data, size_t size);

#endif
