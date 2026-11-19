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

#include <ctype.h>
#include <string.h>
#include "filesystem/path_utils.h"

int path_utils_compare_names(const void *a, const void *b) {
    return strcmp(*(const char * const *)a, *(const char * const *)b);
}

int path_utils_compare_names_ci(const void *a, const void *b) {
    const unsigned char *left = (const unsigned char *)*(const char * const *)a;
    const unsigned char *right = (const unsigned char *)*(const char * const *)b;
    while (*left != '\0' && *right != '\0') {
        int diff = tolower(*left) - tolower(*right);
        if (diff != 0) return diff;
        left++; right++;
    }
    return tolower(*left) - tolower(*right);
}

const char *path_utils_basename(const char *path) {
    if (path == NULL) return "";
    const char *slash = strrchr(path, '/');
    return (slash != NULL) ? slash + 1 : path;
}

int path_utils_has_suffix(const char *path, const char *suffix) {
    size_t path_len = strlen(path);
    size_t suffix_len = strlen(suffix);
    return suffix_len <= path_len && strcmp(path + path_len - suffix_len, suffix) == 0;
}

int path_utils_has_suffix_ci(const char *path, const char *suffix) {
    size_t path_len = strlen(path);
    size_t suffix_len = strlen(suffix);
    if (suffix_len > path_len) return 0;
    const unsigned char *a = (const unsigned char *)path + path_len - suffix_len;
    const unsigned char *b = (const unsigned char *)suffix;
    for (; *b != '\0'; a++, b++) {
        if (tolower(*a) != tolower(*b)) return 0;
    }
    return 1;
}
