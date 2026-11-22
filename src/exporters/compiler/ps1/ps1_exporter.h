/*
 *  ____  _____ ____    _    __  __
 * |  _ \| ____| __ )  / \   \ \/ /
 * | |_) |  _| |  _ \ / _ \   \  / 
 * |  _ <| |___| |_) / ___ \  /  \ 
 * |_| \_\_____|____/_/   \_\/_/\_\
 *
 * Rebax Engine
 * Copyright (c) 2026 PS1HomeDeveloper
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef PS1_EXPORTER_H
#define PS1_EXPORTER_H

typedef enum {
    PS1_EXPORT_STATUS_IDLE,
    PS1_EXPORT_STATUS_RUNNING,
    PS1_EXPORT_STATUS_SUCCESS,
    PS1_EXPORT_STATUS_FAILED
} ps1_export_status_t;

int ps1_export_start(const char *exe_name, int is_release, const char *output_dir);

void ps1_export_update(void);

ps1_export_status_t ps1_export_get_status(void);

const char *ps1_export_poll_next_line(void);

void ps1_export_cancel(void);

#endif
