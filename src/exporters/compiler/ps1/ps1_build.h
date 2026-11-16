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
 * Licensed under the MIT License.
 * See the LICENSE file for details.
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
