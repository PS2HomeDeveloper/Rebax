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

#ifndef FILE_MANAGER_H
#define FILE_MANAGER_H

typedef enum {
    FILE_MANAGER_ROOT_ASSETS,
    FILE_MANAGER_ROOT_SOURCE,
    FILE_MANAGER_ROOT_DEVICE
} file_manager_root_t;

typedef enum {
    FILE_MANAGER_MODE_PICK_FILE,
    FILE_MANAGER_MODE_PICK_FOLDER,
    FILE_MANAGER_MODE_SAVE_FILE
} file_manager_mode_t;

typedef void (*file_manager_callback_t)(const char *picked_path, void *user_data);

void file_manager_open(file_manager_root_t root, file_manager_mode_t mode,
                         const char **extensions, int extension_count,
                         file_manager_callback_t callback, void *user_data);

void file_manager_open_save(file_manager_root_t root, const char *default_filename,
                              file_manager_callback_t callback, void *user_data);

int file_manager_is_open(void);

void file_manager_update(int window_w, int window_h);

void file_manager_draw(int window_w, int window_h);

void file_manager_shutdown(void);

#endif
