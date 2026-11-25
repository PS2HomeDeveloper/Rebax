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

#ifndef TOOL_COMMANDS_H
#define TOOL_COMMANDS_H

int cmd_icon_names(int argc, char **argv);
int cmd_icon_atlas(int argc, char **argv);
int cmd_icon_atlas_pages(int argc, char **argv);
int cmd_node_registry(int argc, char **argv);
int cmd_node_editor_registry(int argc, char **argv);
int cmd_node_sim_registry(int argc, char **argv);
int cmd_mkdir(int argc, char **argv);
int cmd_rm(int argc, char **argv);
int cmd_cp(int argc, char **argv);
int cmd_write_text(int argc, char **argv);
int cmd_copy_tree(int argc, char **argv);
int cmd_copy_dir(int argc, char **argv);
int cmd_asset_manifest(int argc, char **argv);
int cmd_android_entry(int argc, char **argv);
int cmd_apk_pack(int argc, char **argv);
int cmd_android_keygen(int argc, char **argv);
int cmd_require_lib(int argc, char **argv);
int cmd_ios_plist(int argc, char **argv);
int cmd_zip_dir(int argc, char **argv);

#endif
