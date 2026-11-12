#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <string.h>

#include "filesystem/rebax_fs.h"
#include "filesystem/rebax_fs_tree.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>
#endif

#define TREE_PATH_MAX 1600

static int merge_dir(const char *src, const char *dst);
static int copy_dir(const char *src, const char *dst);

#ifdef _WIN32

static int merge_dir(const char *src, const char *dst) {
    char search[TREE_PATH_MAX];
    snprintf(search, sizeof(search), "%s\\*", src);
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(search, &fd);
    if (h == INVALID_HANDLE_VALUE) return 1;
    int ok = 1;
    if (!rebax_fs_mkdir_p(dst)) ok = 0;
    do {
        if (!ok) break;
        if (strcmp(fd.cFileName, ".") == 0 || strcmp(fd.cFileName, "..") == 0) continue;
        char from[TREE_PATH_MAX], to[TREE_PATH_MAX];
        snprintf(from, sizeof(from), "%s\\%s", src, fd.cFileName);
        snprintf(to, sizeof(to), "%s\\%s", dst, fd.cFileName);
        int is_dir = (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        if (is_dir) {
            if (!rebax_fs_exists(to) && MoveFileA(from, to)) continue;
            if (!merge_dir(from, to)) ok = 0;
        } else {
            if (rebax_fs_exists(to)) continue;
            if (MoveFileA(from, to)) continue;
            if (!rebax_fs_copy_file(from, to)) ok = 0;
        }
    } while (FindNextFileA(h, &fd) != 0);
    FindClose(h);
    return ok;
}

static int same_size(const char *a, const char *b) {
    WIN32_FILE_ATTRIBUTE_DATA x, y;
    if (!GetFileAttributesExA(a, GetFileExInfoStandard, &x)) return 0;
    if (!GetFileAttributesExA(b, GetFileExInfoStandard, &y)) return 0;
    return x.nFileSizeHigh == y.nFileSizeHigh && x.nFileSizeLow == y.nFileSizeLow;
}

static int copy_dir(const char *src, const char *dst) {
    char search[TREE_PATH_MAX];
    snprintf(search, sizeof(search), "%s\\*", src);
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(search, &fd);
    if (h == INVALID_HANDLE_VALUE) return 1;
    int ok = rebax_fs_mkdir_p(dst);
    do {
        if (!ok) break;
        if (strcmp(fd.cFileName, ".") == 0 || strcmp(fd.cFileName, "..") == 0) continue;
        char from[TREE_PATH_MAX], to[TREE_PATH_MAX];
        snprintf(from, sizeof(from), "%s\\%s", src, fd.cFileName);
        snprintf(to, sizeof(to), "%s\\%s", dst, fd.cFileName);
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            if (!copy_dir(from, to)) ok = 0;
        } else if (!(rebax_fs_exists(to) && same_size(from, to))) {
            if (!rebax_fs_copy_file(from, to)) ok = 0;
        }
    } while (FindNextFileA(h, &fd) != 0);
    FindClose(h);
    return ok;
}

#else

static int exists_no_follow(const char *path) {
    struct stat st;
    return lstat(path, &st) == 0;
}

static int merge_dir(const char *src, const char *dst) {
    DIR *dir = opendir(src);
    if (dir == NULL) return 1;
    int ok = rebax_fs_mkdir_p(dst);
    struct dirent *entry;
    while (ok && (entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
        char from[TREE_PATH_MAX], to[TREE_PATH_MAX];
        snprintf(from, sizeof(from), "%s/%s", src, entry->d_name);
        snprintf(to, sizeof(to), "%s/%s", dst, entry->d_name);
        struct stat st;
        if (lstat(from, &st) != 0) { ok = 0; break; }
        int target_exists = exists_no_follow(to);
        if (S_ISDIR(st.st_mode)) {
            if (!target_exists && rename(from, to) == 0) continue;
            if (!merge_dir(from, to)) ok = 0;
        } else if (!target_exists) {
            if (rename(from, to) == 0) continue;
            if (S_ISLNK(st.st_mode)) {
                char link[TREE_PATH_MAX];
                ssize_t n = readlink(from, link, sizeof(link) - 1);
                if (n < 0) { ok = 0; break; }
                link[n] = '\0';
                if (symlink(link, to) != 0) ok = 0;
            } else {
                if (!rebax_fs_copy_file(from, to)) { ok = 0; break; }
                chmod(to, st.st_mode & 07777);
            }
        }
    }
    closedir(dir);
    return ok;
}

static int copy_dir(const char *src, const char *dst) {
    DIR *dir = opendir(src);
    if (dir == NULL) return 1;
    int ok = rebax_fs_mkdir_p(dst);
    struct dirent *entry;
    while (ok && (entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
        char from[TREE_PATH_MAX], to[TREE_PATH_MAX];
        snprintf(from, sizeof(from), "%s/%s", src, entry->d_name);
        snprintf(to, sizeof(to), "%s/%s", dst, entry->d_name);
        struct stat st, dst_st;
        if (lstat(from, &st) != 0) { ok = 0; break; }
        if (S_ISDIR(st.st_mode)) {
            if (!copy_dir(from, to)) ok = 0;
        } else if (S_ISLNK(st.st_mode)) {
            char link[TREE_PATH_MAX];
            ssize_t n = readlink(from, link, sizeof(link) - 1);
            if (n < 0) { ok = 0; break; }
            link[n] = '\0';
            if (exists_no_follow(to)) unlink(to);
            if (symlink(link, to) != 0) ok = 0;
        } else {
            if (lstat(to, &dst_st) == 0 && S_ISREG(dst_st.st_mode) && dst_st.st_size == st.st_size) {
                chmod(to, st.st_mode & 07777);
                continue;
            }
            if (exists_no_follow(to)) unlink(to);
            if (!rebax_fs_copy_file(from, to)) { ok = 0; break; }
            chmod(to, st.st_mode & 07777);
        }
    }
    closedir(dir);
    return ok;
}

#endif

int rebax_fs_merge_move(const char *src_dir, const char *dst_dir) {
    if (!src_dir || !dst_dir || !rebax_fs_is_dir(src_dir)) return 0;
    if (!merge_dir(src_dir, dst_dir)) return 0;
    return rebax_fs_remove_recursive(src_dir);
}

int rebax_fs_copy_tree(const char *src_dir, const char *dst_dir) {
    if (!src_dir || !dst_dir || !rebax_fs_is_dir(src_dir)) return 0;
    return copy_dir(src_dir, dst_dir);
}
