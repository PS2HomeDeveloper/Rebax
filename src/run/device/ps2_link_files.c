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

#include <stdio.h>
#include <string.h>
#include "run/device/ps2_link_files.h"
#include "run/run_settings.h"
#include "filesystem/storage_paths.h"
#include "filesystem/rebax_fs.h"

#define LINK_ELF_NAME "ps2link.elf"
#define CONFIG_NAME "IPCONFIG.DAT"

static void elf_path(char *out, int size) { snprintf(out, size, "%s/ps2/ps2link/%s", rebax_sdks_dir(), LINK_ELF_NAME); }

static void set_error(char *error, int size, const char *text) { if (error && size > 0) snprintf(error, size, "%s", text); }

int ps2_link_elf_available(void) {
    char path[1100];
    elf_path(path, sizeof(path));
    return rebax_fs_exists(path);
}

int ps2_link_export_elf(const char *folder, char *error, int error_size) {
    char src[1100], dst[1100];
    elf_path(src, sizeof(src));
    if (!rebax_fs_exists(src)) { set_error(error, error_size, "ps2link.elf is missing. Update the PlayStation 2 package"); return 0; }
    if (!rebax_fs_is_dir(folder)) { set_error(error, error_size, "The destination folder does not exist"); return 0; }
    snprintf(dst, sizeof(dst), "%s/%s", folder, LINK_ELF_NAME);
    if (!rebax_fs_copy_file(src, dst)) { set_error(error, error_size, "Could not write ps2link.elf to the destination"); return 0; }
    return 1;
}

int ps2_link_config_text(const char *address, char *out, int out_size) {
    int a, b, c, d;
    if (!run_settings_valid_address(address) || sscanf(address, "%d.%d.%d.%d", &a, &b, &c, &d) != 4) return 0;
    snprintf(out, out_size, "%d.%d.%d.%d 255.255.255.0 %d.%d.%d.1", a, b, c, d, a, b, c);
    return 1;
}

int ps2_link_export_ipconfig(const char *folder, const char *address, char *error, int error_size) {
    char text[128], dst[1100];
    if (!ps2_link_config_text(address, text, sizeof(text))) { set_error(error, error_size, "The PlayStation 2 address is not valid"); return 0; }
    if (!rebax_fs_is_dir(folder)) { set_error(error, error_size, "The destination folder does not exist"); return 0; }
    snprintf(dst, sizeof(dst), "%s/%s", folder, CONFIG_NAME);
    if (!rebax_fs_write_atomic(dst, text, strlen(text))) { set_error(error, error_size, "Could not write IPCONFIG.DAT to the destination"); return 0; }
    return 1;
}
