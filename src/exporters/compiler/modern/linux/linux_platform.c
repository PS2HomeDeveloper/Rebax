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

#define _POSIX_C_SOURCE 200809L

#include "exporters/compiler/modern/modern_internal.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if !defined(_WIN32)
#include <strings.h>
#include <sys/stat.h>
#endif

#include "exporters/common/export_internal.h"
#include "filesystem/rebax_fs.h"
#include "project/current_project.h"

static int configure(const modern_config_t *config) {
    char path[MODERN_PATH_CAP];
    modern_appendf(g_modern_link_pre, sizeof(g_modern_link_pre), "--target=%s -nostdlib -static -fuse-ld=lld -Wl,--eh-frame-hdr", config->triple);
    modern_appendf(g_modern_link_pre, sizeof(g_modern_link_pre), " -Wl,--gc-sections%s", g_modern_release ? " -s" : "");
    modern_appendf(g_modern_compile_flags, sizeof(g_modern_compile_flags), " -nostdlibinc -isystem \"%s/include\" -isystem \"%s/include\"", config->runtime, config->standard);
    modern_appendf(g_modern_link_pre, sizeof(g_modern_link_pre), " \"%s/lib/crt1.o\" \"%s/lib/crti.o\"", config->standard, config->standard);
    modern_appendf(g_modern_link_post, sizeof(g_modern_link_post), "-L\"%s/lib\" -L\"%s/lib\" -L\"%s/lib\" -lSDL2 -lm -lc -lgcc \"%s/lib/crtn.o\"", config->graphics, config->runtime, config->standard, config->standard);
    snprintf(path, sizeof(path), "%s/lib/crt1.o", config->standard);
    if (!modern_require_file(path, "Linux startup object crt1.o")) return 0;
    snprintf(path, sizeof(path), "%s/lib/crti.o", config->standard);
    if (!modern_require_file(path, "Linux startup object crti.o")) return 0;
    snprintf(path, sizeof(path), "%s/lib/crtn.o", config->standard);
    if (!modern_require_file(path, "Linux startup object crtn.o")) return 0;
    snprintf(path, sizeof(path), "%s/lib/libSDL2.a", config->graphics);
    if (!modern_require_file(path, "Linux SDL2 static library")) return 0;
    modern_appendf(g_modern_link_post, sizeof(g_modern_link_post), " -Wl,-rpath,'$ORIGIN'");
    return 1;
}

static void link_output(char *out, size_t cap) {
    snprintf(out, cap, "%s/%s", g_build_dir, g_modern_product_name);
}

static int package(void) {
    char bundle[MODERN_PATH_CAP], source[MODERN_PATH_CAP], destination[MODERN_PATH_CAP];
    if (!modern_package_desktop(g_modern_product_name, 1, bundle, sizeof(bundle))) return 0;
    snprintf(source, sizeof(source), "%s/graphics/lib/libSDL2.a", g_modern_sdk_root);
    if (!modern_is_regular_file(source)) {
        snprintf(source, sizeof(source), "%s/graphics/lib/libSDL2.so", g_modern_sdk_root);
        if (modern_is_regular_file(source)) {
            snprintf(destination, sizeof(destination), "%s/libSDL2.so", bundle);
            if (!modern_copy_checked(source, destination, "SDL2 runtime")) return 0;
        }
        snprintf(source, sizeof(source), "%s/graphics/lib/libiconv.so", g_modern_sdk_root);
        if (modern_is_regular_file(source)) {
            snprintf(destination, sizeof(destination), "%s/libiconv.so", bundle);
            if (!modern_copy_checked(source, destination, "iconv runtime")) return 0;
        }
    }
    log_pushf("[modern] desktop export ready: %s", bundle);
    return 1;
}

const modern_platform_t modern_platform_linux = { "linux", NULL, configure, NULL, link_output, package };
