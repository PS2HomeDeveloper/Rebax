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

#ifndef PS2LINK_HOST_FILES_H
#define PS2LINK_HOST_FILES_H

#include "libraries/ps2link/ps2link_socket.h"

void ps2link_host_files_reset(const char *root_dir);
void ps2link_host_files_close_all(void);
int ps2link_host_files_ping_answered(void);
int ps2link_host_files_handle(ps2link_socket_t sock, unsigned int number, const unsigned char *payload, int payload_size);

#endif
