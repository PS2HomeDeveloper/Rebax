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
 * Licensed under the MIT License.
 * See the LICENSE file for details.
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
    char path[MODERN_PATH_CAP], builtins[MODERN_PATH_CAP];
    modern_appendf(g_modern_compile_flags, sizeof(g_modern_compile_flags), " --sysroot=\"%s\"", config->standard);
    modern_appendf(g_modern_link_pre, sizeof(g_modern_link_pre), "--target=%s --sysroot=\"%s\" -fuse-ld=lld -static -nodefaultlibs -Wl,--gc-sections%s", config->triple, config->standard, g_modern_release ? " -s" : "");
    modern_appendf(g_modern_link_post, sizeof(g_modern_link_post), "-L\"%s/lib\" -L\"%s/lib\"", config->runtime, config->graphics);
    snprintf(path, sizeof(path), "%s/lib/libSDL2.a", config->graphics);
    if (modern_is_regular_file(path)) {
        modern_appendf(g_modern_link_post, sizeof(g_modern_link_post), " -lmingw32 -lSDL2main -lSDL2 -ldinput8 -ldxguid -ldxerr8 -luser32 -lgdi32 -lwinmm -limm32 -lole32 -loleaut32 -lshell32 -lsetupapi -lversion -luuid -lws2_32 -lbcrypt -lmingwex -lmoldname -lmsvcrt -lkernel32 -ladvapi32");
    } else {
        snprintf(path, sizeof(path), "%s/lib/libSDL2.dll.a", config->graphics);
        if (!modern_require_file(path, "Windows SDL2 import library")) return 0;
        modern_appendf(g_modern_link_post, sizeof(g_modern_link_post), " -lmingw32 -lSDL2main \"%s/lib/libSDL2.dll.a\" -lws2_32 -lbcrypt -lmingwex -lmoldname -lmsvcrt -lkernel32 -ladvapi32 -lshell32 -luser32", config->graphics);
    }
    if (!modern_locate_builtins(g_modern_sdk_root, "", builtins, sizeof(builtins))) {
        log_pushf("[modern] FAILED: the Windows compiler runtime is missing under %s/runtime/lib.", g_modern_sdk_root);
        g_modern_status = MODERN_EXPORT_FAILED;
        return 0;
    }
    modern_appendf(g_modern_link_post, sizeof(g_modern_link_post), " \"%s\"", builtins);
    return 1;
}

static void link_output(char *out, size_t cap) {
    snprintf(out, cap, "%s/%s.exe", g_build_dir, g_modern_product_name);
}

static int package(void) {
    char bundle[MODERN_PATH_CAP], executable[MODERN_PATH_CAP], source[MODERN_PATH_CAP], destination[MODERN_PATH_CAP];
    snprintf(executable, sizeof(executable), "%s.exe", g_modern_product_name);
    if (!modern_package_desktop(executable, 0, bundle, sizeof(bundle))) return 0;
    snprintf(source, sizeof(source), "%s/graphics/lib/libSDL2.a", g_modern_sdk_root);
    if (!modern_is_regular_file(source)) {
        snprintf(source, sizeof(source), "%s/graphics/bin/SDL2.dll", g_modern_sdk_root);
        snprintf(destination, sizeof(destination), "%s/SDL2.dll", bundle);
        if (!modern_copy_checked(source, destination, "SDL2.dll")) return 0;
    }
    log_pushf("[modern] desktop export ready: %s", bundle);
    return 1;
}

const modern_platform_t modern_platform_windows = { "windows", NULL, configure, NULL, link_output, package };
