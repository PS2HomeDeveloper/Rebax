/*
 *  ____  _____ ____    _    __  __
 * |  _ \| ____| __ )  / \   \ \/ /
 * | |_) | _| | _ \ / _ \   \  /
 * |  _ <| |___| |_) / ___ \  /  \
 * |_| \_\_____|____/_/   \_\/_/\_\
 *
 * Rebax Engine
 * Copyright (c) 2026 PS2HomeDeveloper
 *
 * Licensed under the MIT License.
 * See the LICENSE file for details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "filesystem/storage_volumes.h"
#include "filesystem/rebax_fs.h"

#if defined(_WIN32)
#include <windows.h>
#else
#include <dirent.h>
#endif

static int add(storage_volume_t *out, int count, int max, const char *path, const char *label) {
    if (count >= max) return count;
    for (int i = 0; i < count; i++) if (strcmp(out[i].path, path) == 0) return count;
    snprintf(out[count].path, sizeof(out[count].path), "%s", path);
    snprintf(out[count].label, sizeof(out[count].label), "%s", label);
    return count + 1;
}

#if !defined(_WIN32)
static int scan(storage_volume_t *out, int count, int max, const char *dir, const char *prefix, const char *skip) {
    DIR *d = opendir(dir);
    if (!d) return count;
    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        if (e->d_name[0] == '.') continue;
        if (skip && strcmp(e->d_name, skip) == 0) continue;
        char full[512], label[96];
        snprintf(full, sizeof(full), "%s/%s", dir, e->d_name);
        if (!rebax_fs_is_dir(full)) continue;
        snprintf(label, sizeof(label), "%s%s", prefix, e->d_name);
        count = add(out, count, max, full, label);
    }
    closedir(d);
    return count;
}
#endif

int storage_volumes_list(storage_volume_t *out, int max) {
    int n = 0;
#if defined(_WIN32)
    DWORD mask = GetLogicalDrives();
    const char *profile = getenv("USERPROFILE");
    if (profile) { char p[512]; snprintf(p, sizeof(p), "%s", profile); for (char *c = p; *c; c++) if (*c == '\\') *c = '/'; n = add(out, n, max, p, "Home"); }
    for (int i = 0; i < 26; i++) {
        if (!(mask & (1u << i))) continue;
        char path[8], label[16];
        snprintf(path, sizeof(path), "%c:/", 'A' + i);
        UINT type = GetDriveTypeA(path);
        snprintf(label, sizeof(label), "%s (%c:)", type == DRIVE_REMOVABLE ? "USB" : "Drive", 'A' + i);
        n = add(out, n, max, path, label);
    }
#elif defined(__ANDROID__)
    n = add(out, n, max, "/storage/emulated/0", "Internal storage");
    n = scan(out, n, max, "/storage", "USB / SD card ", "emulated");
    n = scan(out, n, max, "/mnt/media_rw", "USB ", NULL);
#else
    const char *home = getenv("HOME");
    const char *user = getenv("USER");
    if (home && rebax_fs_is_dir(home)) n = add(out, n, max, home, "Home");
#if defined(__APPLE__)
    n = scan(out, n, max, "/Volumes", "", NULL);
#else
    char path[512];
    n = scan(out, n, max, "/media", "", user);
    if (user) {
        snprintf(path, sizeof(path), "/media/%s", user); n = scan(out, n, max, path, "", NULL);
        snprintf(path, sizeof(path), "/run/media/%s", user); n = scan(out, n, max, path, "", NULL);
    }
    n = scan(out, n, max, "/mnt", "", NULL);
#endif
    n = add(out, n, max, "/", "Computer");
#endif
    return n;
}
