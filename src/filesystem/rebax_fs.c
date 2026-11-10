/*
 * ============================================================
 * rebax_fs.c
 * ============================================================
 * See rebax_fs.h for full documentation. Every function here has two branches: POSIX
 * (Linux/Android/macOS in the future) and real Win32 (#ifdef _WIN32) -
 * no calls to system()/popen() to invoke an external shell command anywhere in this
 * file, only direct operating system calls.
 * ============================================================
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "rebax_fs.h"

#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#else
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>
#include <errno.h>
#endif

/* ------------------------------------------------------------
 * Path separators - Windows actually accepts both / and \\ , but we always write /
 * (same as the rest of the project) and recognize both when reading to be safe */
static int is_path_sep(char c) {
#ifdef _WIN32
    return c == '/' || c == '\\';
#else
    return c == '/';
#endif
}

int rebax_fs_exists(const char *path) {
#ifdef _WIN32
    DWORD attrs = GetFileAttributesA(path);
    return attrs != INVALID_FILE_ATTRIBUTES;
#else
    struct stat st;
    return stat(path, &st) == 0;
#endif
}

int rebax_fs_is_dir(const char *path) {
#ifdef _WIN32
    DWORD attrs = GetFileAttributesA(path);
    if (attrs == INVALID_FILE_ATTRIBUTES) return 0;
    return (attrs & FILE_ATTRIBUTE_DIRECTORY) != 0;
#else
    struct stat st;
    if (stat(path, &st) != 0) return 0;
    return S_ISDIR(st.st_mode);
#endif
}

static int make_one_dir(const char *path) {
#ifdef _WIN32
    if (CreateDirectoryA(path, NULL)) return 1;
    return GetLastError() == ERROR_ALREADY_EXISTS;
#else
    if (mkdir(path, 0755) == 0) return 1;
    return errno == EEXIST;
#endif
}

int rebax_fs_mkdir_p(const char *path) {
    if (path == NULL || path[0] == '\0') return 0;
    if (rebax_fs_is_dir(path)) return 1;

    char buf[1600];
    strncpy(buf, path, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    size_t len = strlen(buf);

    /* Build the path segment by segment - whenever we reach a separator, create the folder
     * up to that point (if it doesn't exist) before continuing to the next segment */
    for (size_t i = 1; i < len; i++) {
        if (is_path_sep(buf[i])) {
            char saved = buf[i];
            buf[i] = '\0';
            if (buf[0] != '\0' && !rebax_fs_is_dir(buf)) {
                if (!make_one_dir(buf)) return 0;
            }
            buf[i] = saved;
        }
    }
    if (!rebax_fs_is_dir(buf)) {
        if (!make_one_dir(buf)) return 0;
    }
    return 1;
}

int rebax_fs_move(const char *src_path, const char *dst_path) {
    if (!src_path || !dst_path || !rebax_fs_exists(src_path)) return 0;
    if (rebax_fs_exists(dst_path) && !rebax_fs_remove_recursive(dst_path)) return 0;
#ifdef _WIN32
    return MoveFileA(src_path, dst_path) != 0;
#else
    return rename(src_path, dst_path) == 0;
#endif
}

int rebax_fs_copy_file(const char *src_path, const char *dst_path) {
    FILE *src = fopen(src_path, "rb");
    if (src == NULL) return 0;
    FILE *dst = fopen(dst_path, "wb");
    if (dst == NULL) { fclose(src); return 0; }

    char buf[65536];
    size_t n;
    int ok = 1;
    while ((n = fread(buf, 1, sizeof(buf), src)) > 0) {
        if (fwrite(buf, 1, n, dst) != n) { ok = 0; break; }
    }
    if (ferror(src)) ok = 0;

    fclose(src);
    fclose(dst);
    if (!ok) remove(dst_path);
    return ok;
}

#ifdef _WIN32

int rebax_fs_remove_recursive(const char *path) {
    if (!rebax_fs_exists(path)) return 1;

    if (!rebax_fs_is_dir(path)) {
        return DeleteFileA(path) != 0;
    }

    char search[1600];
    snprintf(search, sizeof(search), "%s\\*", path);

    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(search, &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            if (strcmp(fd.cFileName, ".") == 0 || strcmp(fd.cFileName, "..") == 0) continue;
            char child[1600];
            snprintf(child, sizeof(child), "%s\\%s", path, fd.cFileName);
            if (!rebax_fs_remove_recursive(child)) {
                FindClose(h);
                return 0;
            }
        } while (FindNextFileA(h, &fd) != 0);
        FindClose(h);
    }

    return RemoveDirectoryA(path) != 0;
}

void rebax_fs_walk_files(const char *dir_path, rebax_fs_walk_callback_t callback, void *user_data) {
    char search[1600];
    snprintf(search, sizeof(search), "%s\\*", dir_path);

    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(search, &fd);
    if (h == INVALID_HANDLE_VALUE) return;

    do {
        if (strcmp(fd.cFileName, ".") == 0 || strcmp(fd.cFileName, "..") == 0) continue;
        char child[1600];
        snprintf(child, sizeof(child), "%s\\%s", dir_path, fd.cFileName);
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            rebax_fs_walk_files(child, callback, user_data);
        } else {
            callback(child, user_data);
        }
    } while (FindNextFileA(h, &fd) != 0);
    FindClose(h);
}

#else /* POSIX */

int rebax_fs_remove_recursive(const char *path) {
    struct stat link_st;
    if (lstat(path, &link_st) != 0) return 1;

    if (!S_ISDIR(link_st.st_mode)) {
        return unlink(path) == 0;
    }

    DIR *dir = opendir(path);
    if (dir == NULL) return 0;

    struct dirent *entry;
    int ok = 1;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
        char child[1600];
        snprintf(child, sizeof(child), "%s/%s", path, entry->d_name);
        if (!rebax_fs_remove_recursive(child)) { ok = 0; break; }
    }
    closedir(dir);

    if (!ok) return 0;
    return rmdir(path) == 0;
}

void rebax_fs_walk_files(const char *dir_path, rebax_fs_walk_callback_t callback, void *user_data) {
    DIR *dir = opendir(dir_path);
    if (dir == NULL) return;

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;

        char child[1600];
        snprintf(child, sizeof(child), "%s/%s", dir_path, entry->d_name);

        if (rebax_fs_is_dir(child)) {
            rebax_fs_walk_files(child, callback, user_data);
        } else {
            callback(child, user_data);
        }
    }
    closedir(dir);
}

#endif

void rebax_fs_basename(const char *path, char *out, int out_size) {
    if (path == NULL || out == NULL || out_size <= 0) return;

    const char *last = path;
    for (const char *p = path; *p != '\0'; p++) {
        if (is_path_sep(*p)) last = p + 1;
    }
    strncpy(out, last, (size_t)out_size - 1);
    out[out_size - 1] = '\0';
}

const char *rebax_fs_extension(const char *path) {
    if (path == NULL) return NULL;
    const char *dot = strrchr(path, '.');
    const char *slash1 = strrchr(path, '/');
#ifdef _WIN32
    const char *slash2 = strrchr(path, '\\');
    if (slash2 != NULL && (slash1 == NULL || slash2 > slash1)) slash1 = slash2;
#endif
    if (dot == NULL) return NULL;
    if (slash1 != NULL && dot < slash1) return NULL; /* The dot is part of a directory name, not the extension */
    if (dot[1] == '\0') return NULL; /* "file." nothing after the dot */
    return dot + 1;
}

void rebax_fs_basename_no_ext(const char *path, char *out, int out_size) {
    rebax_fs_basename(path, out, out_size);
    char *dot = strrchr(out, '.');
    if (dot != NULL) *dot = '\0';
}

