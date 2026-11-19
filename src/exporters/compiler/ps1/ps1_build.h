/*
 *  ____  _____ ____    _    __  __
 * |  _ \| ____| __ )  / \   \ \/ /
 * | |_) | _| | _ \ / _ \   \  /
 * |  _ <| |___| |_) / ___ \  /  \\
 * |_| \_\_____|____/_/   \_\/ /_\
 *
 * Rebax Engine
 * Copyright (c) 2026 PS2HomeDeveloper
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */
#ifndef REBAX_PS1_BUILD_H
#define REBAX_PS1_BUILD_H

typedef enum { PS1_BUILD_EE_ELF } ps1_build_kind_t;
typedef struct {
    ps1_build_kind_t kind;
    const char *toolchain_root;
    const char *sdk_standard_root;
    const char *sdk_graphics_root;
    const char *work_dir;
    const char *output;
    int release;
    const char *const *incs;
    int inc_count;
    const char *const *libs;
    int lib_count;
} ps1_build_config_t;
int ps1_build_start(const ps1_build_config_t *config);
int ps1_build_poll(int *out_ok);
void ps1_build_cancel(void);
#endif
