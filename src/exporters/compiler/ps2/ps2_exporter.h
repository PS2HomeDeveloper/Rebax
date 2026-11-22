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

#ifndef PS2_EXPORTER_H
#define PS2_EXPORTER_H

typedef enum {
    PS2_EXPORT_STATUS_IDLE,
    PS2_EXPORT_STATUS_RUNNING,
    PS2_EXPORT_STATUS_SUCCESS,
    PS2_EXPORT_STATUS_FAILED
} ps2_export_status_t;

int ps2_export_start(const char *exe_name, int is_release, const char *output_dir);

void ps2_export_update(void);

ps2_export_status_t ps2_export_get_status(void);

const char *ps2_export_poll_next_line(void);

void ps2_export_cancel(void);

#endif
