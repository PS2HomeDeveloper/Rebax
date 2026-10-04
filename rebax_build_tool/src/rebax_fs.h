/*
 * ============================================================
 * rebax_fs.h
 * ============================================================
 * Basic file/directory operations implemented from scratch (POSIX + Win32) -
 * no reliance on external system commands (mkdir/rm/cp/find/basename as shell
 * commands). This removes the dependency vulnerability on Unix tools that may
 * not be available at all on real Android/Windows at the end-user runtime -
 * every system call here uses the OS API directly
 * (CreateDirectoryA/RemoveDirectoryA on Windows, mkdir()/unlink()
 * on POSIX), with no fork/exec of any external process.
 *
 * .tar.xz handling (extracting bundled archives) and running the PS2
 * compiler/linker itself are separate concerns (rebax_archive.h and a
 * modification to ps2_exporter.c to invoke the compiler directly instead of
 * Makefile/make) - out of scope for this particular file.
 * ============================================================
 */

#ifndef REBAX_FS_H
#define REBAX_FS_H

/* Creates a directory and any missing parent directories (like mkdir -p) -
 * returns 1 on success or if it already existed, 0 on actual failure */
int rebax_fs_mkdir_p(const char *path);

/* Removes a file or directory (and all its contents, at any depth) - like rm -rf.
 * Returns 1 on success or if the path didn't exist (no error in that case), 0 on
 * failure to remove an existing item */
int rebax_fs_remove_recursive(const char *path);

/* Removes all contents of a directory while keeping the directory itself.
 * Returns 1 on success, 0 on failure */
int rebax_fs_remove_contents(const char *dir_path);
/* Copies a single file (raw content, byte-for-byte) - creates/replaces the destination.
 * Returns 1 on success, 0 on failure (source missing, or write failed) */
 
 
int rebax_fs_copy_file(const char *src_path, const char *dst_path);
int rebax_fs_move(const char *src_path, const char *dst_path);

/* Does the path actually exist (file or directory)? */
int rebax_fs_exists(const char *path);

/* Is the path actually a directory? (0 if missing or a regular file) */
int rebax_fs_is_dir(const char *path);

/* Walks every file inside dir_path (any depth - descends into subdirectories
 * automatically), calling callback with the full path for each file (like find).
 * Does not call callback for directories themselves, only files */
typedef void (*rebax_fs_walk_callback_t)(const char *file_path, void *user_data);
/* Enumerate immediate entries only; unlike walk_files, do not recurse. */
int rebax_fs_walk_entries(const char *dir_path, rebax_fs_walk_callback_t callback, void *user_data);
void rebax_fs_walk_files(const char *dir_path, rebax_fs_walk_callback_t callback, void *user_data);

/* Extracts the final filename from a full path (like basename) - out must
 * have sufficient capacity (use at least the same capacity as path) */
void rebax_fs_basename(const char *path, char *out, int out_size);

/* Same as rebax_fs_basename but also strips the extension (e.g. ".png") -
 * removes only the last dot and anything after it */
void rebax_fs_basename_no_ext(const char *path, char *out, int out_size);

/* Returns a pointer to the start of the extension inside path itself (after
 * the last dot, without the dot) - or NULL if there is no extension. Does not
 * copy, points inside path itself */
const char *rebax_fs_extension(const char *path);

/* ------------------------------------------------------------
 * Extracts a full .tar.xz archive into a destination folder - the only public
 * function any other file in the project needs (literally the same result as
 * "tar -xJf", with no differences in output). Internally: xz decompression
 * via the adopted xz_embedded library (the same xz decompressor used in the
 * real Linux kernel toolchain - identical quality/accuracy, no compromises),
 * then a tar format parser (USTAR + GNU longname extension for long paths)
 * implemented from scratch here (the tar format itself is simple - fixed 512
 * byte header blocks, nothing requiring an external library).
 *
 * Returns 1 on successful full extraction, 0 on failure (corrupt archive,
 * disk space, unsupported tar header format - PAX extended headers are not
 * supported currently, only ustar + GNU longname) */
int rebax_fs_extract_tar_xz(const char *xz_path, const char *dest_dir);

#endif /* REBAX_FS_H */
