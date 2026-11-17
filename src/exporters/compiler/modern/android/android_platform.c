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
#include "filesystem/storage_paths.h"
#include "project/current_project.h"

#include "libraries/apk/apk_pack.h"
#include "project/project_types.h"

static int g_android_static;

static int configure(const modern_config_t *config) {
    char static_lib[MODERN_PATH_CAP], static_cxx[MODERN_PATH_CAP], static_abi[MODERN_PATH_CAP], unwind_lib[MODERN_PATH_CAP];
    char builtins[MODERN_PATH_CAP], map_path[MODERN_PATH_CAP];
    snprintf(static_lib, sizeof(static_lib), "%s/static/lib/libSDL2.a", config->graphics);
    snprintf(static_cxx, sizeof(static_cxx), "%s/lib/%s/libc++_static.a", config->runtime, config->libdir);
    snprintf(static_abi, sizeof(static_abi), "%s/lib/%s/libc++abi.a", config->runtime, config->libdir);
    snprintf(unwind_lib, sizeof(unwind_lib), "%s/lib/%s/libunwind.a", config->runtime, config->libdir);
    if (!modern_is_regular_file(static_lib) || !modern_is_regular_file(static_cxx) || !modern_is_regular_file(static_abi)) {
        log_pushf("[modern] FAILED: the Android SDK package is outdated, static libraries are missing: %s, %s, or %s. Delete the sdks folder of this platform and download the latest SDK package.", static_lib, static_cxx, static_abi);
        g_modern_status = MODERN_EXPORT_FAILED;
        return 0;
    }
    g_android_static = 1;
    modern_appendf(g_modern_compile_flags, sizeof(g_modern_compile_flags), " --sysroot=\"%s\" -fPIC -DSDL_MAIN_HANDLED=1", config->standard);
    modern_appendf(g_modern_link_pre, sizeof(g_modern_link_pre), "--target=%s --sysroot=\"%s\" -fuse-ld=lld -nodefaultlibs -shared -Wl,-soname,libmain.so -Wl,--gc-sections%s", config->triple, config->standard, g_modern_release ? " -s" : "");
    snprintf(map_path, sizeof(map_path), "%s/Temp/modern-android.map", rebax_root_dir());
    FILE *map = fopen(map_path, "w");
    if (map == NULL) {
        log_pushf("[modern] FAILED: cannot write the Android export symbol list: %s", map_path);
        g_modern_status = MODERN_EXPORT_FAILED;
        return 0;
    }
    fputs("{\n global:\n  Java_*;\n  JNI_*;\n  SDL_main;\n local:\n  *;\n};\n", map);
    fclose(map);
    modern_appendf(g_modern_link_pre, sizeof(g_modern_link_pre), " -Wl,--version-script=\"%s\" -Wl,-u,Java_org_libsdl_app_SDLActivity_nativeInit -Wl,-u,Java_org_libsdl_app_HIDDeviceManager_HIDDeviceRegisterCallback", map_path);
    modern_appendf(g_modern_link_post, sizeof(g_modern_link_post), "-L\"%s/lib/%s\" -L\"%s/usr/lib/%s/21\" -L\"%s/usr/lib/%s\" -L\"%s/static/lib\"", config->runtime, config->libdir, config->standard, config->libdir, config->standard, config->libdir, config->graphics);
    modern_appendf(g_modern_link_post, sizeof(g_modern_link_post), " -lSDL2 -lc++_static -lc++abi");
    if (modern_is_regular_file(unwind_lib)) modern_appendf(g_modern_link_post, sizeof(g_modern_link_post), " -lunwind");
    modern_appendf(g_modern_link_post, sizeof(g_modern_link_post), " -lm -llog -landroid -lOpenSLES -lGLESv1_CM -lGLESv2 -lc -ldl");
    if (!modern_locate_builtins(g_modern_sdk_root, config->libdir, builtins, sizeof(builtins))) {
        log_pushf("[modern] FAILED: the Android compiler runtime for %s is missing under %s/runtime/lib.", config->libdir, g_modern_sdk_root);
        g_modern_status = MODERN_EXPORT_FAILED;
        return 0;
    }
    modern_appendf(g_modern_link_post, sizeof(g_modern_link_post), " \"%s\"", builtins);
    return 1;
}

static int prepare_sources(void) {
    char path[MODERN_PATH_CAP];
    snprintf(path, sizeof(path), "%s/android_entry.c", g_modern_source_dir);
    FILE *file = fopen(path, "wb");
    if (!file) {
        log_push("[modern] FAILED: could not create the Android SDL entry source.");
        return 0;
    }
    fputs("extern int main(int, char **);\nint SDL_main(int argc, char **argv) { return main(argc, argv); }\n", file);
    if (fclose(file) != 0) {
        log_push("[modern] FAILED: could not create the Android SDL entry source.");
        return 0;
    }
    return 1;
}

static void link_output(char *out, size_t cap) {
    snprintf(out, cap, "%s/libmain.so", g_build_dir);
}

static int package(void) {
    char stage[MODERN_PATH_CAP], path[MODERN_PATH_CAP], source[MODERN_PATH_CAP], output[MODERN_PATH_CAP];
    char architecture_dir[MODERN_PATH_CAP], key_dir[MODERN_PATH_CAP];
    snprintf(stage, sizeof(stage), "%s", g_modern_apk_stage);
    if (g_modern_architecture_index == 0) rebax_fs_remove_recursive(stage);
    snprintf(architecture_dir, sizeof(architecture_dir), "%s/lib/%s", stage, g_modern_arch);
    if (!rebax_fs_mkdir_p(architecture_dir)) {
        log_pushf("[modern] FAILED: cannot create APK staging directory: %s", architecture_dir);
        return 0;
    }
    if (g_modern_architecture_index == 0) {
        snprintf(source, sizeof(source), "%s/graphics/dex/classes.dex", g_modern_sdk_root);
        snprintf(path, sizeof(path), "%s/classes.dex", stage);
        if (!modern_copy_checked(source, path, "Android SDL classes.dex")) return 0;
    }
    snprintf(path, sizeof(path), "%s/libmain.so", architecture_dir);
    if (!modern_copy_checked(g_modern_link_output, path, "libmain.so")) return 0;
    if (g_modern_architecture_index == 0) {
        snprintf(path, sizeof(path), "%s/assets", stage);
        if (!modern_copy_project_assets(path)) {
            log_push("[modern] FAILED: could not copy project assets into the APK.");
            return 0;
        }
    }
    if (g_modern_architecture_index + 1 < g_modern_architecture_count) {
        log_pushf("[modern] Android architecture ready: %s", g_modern_arch);
        return 1;
    }
    if (!rebax_fs_mkdir_p(g_modern_output_dir)) {
        log_pushf("[modern] FAILED: cannot create APK output folder: %s", g_modern_output_dir);
        return 0;
    }
    snprintf(output, sizeof(output), "%s/%s.apk", g_modern_output_dir, g_modern_product_name);
    snprintf(key_dir, sizeof(key_dir), "%s/keystore", rebax_root_dir());
    apk_pack_config_t config;
    memset(&config, 0, sizeof(config));
    config.stage_dir = stage;
    config.out_path = output;
    config.keys_dir = key_dir;
    config.package_name = "org.rebax.game";
    config.app_label = g_modern_product_name;
    config.version_name = MODERN_SDK_VERSION;
    config.version_code = 1;
    config.min_sdk = 24;
    config.target_sdk = 28;
    config.activity_name = "org.rebax.game.GameActivity";
    config.screen_orientation = current_project_get_settings()->display_width >= current_project_get_settings()->display_height ? 6 : 7;
    char error[512];
    log_pushf("[modern] packing and signing APK: %s", output);
    if (!apk_pack(&config, error, sizeof(error))) {
        log_pushf("[modern] FAILED: APK creation/signing failed: %s", error[0] ? error : "unknown APK library error");
        return 0;
    }
    log_pushf("[modern] signed APK ready: %s", output);
    return 1;
}

const modern_platform_t modern_platform_android = { "android", NULL, configure, prepare_sources, link_output, package };
