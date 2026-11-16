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
 * Licensed under the MIT License.
 * See the LICENSE file for details.
 */

#ifndef REBAX_FS_TREE_H
#define REBAX_FS_TREE_H

int rebax_fs_merge_move(const char *src_dir, const char *dst_dir);
int rebax_fs_copy_tree(const char *src_dir, const char *dst_dir);

#endif
