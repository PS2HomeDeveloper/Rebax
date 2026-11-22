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
    static const char *const frameworks[] = { "Cocoa", "CoreAudio", "AudioToolbox", "CoreVideo", "IOKit", "ForceFeedback", "Carbon", "CoreServices", "CoreFoundation", "Foundation", "AppKit", "Metal", "QuartzCore", "OpenGL", "CoreHaptics", "GameController", NULL };
    char runtime_lib[MODERN_PATH_CAP];
    if (!modern_configure_apple(config)) return 0;
    snprintf(runtime_lib, sizeof(runtime_lib), "%s/runtime/lib/libclang_rt.osx.a", g_modern_sdk_root);
    if (!modern_require_file(runtime_lib, "macOS compiler runtime libclang_rt.osx.a")) return 0;
    modern_appendf(g_modern_link_post, sizeof(g_modern_link_post), " \"%s\"", runtime_lib);
    modern_add_frameworks(g_modern_sdk_root, frameworks);
    return 1;
}

static void link_output(char *out, size_t cap) {
    snprintf(out, cap, "%s/%s", g_build_dir, g_modern_product_name);
}

static int package(void) {
    char bundle[MODERN_PATH_CAP], source[MODERN_PATH_CAP], destination[MODERN_PATH_CAP];
    if (!modern_package_desktop(g_modern_product_name, 1, bundle, sizeof(bundle))) return 0;
    snprintf(source, sizeof(source), "%s/graphics/lib/libSDL2.dylib", g_modern_sdk_root);
    if (modern_is_regular_file(source)) {
        snprintf(destination, sizeof(destination), "%s/libSDL2.dylib", bundle);
        if (!modern_copy_checked(source, destination, "SDL2 runtime")) return 0;
    }
    log_pushf("[modern] desktop export ready: %s", bundle);
    return 1;
}

const modern_platform_t modern_platform_macos = { "macos", NULL, configure, NULL, link_output, package };
