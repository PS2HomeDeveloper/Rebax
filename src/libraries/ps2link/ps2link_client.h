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

#ifndef PS2LINK_CLIENT_H
#define PS2LINK_CLIENT_H

typedef enum {
    PS2LINK_STATE_IDLE,
    PS2LINK_STATE_CONNECTING,
    PS2LINK_STATE_RUNNING,
    PS2LINK_STATE_ENDED,
    PS2LINK_STATE_FAILED
} ps2link_state_t;

int ps2link_start(const char *host, const char *root_dir, const char *elf_name);

void ps2link_stop(void);

ps2link_state_t ps2link_get_state(void);

const char *ps2link_get_error(void);

const char *ps2link_poll_line(void);

#endif
