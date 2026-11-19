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

/* Shared name/path utilities without UI dependency. */
#ifndef PATH_UTILS_H
#define PATH_UTILS_H

int path_utils_compare_names(const void *a, const void *b);
int path_utils_compare_names_ci(const void *a, const void *b);
const char *path_utils_basename(const char *path);
int path_utils_has_suffix(const char *path, const char *suffix);
int path_utils_has_suffix_ci(const char *path, const char *suffix);

#endif /* PATH_UTILS_H */
