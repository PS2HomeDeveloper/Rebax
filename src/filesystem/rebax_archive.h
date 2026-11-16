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

#ifndef REBAX_ARCHIVE_H
#define REBAX_ARCHIVE_H

typedef void (*rebax_progress_fn)(unsigned long long done, unsigned long long total, void *user);

int rebax_archive_extract_tar_xz(const char *xz_path, const char *dest_dir, rebax_progress_fn progress, void *user, char *error, unsigned long error_cap);

int rebax_archive_install(const char *xz_path, const char *dest_dir, rebax_progress_fn progress, void *user, char *error, unsigned long error_cap);

#endif
