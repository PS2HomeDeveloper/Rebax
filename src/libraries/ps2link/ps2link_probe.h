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
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef PS2LINK_PROBE_H
#define PS2LINK_PROBE_H

typedef enum {
    PS2LINK_PROBE_IDLE,
    PS2LINK_PROBE_CHECKING,
    PS2LINK_PROBE_REACHABLE,
    PS2LINK_PROBE_UNREACHABLE
} ps2link_probe_state_t;

int ps2link_probe_start(const char *host);
ps2link_probe_state_t ps2link_probe_state(void);
void ps2link_probe_reset(void);

#endif
