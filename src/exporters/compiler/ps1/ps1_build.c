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
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "exporters/compiler/ps1/ps1_build.h"
#include "exporters/common/export_internal.h"
#include "filesystem/rebax_fs.h"

#define PS1_CMD 32000
#define PS1_PATH 2048
static int g_active;
static char *g_command;

static int source_file(const char *name) {
    size_t n = strlen(name);
    return n > 2 && strcmp(name + n - 2, ".c") == 0;
}

static void append(char *out, size_t cap, const char *text) {
    size_t have = strlen(out), add = strlen(text);
    if (have + add + 1 < cap) memcpy(out + have, text, add + 1);
}

static void append_sources(const char *root, char *out, size_t cap) {
    DIR *dir = opendir(root);
    struct dirent *entry;
    if (dir == NULL) return;
    while ((entry = readdir(dir)) != NULL) {
        char path[PS1_PATH];
        struct stat st;
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
        snprintf(path, sizeof(path), "%s/%s", root, entry->d_name);
        if (stat(path, &st) != 0) continue;
        if (S_ISDIR(st.st_mode)) append_sources(path, out, cap);
        else if (S_ISREG(st.st_mode) && source_file(entry->d_name)) {
            append(out, cap, " \""); append(out, cap, path); append(out, cap, "\"");
        }
    }
    closedir(dir);
}

static void quote_arg(char *out, size_t cap, const char *value) {
    append(out, cap, " \""); append(out, cap, value); append(out, cap, "\"");
}

int ps1_build_start(const ps1_build_config_t *config) {
    char command[PS1_CMD];
    char include[PS1_PATH];
    if (config == NULL || config->toolchain_root == NULL || config->sdk_standard_root == NULL || config->sdk_graphics_root == NULL) return 0;
    ps1_build_cancel();
    command[0] = '\0';
    snprintf(include, sizeof(include), "%s/bin/mipsel-none-elf-gcc", config->toolchain_root);
    quote_arg(command, sizeof(command), include);
    append(command, sizeof(command), " -ffreestanding -fno-builtin -nostdlib -fsigned-char -fno-strict-overflow -msoft-float -march=r3000 -mtune=r3000 -mabi=32 -mno-mt -mno-llsc -G0 -fno-pic -mno-abicalls -mno-gpopt -ffunction-sections -fdata-sections -O2 -DPSN00BSDK=1 -DNDEBUG=1");
    snprintf(include, sizeof(include), "-I%s", config->work_dir); append(command, sizeof(command), " "); append(command, sizeof(command), include);
    snprintf(include, sizeof(include), " -I%s/include/libpsn00b", config->sdk_standard_root); append(command, sizeof(command), include);
    snprintf(include, sizeof(include), " -I%s/include/libpsn00b", config->sdk_graphics_root); append(command, sizeof(command), include);
    for (int i = 0; i < config->inc_count; i++) { append(command, sizeof(command), " "); append(command, sizeof(command), config->incs[i]); }
    append_sources(config->work_dir, command, sizeof(command));
    snprintf(include, sizeof(include), " -T%s/lib/libpsn00b/ldscripts/exe.ld -L%s/lib/libpsn00b/release -L%s/lib/libpsn00b/release", config->sdk_standard_root, config->sdk_standard_root, config->sdk_graphics_root);
    append(command, sizeof(command), include);
    append(command, sizeof(command), " -static -Wl,--gc-sections -o "); quote_arg(command, sizeof(command), config->output);
    append(command, sizeof(command), " -lpsxgpu_exe_nogprel -lpsxgte_exe_nogprel -lpsxspu_exe_nogprel -lpsxcd_exe_nogprel -lpsxetc_exe_nogprel -lpsxapi_exe_nogprel -lc_exe_nogprel -lgcc");
    for (int i = 0; i < config->lib_count; i++) { append(command, sizeof(command), " "); append(command, sizeof(command), config->libs[i]); }
    append(command, sizeof(command), " 2>&1");
    {
        char gcc_path[PS1_PATH];
        snprintf(gcc_path, sizeof(gcc_path), "%s/bin/mipsel-none-elf-gcc", config->toolchain_root);
        if (!rebax_fs_exists(gcc_path)) {
            snprintf(gcc_path, sizeof(gcc_path), "%s/bin/mipsel-none-elf-gcc.exe", config->toolchain_root);
            if (!rebax_fs_exists(gcc_path)) {
                log_pushf("[ps1] FAILED: the PlayStation 1 compiler was not found in %s/bin. Install the PlayStation 1 package from Export Packages.", config->toolchain_root);
                return 0;
            }
        }
        snprintf(gcc_path, sizeof(gcc_path), "%s/lib/libpsn00b/ldscripts/exe.ld", config->sdk_standard_root);
        if (!rebax_fs_exists(gcc_path)) {
            log_pushf("[ps1] FAILED: the PlayStation 1 SDK was not found (missing %s). Install the PlayStation 1 package from Export Packages.", gcc_path);
            return 0;
        }
    }
    g_command = strdup(command);
    if (g_command == NULL) return 0;
    log_push("[ps1] starting mipsel-none-elf-gcc build");
    if (!shell_step_start(&g_step, g_command)) { free(g_command); g_command = NULL; return 0; }
    g_active = 1;
    return 1;
}

int ps1_build_poll(int *out_ok) {
    int running;
    if (!g_active) { if (out_ok) *out_ok = 0; return 0; }
    running = shell_step_poll(&g_step, out_ok);
    if (!running) { g_active = 0; free(g_command); g_command = NULL; }
    return running;
}

void ps1_build_cancel(void) {
    if (g_active) shell_step_cancel(&g_step);
    g_active = 0;
    free(g_command);
    g_command = NULL;
}
