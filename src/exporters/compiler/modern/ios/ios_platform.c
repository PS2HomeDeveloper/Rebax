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

#if defined(__APPLE__)
#include <TargetConditionals.h>
#endif

static int check_host(void) {
#if defined(__APPLE__)
#if TARGET_OS_IPHONE
    return 0;
#else
    return 1;
#endif
#else
    return 0;
#endif
}

static int configure(const modern_config_t *config) {
    static const char *const frameworks[] = { "UIKit", "Foundation", "CoreGraphics", "QuartzCore", "OpenGLES", "Metal", "AudioToolbox", "AVFoundation", "CoreAudio", "CoreMotion", "CoreHaptics", "GameController", "CoreBluetooth", "CoreFoundation", NULL };
    char runtime_lib[MODERN_PATH_CAP];
    if (!modern_configure_apple(config)) return 0;
    snprintf(runtime_lib, sizeof(runtime_lib), "%s/runtime/lib/%s", g_modern_sdk_root,
             config->arch_index == 1 ? "libclang_rt.iossim.a" : "libclang_rt.ios.a");
    if (!modern_require_file(runtime_lib, "iOS compiler runtime archive")) return 0;
    modern_appendf(g_modern_link_post, sizeof(g_modern_link_post), " \"%s\"", runtime_lib);
    modern_add_frameworks(g_modern_sdk_root, frameworks);
    return 1;
}

static void link_output(char *out, size_t cap) {
    snprintf(out, cap, "%s/%s", g_build_dir, g_modern_product_name);
}

static int write_ios_plist(const char *path) {
    FILE *file = fopen(path, "wb");
    if (!file) return 0;
    fprintf(file,
            "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
            "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"
            "<plist version=\"1.0\"><dict>\n"
            "<key>CFBundleExecutable</key><string>%s</string>\n"
            "<key>CFBundleIdentifier</key><string>org.rebax.game</string>\n"
            "<key>CFBundleName</key><string>%s</string>\n"
            "<key>CFBundlePackageType</key><string>APPL</string>\n"
            "<key>CFBundleShortVersionString</key><string>1.0.0</string>\n"
            "<key>CFBundleVersion</key><string>1</string>\n"
            "<key>MinimumOSVersion</key><string>13.0</string>\n"
            "<key>UIDeviceFamily</key><array><integer>1</integer><integer>2</integer></array>\n"
            "</dict></plist>\n", g_modern_product_name, g_modern_product_name);
    return fclose(file) == 0;
}

static int package(void) {
    char app[MODERN_PATH_CAP], path[MODERN_PATH_CAP];
    snprintf(app, sizeof(app), "%s/%s.app", g_modern_output_dir, g_modern_product_name);
    if (!rebax_fs_mkdir_p(app)) {
        log_pushf("[modern] FAILED: cannot create iOS app bundle: %s", app);
        return 0;
    }
    snprintf(path, sizeof(path), "%s/%s", app, g_modern_product_name);
    if (!modern_copy_checked(g_modern_link_output, path, "iOS executable")) return 0;
    snprintf(path, sizeof(path), "%s/Info.plist", app);
    if (!write_ios_plist(path)) {
        log_pushf("[modern] FAILED: could not write iOS bundle metadata: %s", path);
        return 0;
    }
    snprintf(path, sizeof(path), "%s/assets", app);
    if (!modern_copy_project_assets(path)) {
        log_push("[modern] FAILED: could not copy project assets into the iOS bundle.");
        return 0;
    }
    log_pushf("[modern] iOS app bundle ready: %s (unsigned)", app);
    return 1;
}

const modern_platform_t modern_platform_ios = { "ios", check_host, configure, NULL, link_output, package };
