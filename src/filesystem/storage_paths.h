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

#ifndef REBAX_PATHS_H
#define REBAX_PATHS_H

const char *rebax_root_dir(void);
const char *rebax_toolchains_dir(void);
const char *rebax_toolchain_dir(void);
const char *rebax_sdks_dir(void);
const char *rebax_tools_dir(void);
const char *rebax_node_resources_dir(void);
const char *rebax_temp_export_dir(void);
const char *rebax_editor_dir(void);

int rebax_paths_is_setup_needed(void);
void rebax_paths_setup_start(void);
void rebax_paths_setup_update(void);
int rebax_paths_setup_done(void);
int rebax_paths_setup_failed(void);

#endif
