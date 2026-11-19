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


int modern_configure_apple(const modern_config_t *config) {
    char path[MODERN_PATH_CAP];
    modern_appendf(g_modern_compile_flags, sizeof(g_modern_compile_flags), " -nostdlibinc -isysroot \"%s\" -isystem \"%s/include\" -F\"%s/standard/Frameworks\" -F\"%s/graphics/Frameworks\"", config->empty_sysroot, config->standard, g_modern_sdk_root, g_modern_sdk_root);
    modern_appendf(g_modern_link_pre, sizeof(g_modern_link_pre), "--target=%s -isysroot \"%s\" -fuse-ld=lld -nostdlib -Wl,-dead_strip", config->triple, config->empty_sysroot);
    modern_appendf(g_modern_link_post, sizeof(g_modern_link_post), "-L\"%s/lib\" -L\"%s/lib\" -L\"%s/lib\" -lSDL2 -liconv -lSystem", config->standard, config->runtime, config->graphics);
    snprintf(path, sizeof(path), "%s/graphics/lib/libSDL2.a", g_modern_sdk_root);
    return modern_require_file(path, "SDL2 static library");
}

void modern_add_frameworks(const char *sdk_root, const char *const *names) {
    char path[MODERN_PATH_CAP];
    for (int i = 0; names[i] != NULL; i++) {
        snprintf(path, sizeof(path), "%s/standard/Frameworks/%s.framework", sdk_root, names[i]);
        if (rebax_fs_is_dir(path)) modern_appendf(g_modern_link_post, sizeof(g_modern_link_post), " -framework %s", names[i]);
        else {
            snprintf(path, sizeof(path), "%s/graphics/Frameworks/%s.framework", sdk_root, names[i]);
            if (rebax_fs_is_dir(path)) modern_appendf(g_modern_link_post, sizeof(g_modern_link_post), " -F\"%s/graphics/Frameworks\" -framework %s", sdk_root, names[i]);
        }
    }
}

