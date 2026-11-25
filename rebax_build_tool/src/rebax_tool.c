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

#ifndef __APPLE__
#define _POSIX_C_SOURCE 200809L
#endif

#include <stdio.h>
#include <string.h>

#include "network/package_fetch.h"
#include "rebax_env.h"
#include "tool_commands.h"

static void usage(void) {
    fputs("usage: rebax-tool <command> [args]\n"
          "  mkdir <dir>...                          create directories (mkdir -p)\n"
          "  cp <src> <dst>                          copy a file\n"
          "  icon-names <out.h> <icon.png>...        generate icon_names.h\n"
          "  icon-atlas <out.png> <icon.png>...      build one 256x256 icon atlas PNG\n"
          "  icon-atlas-pages <dir> <header> <icon.png>... build icons1.png, icons2.png, ...\n"
          "  node-registry <registry.h> <types.h> <node.c>...\n"
          "  node-editor-registry <out.h> <node_editor.c>...\n"
          "  node-sim-registry <out.h> <node_sim.c>...\n"
          "  android-entry <out.c>                   write the SDL_main wrapper source\n"
          "  android-keygen <keys_dir>\n"
          "  apk-pack <stage_dir> <out.apk> <keys_dir> <package> <label> <version_name> <version_code> <min_sdk> <target_sdk>\n"
          "  require-lib <label> <arch> <path>       check that a library exists for the arch\n"
          "  copy-dir <src> <dst> [--skip name]...   copy a directory tree, skipping parts named <name>\n"
          "  asset-manifest <dir> <out_file>         list every file under <dir>, one relative path per line\n"
          "  ios-plist <out.plist> <version> <exe> <iPhoneOS|iPhoneSimulator>  write Info.plist\n"
          "  zip-dir <out.zip> <base> <entry> [--exec <suffix>]...  zip <base>/<entry> (stored) keeping exec bits\n",
          stderr);
}

int main(int argc, char **argv) {
    if (argc < 2) { usage(); return 2; }
    const char *cmd = argv[1];

    if (strcmp(cmd, "env") == 0) return rebax_env_main(argc, argv);
    if (strcmp(cmd, "ok") == 0) { puts("ok"); return 0; }
    if (strcmp(cmd, "mkdir") == 0) return cmd_mkdir(argc, argv);
    if (strcmp(cmd, "rm") == 0) return cmd_rm(argc, argv);
    if (strcmp(cmd, "cp") == 0) return cmd_cp(argc, argv);
    if (strcmp(cmd, "icon-names") == 0) return cmd_icon_names(argc, argv);
    if (strcmp(cmd, "icon-atlas") == 0) return cmd_icon_atlas(argc, argv);
    if (strcmp(cmd, "icon-atlas-pages") == 0) return cmd_icon_atlas_pages(argc, argv);
    if (strcmp(cmd, "node-registry") == 0) return cmd_node_registry(argc, argv);
    if (strcmp(cmd, "node-editor-registry") == 0) return cmd_node_editor_registry(argc, argv);
    if (strcmp(cmd, "node-sim-registry") == 0) return cmd_node_sim_registry(argc, argv);
    if (strcmp(cmd, "android-entry") == 0) return cmd_android_entry(argc, argv);
    if (strcmp(cmd, "android-keygen") == 0) return cmd_android_keygen(argc, argv);
    if (strcmp(cmd, "apk-pack") == 0) return cmd_apk_pack(argc, argv);
    if (strcmp(cmd, "require-lib") == 0) return cmd_require_lib(argc, argv);
    if (strcmp(cmd, "copy-dir") == 0) return cmd_copy_dir(argc, argv);
    if (strcmp(cmd, "copy-tree") == 0) return cmd_copy_tree(argc, argv);
    if (strcmp(cmd, "package-archive") == 0) return rebax_package_archive_main(argc, argv);
    if (strcmp(cmd, "write-text") == 0) return cmd_write_text(argc, argv);
    if (strcmp(cmd, "asset-manifest") == 0) return cmd_asset_manifest(argc, argv);
    if (strcmp(cmd, "ios-plist") == 0) return cmd_ios_plist(argc, argv);
    if (strcmp(cmd, "zip-dir") == 0) return cmd_zip_dir(argc, argv);

    usage();
    return 2;
}
