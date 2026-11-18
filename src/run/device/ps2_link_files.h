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
 * Licensed under the MIT License.
 * See the LICENSE file for details.
 */

#ifndef PS2_LINK_FILES_H
#define PS2_LINK_FILES_H

int ps2_link_elf_available(void);

int ps2_link_export_elf(const char *folder, char *error, int error_size);

int ps2_link_export_ipconfig(const char *folder, const char *address, char *error, int error_size);

int ps2_link_config_text(const char *address, char *out, int out_size);

#endif
