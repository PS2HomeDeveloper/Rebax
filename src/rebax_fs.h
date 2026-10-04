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

/* ------------------------------------------------------------
 * Extract a .tar.xz archive completely to a destination directory - the only
 * public function any other file in the project should need (literally the
 * same result as "tar -xJf", with no differences in output). Internally:
 * decompress xz via the adopted xz_embedded library (the same real xz
 * decompressor used in the Linux kernel - identical quality/correctness,
 * with no compromises), then parse the tar format (USTAR + GNU longname
 * extension for long paths) implemented from scratch here (the tar format
 * itself is simple - fixed 512-byte header blocks, nothing complex that
 * requires an external library).
 *
 * Returns 1 on full successful extraction, 0 on failure (corrupt archive,
 * disk space, unsupported tar header format - PAX extended headers are not
 * supported currently, only ustar + GNU longname) */
int rebax_fs_extract_tar_xz(const char *xz_path, const char *dest_dir);

#endif /* REBAX_FS_H */
