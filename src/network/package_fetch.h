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

#ifndef REBAX_PACKAGE_FETCH_H
#define REBAX_PACKAGE_FETCH_H

#include "filesystem/rebax_archive.h"

typedef struct {
    rebax_progress_fn on_download;
    rebax_progress_fn on_extract;
    void (*on_warning)(const char *text, void *user);
    void *user;
    const volatile int *cancel;
} rebax_fetch_hooks_t;

int rebax_package_download(const char *base_url, const char *file_name, const char *work_dir, const char *dest_path, const rebax_fetch_hooks_t *hooks, char *error, unsigned long error_cap);

int rebax_package_fetch(const char *base_url, const char *file_name, const char *work_dir, const char *extract_dir, const rebax_fetch_hooks_t *hooks, char *error, unsigned long error_cap);

#endif
