/*
 *  ____  _____ ____    _    __  __
 * |  _ \\| ____| __ )  / \\   \\ \/ /
 * | |_) |  _| |  _ \\ / _ \\   \\  / 
 * |  _ <| |___| |_) / ___ \\  /  \\ 
 * |_| \\_\\_____|____/_/   \\_\\/_/\\_\\
 *
 * Rebax Engine
 * Copyright (c) 2026 PS2HomeDeveloper
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */


#ifndef REBAX_MODERN_INTERNAL_H
#define REBAX_MODERN_INTERNAL_H

#include <stddef.h>

#include "exporters/compiler/modern/modern_exporter.h"

#define MODERN_PATH_CAP 2048
#define MODERN_COMMAND_CAP 32768
#define MODERN_SDK_VERSION "1.0.0"

typedef struct {
    const char *name;
    const char *sdk;
    const char *triple;
    const char *libdir;
    unsigned project_flag;
    int arch_count;
    const char *const *architectures;
} modern_target_t;

typedef struct {
    char standard[MODERN_PATH_CAP];
    char runtime[MODERN_PATH_CAP];
    char graphics[MODERN_PATH_CAP];
    char empty_sysroot[MODERN_PATH_CAP];
    const char *triple;
    const char *libdir;
    int arch_index;
} modern_config_t;

typedef struct {
    const char *name;
    int (*check_host)(void);
    int (*configure)(const modern_config_t *config);
    int (*prepare_sources)(void);
    void (*link_output)(char *out, size_t cap);
    int (*package)(void);
} modern_platform_t;

extern const modern_platform_t modern_platform_android;
extern const modern_platform_t modern_platform_linux;
extern const modern_platform_t modern_platform_windows;
extern const modern_platform_t modern_platform_macos;
extern const modern_platform_t modern_platform_ios;

extern modern_export_status_t g_modern_status;
extern int g_modern_release;
extern int g_modern_target_index;
extern int g_modern_architecture_index;
extern int g_modern_architecture_count;
extern char g_modern_target[24];
extern char g_modern_arch[32];
extern char g_modern_sdk_root[MODERN_PATH_CAP];
extern char g_modern_source_dir[MODERN_PATH_CAP];
extern char g_modern_compile_flags[8192];
extern char g_modern_link_pre[8192];
extern char g_modern_link_post[12288];
extern char g_modern_link_output[MODERN_PATH_CAP];
extern char g_modern_product_name[128];
extern char g_modern_output_dir[MODERN_PATH_CAP];
extern char g_modern_apk_stage[MODERN_PATH_CAP];

void modern_appendf(char *out, size_t cap, const char *format, ...);
int modern_is_regular_file(const char *path);
int modern_require_file(const char *path, const char *description);
int modern_fail(const char *message);
int modern_locate_builtins(const char *sdk_root, const char *libdir, char *out, size_t cap);
int modern_copy_checked(const char *source, const char *destination, const char *description);
int modern_copy_project_assets(const char *destination);
int modern_set_executable_permission(const char *path);
int modern_package_desktop(const char *executable, int executable_bit, char *bundle, size_t bundle_cap);
int modern_configure_apple(const modern_config_t *config);
void modern_add_frameworks(const char *sdk_root, const char *const *names);
const char *modern_target_libdir(const modern_target_t *target, int arch_index);
int modern_target_arch_index(const modern_target_t *target, const char *architecture);

#endif
