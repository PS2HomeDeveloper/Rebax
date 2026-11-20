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

#ifndef NODE_SOURCE_H
#define NODE_SOURCE_H

typedef void (*node_source_name_cb)(const char *name, void *user);

void node_source_use(const char *sdk_name, const char *sdk_root);
int node_source_is_modern(void);
int node_source_is_ps1(void);
int node_source_list(node_source_name_cb callback, void *user);
int node_source_copy(const char *name, const char *dest_path);
char *node_source_read(const char *name, long *out_size);

#endif
