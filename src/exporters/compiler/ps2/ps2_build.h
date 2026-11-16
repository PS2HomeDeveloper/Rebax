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

/* Direct PS2 toolchain driver. Plans and runs compiler/linker commands without make. */
#ifndef REBAX_PS2_BUILD_H
#define REBAX_PS2_BUILD_H

typedef enum {
    PS2_BUILD_EE_ELF,
    PS2_BUILD_EE_ERL,
    PS2_BUILD_EE_LIB,
    PS2_BUILD_IOP_IRX,
    PS2_BUILD_IOP_LIB,
    PS2_BUILD_IOPRP
} ps2_build_kind_t;

typedef struct {
    ps2_build_kind_t kind;
    const char *toolchain_root;
    const char *sdk_standard_root;
    const char *sdk_graphics_root;
    const char *work_dir;
    const char *output;

    int release;
    int newlib_nano;
    int parallel_jobs;

    const char *const *incs;
    int inc_count;
    const char *const *libs;
    int lib_count;
    const char *const *cflags;
    int cflag_count;
    const char *const *ldflags;
    int ldflag_count;

    const char *opt_flags;
    const char *warn_flags;
    const char *debug_flags;
    const char *linkfile;
    const char *iop_gpopt_size;

    const char *const *ioprp_contents;
    int ioprp_count;
} ps2_build_config_t;

int ps2_build_start(const ps2_build_config_t *config);
int ps2_build_poll(int *out_ok);
void ps2_build_cancel(void);

#endif
