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

/*
 * ============================================================
 * rebax_fs.h
 * ============================================================
 * Basic file/directory operations written from scratch (POSIX + Win32) -
 * no reliance on external system commands (mkdir/rm/cp/find/basename as
 * shell commands). This removes the dependency on Unix tools that may
 * not be available at all on real Android/Windows when the engine runs
 * on the end user's machine - every system call here goes through the
 * OS API directly (CreateDirectoryA/RemoveDirectoryA on Windows,
 * mkdir()/unlink() on POSIX, etc.), with no fork/exec of any external
 * processes.
 *
 * The .tar.xz format (extracting embedded archives) and running the
 * PS2 compiler/linker itself are separate files (rebax_archive.h and
 * modifying ps2_exporter.c to invoke the compiler directly instead of
 * Makefile/make) - out of scope for this file.
 * ============================================================
 */

#ifndef REBAX_FS_H
#define REBAX_FS_H

#include <stddef.h>

/* Creates a directory and any missing parent directories (like mkdir -p) - returns 1
 * on success or if it already existed, 0 on actual failure */
int rebax_fs_mkdir_p(const char *path);

/* Removes a file or directory (and all its contents, any depth) - like rm -rf. Returns 1
 * on success or if the path did not exist (no error in that case), 0 if
 * failed to remove an actually existing item */
int rebax_fs_remove_recursive(const char *path);

/* Copies a single file (raw contents, byte-for-byte) - creates/replaces the destination.
 * Returns 1 on success, 0 on failure (source missing, or write failed) */
int rebax_fs_copy_file(const char *src_path, const char *dst_path);
int rebax_fs_move(const char *src_path, const char *dst_path);

/* Does the path actually exist (file or directory)? */
int rebax_fs_exists(const char *path);

/* Is the path a directory? (0 if missing or a regular file) */
int rebax_fs_is_dir(const char *path);

/* Walks every file inside dir_path (any depth - descends into subdirectories
 * automatically), calling callback with the full path for each file (like find). It does not
 * call callback for directories themselves, only files */
typedef void (*rebax_fs_walk_callback_t)(const char *file_path, void *user_data);
void rebax_fs_walk_files(const char *dir_path, rebax_fs_walk_callback_t callback, void *user_data);

/* Extracts the final filename from a full path (like basename) - out must
 * have enough capacity (use at least the same capacity as path) */
void rebax_fs_basename(const char *path, char *out, int out_size);

/* Same as rebax_fs_basename but also removes the extension (".png" for example) -
 * removes only the last dot and anything after it */
void rebax_fs_basename_no_ext(const char *path, char *out, int out_size);

/* Returns a pointer to the start of the extension inside path itself (after the last dot, without the dot) - or NULL if there is no extension. Does not copy, points inside the path itself */
const char *rebax_fs_extension(const char *path);

/* Writes the whole file through a temporary file and a rename, so a crash never leaves a half written file. Returns 1 on success */
int rebax_fs_write_atomic(const char *path, const void *data, size_t size);

#endif /* REBAX_FS_H */
