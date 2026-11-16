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

#include "exporters/compiler/modern/modern_exporter.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if !defined(_WIN32)
#include <strings.h>
#endif
#if !defined(_WIN32)
#include <sys/stat.h>
#endif

#include "exporters/common/export_internal.h"
#include "exporters/common/node_source.h"
#include "filesystem/rebax_fs.h"
#include "filesystem/storage_paths.h"
#include "project/current_project.h"
#include "project/project_types.h"
#include "libraries/apk/apk_pack.h"

#if defined(__APPLE__)
#include <TargetConditionals.h>
#endif

#define MODERN_MAX_SOURCES 192
#define MODERN_MAX_ACTIVE 4
#define MODERN_PATH_CAP 2048
#define MODERN_COMMAND_CAP 32768
#define MODERN_SDK_VERSION "1.0.0"
#define MODERN_SDK_OBJECTS 7

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
    char source[MODERN_PATH_CAP];
    char object[MODERN_PATH_CAP];
    char command[MODERN_COMMAND_CAP];
    export_shell_step_t process;
    int sdk_index;
    int started;
    int done;
} modern_compile_job_t;

typedef struct {
    char source_root[MODERN_PATH_CAP];
    char destination_root[MODERN_PATH_CAP];
    int failed;
    int resources_only;
} modern_copy_context_t;

static const char *const ANDROID_ARCHES[] = { "arm64-v8a", "armeabi-v7a", "x86", "x86_64" };
static const char *const LINUX_ARCHES[] = { "arm64", "arm", "x86", "x86_64" };
static const char *const WINDOWS_ARCHES[] = { "arm64", "arm", "x86", "x86_64" };
static const char *const MACOS_ARCHES[] = { "arm64", "x86_64" };
static const char *const IOS_ARCHES[] = { "arm64", "x86_64" };
static const modern_target_t MODERN_TARGETS[] = {
    { "android", "android-arm64-v8a", "aarch64-linux-android21", "aarch64-linux-android", PROJECT_PLATFORM_ANDROID, 4, ANDROID_ARCHES },
    { "linux", "linux-arm64", "aarch64-linux-musl", "", PROJECT_PLATFORM_LINUX, 4, LINUX_ARCHES },
    { "windows", "windows-arm64", "aarch64-w64-mingw32", "", PROJECT_PLATFORM_WINDOWS, 4, WINDOWS_ARCHES },
    { "macos", "macos-arm64", "arm64-apple-macos11", "", PROJECT_PLATFORM_MACOS, 2, MACOS_ARCHES },
    { "ios", "ios-arm64", "arm64-apple-ios13.0", "", PROJECT_PLATFORM_IOS, 2, IOS_ARCHES }
};
static const char *const SDK_LIBRARY_FILES[MODERN_SDK_OBJECTS] = {
    "engine_context.c", "engine_input.c", "scene_runtime.c", "element.c",
    "element_2d.c", "element_3d.c", "asset_image.c"
};

static modern_compile_job_t g_jobs[MODERN_MAX_SOURCES];
static int g_job_count;
static int g_next_job;
static int g_running_jobs;
static int g_failed_jobs;
static int g_cache_ready;
static int g_android_static;
static int g_sdk_seen[MODERN_SDK_OBJECTS];
static char g_cache_objects[MODERN_SDK_OBJECTS][MODERN_PATH_CAP];
static char g_cache_root[MODERN_PATH_CAP];
static char g_cache_work[MODERN_PATH_CAP];
static char g_source_dir[MODERN_PATH_CAP];
static char g_sdk_root[MODERN_PATH_CAP];
static char g_modern_output_dir[MODERN_PATH_CAP];
static char g_product_name[128];
static char g_compiler[MODERN_PATH_CAP];
static char g_compile_flags[8192];
static char g_link_pre[8192];
static char g_link_post[12288];
static char g_target[24];
static char g_arch[32];
static char g_link_output[MODERN_PATH_CAP];
static int g_release;
static int g_target_index;
static int g_arch_index;
static modern_export_status_t g_status = MODERN_EXPORT_IDLE;
static char g_architectures[4][32];
static int g_architecture_count;
static int g_architecture_index;
static char g_apk_stage[MODERN_PATH_CAP];

typedef struct {
    const modern_target_t *target;
    const char *architecture;
    int architecture_index;
} target_choice_t;

static void appendf(char *out, size_t cap, const char *format, ...) {
    size_t used = strlen(out);
    if (used >= cap) return;
    va_list args;
    va_start(args, format);
    vsnprintf(out + used, cap - used, format, args);
    va_end(args);
}

static void append_quoted(char *out, size_t cap, const char *value) {
#if defined(_WIN32)
    appendf(out, cap, "\"");
    for (const char *p = value; *p; p++) {
        if (*p == '"') appendf(out, cap, "\\\"");
        else appendf(out, cap, "%c", *p);
    }
    appendf(out, cap, "\"");
#else
    appendf(out, cap, "'");
    for (const char *p = value; *p; p++) {
        if (*p == '\'') appendf(out, cap, "'\\''");
        else appendf(out, cap, "%c", *p);
    }
    appendf(out, cap, "'");
#endif
}

static void append_sdk_expanded_flag(char *out, size_t cap, const char *flag) {
    static const char marker[] = "${REBAX_SDK_ROOT}";
    const char *p = flag;
    while (*p) {
        if (strncmp(p, marker, sizeof(marker) - 1) == 0) {
            appendf(out, cap, "\"%s\"", g_sdk_root);
            p += sizeof(marker) - 1;
        } else {
            appendf(out, cap, "%c", *p++);
        }
    }
}

static const modern_target_t *find_target(const char *platform, int *index) {
    for (int i = 0; i < (int)(sizeof(MODERN_TARGETS) / sizeof(MODERN_TARGETS[0])); i++) {
        if (strcmp(MODERN_TARGETS[i].name, platform) == 0) {
            if (index) *index = i;
            return &MODERN_TARGETS[i];
        }
    }
    return NULL;
}

static int target_arch_index(const modern_target_t *target, const char *architecture) {
    if (target == NULL || architecture == NULL) return -1;
    for (int i = 0; i < target->arch_count; i++) {
        if (strcmp(target->architectures[i], architecture) == 0) return i;
    }
    return -1;
}

static const char *platform_sdk_name(const modern_target_t *target, int arch_index) {
    static const char *const android[] = { "android-arm64-v8a", "android-armeabi-v7a", "android-x86", "android-x86_64" };
    static const char *const linux[] = { "linux-arm64", "linux-arm", "linux-x86", "linux-x86_64" };
    static const char *const windows[] = { "windows-arm64", "windows-arm", "windows-x86", "windows-x86_64" };
    static const char *const macos[] = { "macos-arm64", "macos-x86_64" };
    static const char *const ios[] = { "ios-arm64", "ios-simulator-x86_64" };
    if (strcmp(target->name, "android") == 0) return android[arch_index];
    if (strcmp(target->name, "linux") == 0) return linux[arch_index];
    if (strcmp(target->name, "windows") == 0) return windows[arch_index];
    if (strcmp(target->name, "macos") == 0) return macos[arch_index];
    return ios[arch_index];
}

static const char *target_triple(const modern_target_t *target, int arch_index) {
    static const char *const android[] = { "aarch64-linux-android21", "armv7a-linux-androideabi21", "i686-linux-android21", "x86_64-linux-android21" };
    static const char *const linux[] = { "aarch64-linux-musl", "armv7-linux-musleabihf", "i686-linux-musl", "x86_64-linux-musl" };
    static const char *const windows[] = { "aarch64-w64-mingw32", "armv7-w64-mingw32", "i686-w64-mingw32", "x86_64-w64-mingw32" };
    static const char *const macos[] = { "arm64-apple-macos11", "x86_64-apple-macos10.13" };
    static const char *const ios[] = { "arm64-apple-ios13.0", "x86_64-apple-ios13.0-simulator" };
    if (strcmp(target->name, "android") == 0) return android[arch_index];
    if (strcmp(target->name, "linux") == 0) return linux[arch_index];
    if (strcmp(target->name, "windows") == 0) return windows[arch_index];
    if (strcmp(target->name, "macos") == 0) return macos[arch_index];
    return ios[arch_index];
}

static const char *target_libdir(const modern_target_t *target, int arch_index) {
    static const char *const android[] = { "aarch64-linux-android", "arm-linux-androideabi", "i686-linux-android", "x86_64-linux-android" };
    return strcmp(target->name, "android") == 0 ? android[arch_index] : "";
}

static void slugify(const char *value, char *out, size_t cap) {
    size_t n = 0;
    for (const unsigned char *p = (const unsigned char *)value; *p && n + 1 < cap; p++) {
        int invalid = *p < 32 || strchr("/\\:*?\"<>|", *p) != NULL;
        if (invalid) {
            if (n > 0 && out[n - 1] != '_') out[n++] = '_';
        } else {
            out[n++] = (char)*p;
        }
    }
    while (n > 0 && (out[n - 1] == ' ' || out[n - 1] == '_' || out[n - 1] == '.')) n--;
    if (n == 0) { snprintf(out, cap, "game"); return; }
    out[n] = '\0';
}

static int is_host_macos(void) {
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

static int is_regular_file(const char *path) {
    return rebax_fs_exists(path) && !rebax_fs_is_dir(path);
}

static int require_file(const char *path, const char *description) {
    if (is_regular_file(path)) return 1;
    log_pushf("[modern] FAILED: %s is missing: %s", description, path);
    g_status = MODERN_EXPORT_FAILED;
    return 0;
}

typedef struct {
    const char *needle;
    const char *required_subdir;
    char path[MODERN_PATH_CAP];
} runtime_find_t;

static void find_runtime_file(const char *path, void *user_data) {
    runtime_find_t *find = (runtime_find_t *)user_data;
    if (find->path[0] != '\0') return;
    const char *base = strrchr(path, '/');
    base = base ? base + 1 : path;
    if (strncmp(base, find->needle, strlen(find->needle)) != 0) return;
    size_t n = strlen(base);
    if (n < 3 || strcmp(base + n - 2, ".a") != 0) return;
    if (find->required_subdir && find->required_subdir[0]) {
        char needle[128];
        snprintf(needle, sizeof(needle), "/%s/", find->required_subdir);
        if (strstr(path, needle) == NULL) return;
    }
    snprintf(find->path, sizeof(find->path), "%s", path);
}

static int locate_builtins(const char *sdk_root, const char *libdir, char *out, size_t cap) {
    char runtime[MODERN_PATH_CAP];
    snprintf(runtime, sizeof(runtime), "%s/runtime/lib", sdk_root);
    runtime_find_t find;
    memset(&find, 0, sizeof(find));
    find.needle = "libclang_rt.builtins";
    find.required_subdir = libdir;
    rebax_fs_walk_files(runtime, find_runtime_file, &find);
    if (find.path[0] == '\0') return 0;
    snprintf(out, cap, "%s", find.path);
    return 1;
}

static void add_frameworks(const char *sdk_root, const char *const *names) {
    char path[MODERN_PATH_CAP];
    for (int i = 0; names[i] != NULL; i++) {
        snprintf(path, sizeof(path), "%s/standard/Frameworks/%s.framework", sdk_root, names[i]);
        if (rebax_fs_is_dir(path)) appendf(g_link_post, sizeof(g_link_post), " -framework %s", names[i]);
        else {
            snprintf(path, sizeof(path), "%s/graphics/Frameworks/%s.framework", sdk_root, names[i]);
            if (rebax_fs_is_dir(path)) appendf(g_link_post, sizeof(g_link_post), " -F\"%s/graphics/Frameworks\" -framework %s", sdk_root, names[i]);
        }
    }
}

static int set_compiler(void) {
    char path[MODERN_PATH_CAP], target_path[MODERN_PATH_CAP];
#if defined(_WIN32)
    const char *suffix = ".exe";
#else
    const char *suffix = "";
#endif
    snprintf(target_path, sizeof(target_path), "%s/%s-%s/clang/bin/clang%s", rebax_toolchains_dir(), g_target, g_arch, suffix);
    snprintf(path, sizeof(path), "%s", target_path);
    if (!is_regular_file(path)) snprintf(path, sizeof(path), "%s/clang/bin/clang%s", rebax_toolchains_dir(), suffix);
    if (!is_regular_file(path)) {
        log_pushf("[modern] FAILED: clang is missing for %s-%s. Install a target compiler at %s or the shared host compiler at %s/clang/bin/clang%s.",
                  g_target, g_arch, target_path, rebax_toolchains_dir(), suffix);
        g_status = MODERN_EXPORT_FAILED;
        return 0;
    }
    snprintf(g_compiler, sizeof(g_compiler), "%s", path);
    return 1;
}

static int configure_flags(const modern_target_t *target, int arch_index) {
    char standard[MODERN_PATH_CAP], runtime[MODERN_PATH_CAP], graphics[MODERN_PATH_CAP];
    char empty_sysroot[MODERN_PATH_CAP], builtins[MODERN_PATH_CAP];
    const char *triple = target_triple(target, arch_index);
    const char *libdir = target_libdir(target, arch_index);
    snprintf(standard, sizeof(standard), "%s/standard", g_sdk_root);
    snprintf(runtime, sizeof(runtime), "%s/runtime", g_sdk_root);
    snprintf(graphics, sizeof(graphics), "%s/graphics", g_sdk_root);
    snprintf(empty_sysroot, sizeof(empty_sysroot), "%s/Temp/modern-empty-sysroot", rebax_root_dir());
    g_compile_flags[0] = g_link_pre[0] = g_link_post[0] = '\0';
    rebax_fs_mkdir_p(empty_sysroot);

    appendf(g_compile_flags, sizeof(g_compile_flags), "--target=%s -std=c11 -Wall -Wextra", triple);
    if (g_release) appendf(g_compile_flags, sizeof(g_compile_flags), " -Os -DNDEBUG -ffunction-sections -fdata-sections");
    else appendf(g_compile_flags, sizeof(g_compile_flags), " -O0 -g -ffunction-sections -fdata-sections");
    if (strcmp(g_target, "linux") == 0) {
        appendf(g_compile_flags, sizeof(g_compile_flags), " -nostdlibinc -isystem \"%s/include\" -isystem \"%s/include\"", runtime, standard);
        appendf(g_link_pre, sizeof(g_link_pre), "--target=%s -nostdlib -static -fuse-ld=lld -Wl,--eh-frame-hdr", triple);
        appendf(g_link_pre, sizeof(g_link_pre), " -Wl,--gc-sections%s", g_release ? " -s" : "");
        appendf(g_link_pre, sizeof(g_link_pre), " \"%s/lib/crt1.o\" \"%s/lib/crti.o\"", standard, standard);
        appendf(g_link_post, sizeof(g_link_post), "-L\"%s/lib\" -L\"%s/lib\" -L\"%s/lib\" -lSDL2 -lm -lc -lgcc \"%s/lib/crtn.o\"", graphics, runtime, standard, standard);
        snprintf(empty_sysroot, sizeof(empty_sysroot), "%s/lib/crt1.o", standard);
        if (!require_file(empty_sysroot, "Linux startup object crt1.o")) return 0;
        snprintf(empty_sysroot, sizeof(empty_sysroot), "%s/lib/crti.o", standard);
        if (!require_file(empty_sysroot, "Linux startup object crti.o")) return 0;
        snprintf(empty_sysroot, sizeof(empty_sysroot), "%s/lib/crtn.o", standard);
        if (!require_file(empty_sysroot, "Linux startup object crtn.o")) return 0;
        snprintf(empty_sysroot, sizeof(empty_sysroot), "%s/lib/libSDL2.a", graphics);
        if (!require_file(empty_sysroot, "Linux SDL2 static library")) return 0;
        appendf(g_link_post, sizeof(g_link_post), " -Wl,-rpath,'$ORIGIN'");
    } else if (strcmp(g_target, "windows") == 0) {
        appendf(g_compile_flags, sizeof(g_compile_flags), " --sysroot=\"%s\"", standard);
        appendf(g_link_pre, sizeof(g_link_pre), "--target=%s --sysroot=\"%s\" -fuse-ld=lld -static -nodefaultlibs -Wl,--gc-sections%s", triple, standard, g_release ? " -s" : "");
        appendf(g_link_post, sizeof(g_link_post), "-L\"%s/lib\" -L\"%s/lib\"", runtime, graphics);
        snprintf(empty_sysroot, sizeof(empty_sysroot), "%s/lib/libSDL2.a", graphics);
        if (is_regular_file(empty_sysroot)) {
            appendf(g_link_post, sizeof(g_link_post), " -lmingw32 -lSDL2main -lSDL2 -ldinput8 -ldxguid -ldxerr8 -luser32 -lgdi32 -lwinmm -limm32 -lole32 -loleaut32 -lshell32 -lsetupapi -lversion -luuid -lws2_32 -lbcrypt -lmingwex -lmoldname -lmsvcrt -lkernel32 -ladvapi32");
        } else {
            snprintf(empty_sysroot, sizeof(empty_sysroot), "%s/lib/libSDL2.dll.a", graphics);
            if (!require_file(empty_sysroot, "Windows SDL2 import library")) return 0;
            appendf(g_link_post, sizeof(g_link_post), " -lmingw32 -lSDL2main \"%s/lib/libSDL2.dll.a\" -lws2_32 -lbcrypt -lmingwex -lmoldname -lmsvcrt -lkernel32 -ladvapi32 -lshell32 -luser32", graphics);
        }
        if (!locate_builtins(g_sdk_root, "", builtins, sizeof(builtins))) {
            log_pushf("[modern] FAILED: the Windows compiler runtime is missing under %s/runtime/lib.", g_sdk_root);
            g_status = MODERN_EXPORT_FAILED;
            return 0;
        }
        appendf(g_link_post, sizeof(g_link_post), " \"%s\"", builtins);
    } else if (strcmp(g_target, "android") == 0) {
        char static_lib[MODERN_PATH_CAP], static_cxx[MODERN_PATH_CAP], static_abi[MODERN_PATH_CAP], unwind_lib[MODERN_PATH_CAP];
        snprintf(static_lib, sizeof(static_lib), "%s/static/lib/libSDL2.a", graphics);
        snprintf(static_cxx, sizeof(static_cxx), "%s/lib/%s/libc++_static.a", runtime, libdir);
        snprintf(static_abi, sizeof(static_abi), "%s/lib/%s/libc++abi.a", runtime, libdir);
        snprintf(unwind_lib, sizeof(unwind_lib), "%s/lib/%s/libunwind.a", runtime, libdir);
        if (!is_regular_file(static_lib) || !is_regular_file(static_cxx) || !is_regular_file(static_abi)) {
            log_pushf("[modern] FAILED: the Android SDK package is outdated, static libraries are missing: %s, %s, or %s. Delete the sdks folder of this platform and download the latest SDK package.", static_lib, static_cxx, static_abi);
            g_status = MODERN_EXPORT_FAILED;
            return 0;
        }
        g_android_static = 1;
        appendf(g_compile_flags, sizeof(g_compile_flags), " --sysroot=\"%s\" -fPIC -DSDL_MAIN_HANDLED=1", standard);
        appendf(g_link_pre, sizeof(g_link_pre), "--target=%s --sysroot=\"%s\" -fuse-ld=lld -nodefaultlibs -shared -Wl,-soname,libmain.so -Wl,--gc-sections%s", triple, standard, g_release ? " -s" : "");
        if (g_android_static) {
            char map_path[MODERN_PATH_CAP];
            snprintf(map_path, sizeof(map_path), "%s/Temp/modern-android.map", rebax_root_dir());
            FILE *map = fopen(map_path, "w");
            if (map == NULL) {
                log_pushf("[modern] FAILED: cannot write the Android export symbol list: %s", map_path);
                g_status = MODERN_EXPORT_FAILED;
                return 0;
            }
            fputs("{\n global:\n  Java_*;\n  JNI_*;\n  SDL_main;\n local:\n  *;\n};\n", map);
            fclose(map);
            appendf(g_link_pre, sizeof(g_link_pre), " -Wl,--version-script=\"%s\" -Wl,-u,Java_org_libsdl_app_SDLActivity_nativeInit -Wl,-u,Java_org_libsdl_app_HIDDeviceManager_HIDDeviceRegisterCallback", map_path);
            appendf(g_link_post, sizeof(g_link_post), "-L\"%s/lib/%s\" -L\"%s/usr/lib/%s/21\" -L\"%s/usr/lib/%s\" -L\"%s/static/lib\"", runtime, libdir, standard, libdir, standard, libdir, graphics);
            appendf(g_link_post, sizeof(g_link_post), " -lSDL2 -lc++_static -lc++abi");
            if (is_regular_file(unwind_lib)) appendf(g_link_post, sizeof(g_link_post), " -lunwind");
            appendf(g_link_post, sizeof(g_link_post), " -lm -llog -landroid -lOpenSLES -lGLESv1_CM -lGLESv2 -lc -ldl");
        } else {
            appendf(g_link_post, sizeof(g_link_post), "-L\"%s/lib/%s\" -L\"%s/usr/lib/%s/21\" -L\"%s/usr/lib/%s\" -L\"%s/lib\"", runtime, libdir, standard, libdir, standard, libdir, graphics);
            appendf(g_link_post, sizeof(g_link_post), " -lSDL2 -liconv -lm -llog -landroid -lOpenSLES -lc -ldl");
        }
        if (!locate_builtins(g_sdk_root, libdir, builtins, sizeof(builtins))) {
            log_pushf("[modern] FAILED: the Android compiler runtime for %s is missing under %s/runtime/lib.", libdir, g_sdk_root);
            g_status = MODERN_EXPORT_FAILED;
            return 0;
        }
        appendf(g_link_post, sizeof(g_link_post), " \"%s\"", builtins);
    } else {
        appendf(g_compile_flags, sizeof(g_compile_flags), " -nostdlibinc -isysroot \"%s\" -isystem \"%s/include\" -F\"%s/standard/Frameworks\" -F\"%s/graphics/Frameworks\"", empty_sysroot, standard, g_sdk_root, g_sdk_root);
        appendf(g_link_pre, sizeof(g_link_pre), "--target=%s -isysroot \"%s\" -fuse-ld=lld -nostdlib -Wl,-dead_strip", triple, empty_sysroot);
        appendf(g_link_post, sizeof(g_link_post), "-L\"%s/lib\" -L\"%s/lib\" -L\"%s/lib\" -lSDL2 -liconv -lSystem", standard, runtime, graphics);
        snprintf(empty_sysroot, sizeof(empty_sysroot), "%s/graphics/lib/libSDL2.a", g_sdk_root);
        if (!require_file(empty_sysroot, "SDL2 static library")) return 0;
        if (strcmp(g_target, "macos") == 0) {
            static const char *const frameworks[] = { "Cocoa", "CoreAudio", "AudioToolbox", "CoreVideo", "IOKit", "ForceFeedback", "Carbon", "CoreServices", "CoreFoundation", "Foundation", "AppKit", "Metal", "QuartzCore", "OpenGL", "CoreHaptics", "GameController", NULL };
            char runtime_lib[MODERN_PATH_CAP];
            snprintf(runtime_lib, sizeof(runtime_lib), "%s/runtime/lib/libclang_rt.osx.a", g_sdk_root);
            if (!require_file(runtime_lib, "macOS compiler runtime libclang_rt.osx.a")) return 0;
            appendf(g_link_post, sizeof(g_link_post), " \"%s\"", runtime_lib);
            add_frameworks(g_sdk_root, frameworks);
        } else {
            static const char *const frameworks[] = { "UIKit", "Foundation", "CoreGraphics", "QuartzCore", "OpenGLES", "Metal", "AudioToolbox", "AVFoundation", "CoreAudio", "CoreMotion", "CoreHaptics", "GameController", "CoreBluetooth", "CoreFoundation", NULL };
            char runtime_lib[MODERN_PATH_CAP];
            snprintf(runtime_lib, sizeof(runtime_lib), "%s/runtime/lib/%s", g_sdk_root,
                     arch_index == 1 ? "libclang_rt.iossim.a" : "libclang_rt.ios.a");
            if (!require_file(runtime_lib, "iOS compiler runtime archive")) return 0;
            appendf(g_link_post, sizeof(g_link_post), " \"%s\"", runtime_lib);
            add_frameworks(g_sdk_root, frameworks);
        }
    }
    appendf(g_compile_flags, sizeof(g_compile_flags), " -I\"%s\" -I\"%s/graphics/include\" -I\"%s/graphics/include/SDL2\"", g_source_dir, g_sdk_root, g_sdk_root);
    for (int i = 0; i < g_extra_incs_count; i++) {
        appendf(g_compile_flags, sizeof(g_compile_flags), " ");
        append_sdk_expanded_flag(g_compile_flags, sizeof(g_compile_flags), g_extra_incs[i]);
    }
    return 1;
}

static unsigned long long hash_flags(const char *text) {
    unsigned long long hash = 1469598103934665603ull;
    for (const unsigned char *p = (const unsigned char *)text; *p; p++) {
        hash ^= *p;
        hash *= 1099511628211ull;
    }
    return hash;
}

static int sdk_index_for_source(const char *path) {
    const char *base = strrchr(path, '/');
    base = base ? base + 1 : path;
    for (int i = 0; i < MODERN_SDK_OBJECTS; i++) {
        if (strcmp(base, SDK_LIBRARY_FILES[i]) == 0) {
            const char *relative = path + strlen(g_source_dir);
            if (*relative == '/') relative++;
            if (strchr(relative, '/') == NULL) return i;
        }
    }
    return -1;
}

static void build_compile_command(modern_compile_job_t *job) {
    job->command[0] = '\0';
    append_quoted(job->command, sizeof(job->command), g_compiler);
    appendf(job->command, sizeof(job->command), " %s -c ", g_compile_flags);
    append_quoted(job->command, sizeof(job->command), job->source);
    appendf(job->command, sizeof(job->command), " -o ");
    append_quoted(job->command, sizeof(job->command), job->object);
    appendf(job->command, sizeof(job->command), " 2>&1");
}

static int add_compile_job(const char *source, const char *object, int sdk_index) {
    if (g_job_count >= MODERN_MAX_SOURCES) {
        log_pushf("[modern] FAILED: the export contains more than %d C source files.", MODERN_MAX_SOURCES);
        g_status = MODERN_EXPORT_FAILED;
        return 0;
    }
    modern_compile_job_t *job = &g_jobs[g_job_count++];
    memset(job, 0, sizeof(*job));
    snprintf(job->source, sizeof(job->source), "%s", source);
    snprintf(job->object, sizeof(job->object), "%s", object);
    job->sdk_index = sdk_index;
    build_compile_command(job);
    return 1;
}

static void collect_source(const char *path, void *user_data) {
    (void)user_data;
    size_t length = strlen(path);
    if (length >= 4 && strcmp(path + length - 4, ".cpp") == 0) {
        log_pushf("[modern] FAILED: C++ source files are not supported by this C-only exporter: %s", path);
        g_status = MODERN_EXPORT_FAILED;
        return;
    }
    if (length < 3 || strcmp(path + length - 2, ".c") != 0 || g_status == MODERN_EXPORT_FAILED) return;
    int sdk_index = sdk_index_for_source(path);
    if (sdk_index >= 0) g_sdk_seen[sdk_index] = 1;
    if (sdk_index >= 0 && g_cache_ready) return;
    char object[MODERN_PATH_CAP];
    if (sdk_index >= 0) snprintf(object, sizeof(object), "%s/sdk_%d.o", g_cache_work, sdk_index);
    else snprintf(object, sizeof(object), "%s/app_%03d.o", g_build_dir, g_job_count);
    add_compile_job(path, object, sdk_index);
}

static void reset_shared_export_data(void) {
    export_scene_reset();
    g_used_types_count = 0;
    g_sdk_types_count = 0;
    g_image_paths_count = 0;
    g_extra_libs_count = 0;
    g_extra_incs_count = 0;
    g_needs_image_loader = 0;
    g_needs_engine_context_stub = 0;
    memset(g_image_specs, 0, sizeof(g_image_specs));
}

static int write_android_entry(void) {
    char path[MODERN_PATH_CAP];
    snprintf(path, sizeof(path), "%s/android_entry.c", g_source_dir);
    FILE *file = fopen(path, "wb");
    if (!file) return 0;
    fputs("extern int main(int, char **);\nint SDL_main(int argc, char **argv) { return main(argc, argv); }\n", file);
    return fclose(file) == 0;
}

static int prepare_cache(void) {
    char key[1024];
    snprintf(key, sizeof(key), "%s|%s|%s|%s|%s|%s", g_target, g_arch, MODERN_SDK_VERSION,
             g_compiler, g_compile_flags, g_link_post);
    unsigned long long hash = hash_flags(key);
    snprintf(g_cache_root, sizeof(g_cache_root), "%s/Cache/modern_sdk/%s-%s-%s-%016llx",
             rebax_root_dir(), g_target, g_arch, MODERN_SDK_VERSION, hash);
    snprintf(g_cache_work, sizeof(g_cache_work), "%s/cache_work", g_build_dir);
    rebax_fs_mkdir_p(g_cache_work);
    char ready[MODERN_PATH_CAP];
    snprintf(ready, sizeof(ready), "%s/.ready", g_cache_root);
    g_cache_ready = is_regular_file(ready);
    for (int i = 0; i < MODERN_SDK_OBJECTS; i++) {
        snprintf(g_cache_objects[i], sizeof(g_cache_objects[i]), "%s/sdk_%d.o", g_cache_root, i);
        if (g_cache_ready && !is_regular_file(g_cache_objects[i])) g_cache_ready = 0;
    }
    if (g_cache_ready) log_pushf("[modern] SDK cache hit: %s", g_cache_root);
    else log_pushf("[modern] SDK cache miss; building platform SDK objects for %s-%s.", g_target, g_arch);
    return 1;
}

static int write_scene_data(const char *project_root) {
    char main_path[MODERN_PATH_CAP], table_path[MODERN_PATH_CAP];
    snprintf(main_path, sizeof(main_path), "%s/scene_data.c", g_source_dir);
    snprintf(table_path, sizeof(table_path), "%s/.scene_table_tmp", g_source_dir);
    g_codegen_main = fopen(main_path, "w");
    g_codegen_table = fopen(table_path, "w");
    if (g_codegen_main == NULL || g_codegen_table == NULL) {
        if (g_codegen_main != NULL) fclose(g_codegen_main);
        if (g_codegen_table != NULL) fclose(g_codegen_table);
        g_codegen_main = NULL;
        g_codegen_table = NULL;
        log_push("[modern] FAILED: could not create scene_data.c.");
        return 0;
    }
    fputs("#include \"scene_runtime.h\"\n\n", g_codegen_main);
    g_declared_interfaces_count = 0;
    g_used_types_count = 0;
    log_pushf("[modern] project folder: %s", project_root);
    export_scene_walk_rscene_files(project_root, export_scene_generate_from_file);
    fclose(g_codegen_main);
    fclose(g_codegen_table);
    g_codegen_main = NULL;
    g_codegen_table = NULL;
    if (g_used_types_count == 0) {
        log_push("[modern] FAILED: no valid nodes were found in any saved project scene.");
        remove(table_path);
        return 0;
    }
    for (int i = 0; i < g_used_types_count; i++) {
        log_pushf("[modern] scene usage found: %s", g_used_types[i]);
    }
    FILE *append_target = fopen(main_path, "a");
    FILE *table_source = fopen(table_path, "r");
    if (append_target != NULL && table_source != NULL) {
        fprintf(append_target, "const scene_table_entry_t g_scenes[] = {\n");
        char line[256];
        while (fgets(line, sizeof(line), table_source) != NULL) fputs(line, append_target);
        fprintf(append_target, "};\nconst int g_scene_count = sizeof(g_scenes) / sizeof(g_scenes[0]);\n");
    }
    if (append_target != NULL) fclose(append_target);
    if (table_source != NULL) fclose(table_source);
    remove(table_path);
    return 1;
}

static void prepare_export(void) {
    const project_settings_t *settings = current_project_get_settings();
    const modern_target_t *target = find_target(g_target, &g_target_index);
    g_arch_index = target_arch_index(target, g_arch);
    if (settings == NULL || !current_project_is_open()) {
        log_push("[modern] FAILED: open a project before exporting.");
        g_status = MODERN_EXPORT_FAILED;
        return;
    }
    if (settings->type != PROJECT_TYPE_MODERN) {
        log_push("[modern] FAILED: the modern exporter only accepts projects whose project_type is modern.");
        g_status = MODERN_EXPORT_FAILED;
        return;
    }
    if (settings->export_method != EXPORT_METHOD_COMPILER) {
        log_push("[modern] FAILED: this project selects a non-compiler export_method; choose the compiler method in project settings.");
        g_status = MODERN_EXPORT_FAILED;
        return;
    }
    if (target == NULL || g_arch_index < 0) {
        log_pushf("[modern] FAILED: unsupported target architecture: %s-%s.", g_target, g_arch);
        g_status = MODERN_EXPORT_FAILED;
        return;
    }
    if ((settings->platforms & target->project_flag) == 0) {
        log_pushf("[modern] FAILED: project.rebax does not enable the %s platform in its platforms field.", g_target);
        g_status = MODERN_EXPORT_FAILED;
        return;
    }
    if (strcmp(g_target, "ios") == 0 && !is_host_macos()) {
        log_push("[modern] FAILED: iOS export requires a macOS host with Apple platform tools and frameworks; this host cannot produce an iOS app.");
        g_status = MODERN_EXPORT_FAILED;
        return;
    }
    if (!set_compiler()) return;
    snprintf(g_sdk_root, sizeof(g_sdk_root), "%s/%s", rebax_sdks_dir(), platform_sdk_name(target, g_arch_index));
    if (!rebax_fs_is_dir(g_sdk_root)) {
        log_pushf("[modern] FAILED: the %s-%s SDK package is missing: %s. Install that architecture's SDK package.", g_target, g_arch, g_sdk_root);
        g_status = MODERN_EXPORT_FAILED;
        return;
    }
    char sdk_part[MODERN_PATH_CAP];
    snprintf(sdk_part, sizeof(sdk_part), "%s/standard", g_sdk_root);
    if (!rebax_fs_is_dir(sdk_part)) { log_pushf("[modern] FAILED: SDK prerequisite folder is missing: %s", sdk_part); g_status = MODERN_EXPORT_FAILED; return; }
    snprintf(sdk_part, sizeof(sdk_part), "%s/runtime", g_sdk_root);
    if (!rebax_fs_is_dir(sdk_part)) { log_pushf("[modern] FAILED: SDK prerequisite folder is missing: %s", sdk_part); g_status = MODERN_EXPORT_FAILED; return; }
    snprintf(sdk_part, sizeof(sdk_part), "%s/graphics", g_sdk_root);
    if (!rebax_fs_is_dir(sdk_part)) { log_pushf("[modern] FAILED: SDL2 graphics SDK folder is missing: %s", sdk_part); g_status = MODERN_EXPORT_FAILED; return; }
    snprintf(sdk_part, sizeof(sdk_part), "%s/graphics/include/SDL2/SDL.h", g_sdk_root);
    if (!require_file(sdk_part, "SDL2 development header")) return;
    char old_workspace[MODERN_PATH_CAP];
    snprintf(old_workspace, sizeof(old_workspace), "%s/modern_export", rebax_temp_export_dir());
    snprintf(g_build_dir, sizeof(g_build_dir), "%s", old_workspace);
    if (!rebax_fs_remove_recursive(g_build_dir) || !rebax_fs_mkdir_p(g_build_dir)) {
        log_pushf("[modern] FAILED: cannot reset the temporary export workspace: %s", g_build_dir);
        g_status = MODERN_EXPORT_FAILED;
        return;
    }
    snprintf(g_source_dir, sizeof(g_source_dir), "%s/src", g_build_dir);
    if (!rebax_fs_mkdir_p(g_source_dir)) {
        log_pushf("[modern] FAILED: cannot create temporary source directory: %s", g_source_dir);
        g_status = MODERN_EXPORT_FAILED;
        return;
    }
    {
        char config_path[MODERN_PATH_CAP];
        const project_settings_t *project = current_project_get_settings();
        snprintf(config_path, sizeof(config_path), "%s/rebax_project_config.h", g_source_dir);
        FILE *config = fopen(config_path, "w");
        if (config == NULL) {
            log_push("[modern] FAILED: could not create project display configuration.");
            g_status = MODERN_EXPORT_FAILED;
            return;
        }
        fprintf(config, "#ifndef REBAX_PROJECT_CONFIG_H\n#define REBAX_PROJECT_CONFIG_H\n#define REBAX_PROJECT_WIDTH %d\n#define REBAX_PROJECT_HEIGHT %d\n#endif\n",
                project->display_width, project->display_height);
        fclose(config);
    }
    if (!configure_flags(target, g_arch_index)) return;

    reset_shared_export_data();
    node_source_use("modern");
    snprintf(g_nodes_src_dir, sizeof(g_nodes_src_dir), "%s/rebax/modern", rebax_sdks_dir());
    const char *project_root = current_project_get_path();
    log_pushf("[modern] preparing %s-%s export for %s (%s).", g_target, g_arch, g_product_name, g_release ? "Release" : "Debug");
    if (!write_scene_data(project_root)) {
        g_status = MODERN_EXPORT_FAILED;
        return;
    }
    if (!export_nodes_copy_matched(g_source_dir)) {
        log_push("[modern] FAILED: one or more scene node types have no matching modern SDK source.");
        g_status = MODERN_EXPORT_FAILED;
        return;
    }
    if (!export_codegen_write_sdk_files(g_source_dir)) {
        log_push("[modern] FAILED: could not generate the Native Rebax SDK source.");
        g_status = MODERN_EXPORT_FAILED;
        return;
    }
    if (!export_project_copy_sources(project_root, g_source_dir)) {
        g_status = MODERN_EXPORT_FAILED;
        return;
    }
    if (strcmp(g_target, "android") == 0 && !write_android_entry()) {
        log_push("[modern] FAILED: could not create the Android SDL entry source.");
        g_status = MODERN_EXPORT_FAILED;
        return;
    }
    if (!prepare_cache()) {
        log_push("[modern] FAILED: cannot prepare the SDK object cache.");
        g_status = MODERN_EXPORT_FAILED;
        return;
    }
    memset(g_sdk_seen, 0, sizeof(g_sdk_seen));
    memset(g_jobs, 0, sizeof(g_jobs));
    g_job_count = g_next_job = g_running_jobs = g_failed_jobs = 0;
    rebax_fs_walk_files(g_source_dir, collect_source, NULL);
    if (g_status == MODERN_EXPORT_FAILED) return;
    for (int i = 0; i < MODERN_SDK_OBJECTS; i++) {
        if (!g_sdk_seen[i]) {
            log_pushf("[modern] FAILED: required modern SDK source is missing from the export tree: %s", SDK_LIBRARY_FILES[i]);
            g_status = MODERN_EXPORT_FAILED;
            return;
        }
    }
    if (g_job_count == 0) {
        log_push("[modern] FAILED: there are no application sources to compile.");
        g_status = MODERN_EXPORT_FAILED;
        return;
    }
    if (strcmp(g_target, "android") == 0) {
        snprintf(g_link_output, sizeof(g_link_output), "%s/libmain.so", g_build_dir);
    } else if (strcmp(g_target, "windows") == 0) {
        snprintf(g_link_output, sizeof(g_link_output), "%s/%s.exe", g_build_dir, g_product_name);
    } else {
        snprintf(g_link_output, sizeof(g_link_output), "%s/%s", g_build_dir, g_product_name);
    }
    log_pushf("[modern] queued %d compiler jobs with up to %d parallel clang processes.", g_job_count, MODERN_MAX_ACTIVE);
    g_status = MODERN_EXPORT_RUNNING;
}

static int start_next_jobs(void) {
    while (g_running_jobs < MODERN_MAX_ACTIVE && g_next_job < g_job_count) {
        modern_compile_job_t *job = &g_jobs[g_next_job++];
        if (!rebax_fs_mkdir_p(job->sdk_index >= 0 ? g_cache_work : g_build_dir)) {
            log_pushf("[modern] FAILED: cannot create an object output directory for %s.", job->source);
            return 0;
        }
        if (!shell_step_start(&job->process, job->command)) {
            log_pushf("[modern] FAILED: could not start clang for %s; verify the compiler package and host execution permissions.", job->source);
            return 0;
        }
        job->started = 1;
        g_running_jobs++;
        log_pushf("[modern] clang started: %s", job->source);
    }
    return 1;
}

static void cancel_active_jobs(void) {
    for (int i = 0; i < g_job_count; i++) {
        if (g_jobs[i].started && !g_jobs[i].done) {
            shell_step_cancel(&g_jobs[i].process);
            g_jobs[i].done = 1;
        }
    }
    g_running_jobs = 0;
}

static int finish_compilation(void) {
    char ready[MODERN_PATH_CAP];
    if (!g_cache_ready) {
        if (!rebax_fs_mkdir_p(g_cache_root)) {
            log_pushf("[modern] FAILED: cannot create SDK cache folder: %s", g_cache_root);
            return 0;
        }
        for (int i = 0; i < MODERN_SDK_OBJECTS; i++) {
            int found = 0;
            for (int j = 0; j < g_job_count; j++) {
                if (g_jobs[j].sdk_index == i) {
                    if (!rebax_fs_copy_file(g_jobs[j].object, g_cache_objects[i])) {
                        log_pushf("[modern] FAILED: could not store SDK object %s in the cache.", SDK_LIBRARY_FILES[i]);
                        return 0;
                    }
                    found = 1;
                    break;
                }
            }
            if (!found) {
                log_pushf("[modern] FAILED: compiler did not produce a cached SDK object for %s.", SDK_LIBRARY_FILES[i]);
                return 0;
            }
        }
        snprintf(ready, sizeof(ready), "%s/.ready", g_cache_root);
        if (!rebax_fs_write_atomic(ready, MODERN_SDK_VERSION, sizeof(MODERN_SDK_VERSION) - 1)) {
            log_pushf("[modern] FAILED: cannot mark the SDK cache as complete: %s", ready);
            return 0;
        }
        g_cache_ready = 1;
        log_pushf("[modern] SDK object cache created: %s", g_cache_root);
    }
    return 1;
}

static void build_link_command(char *command, size_t cap) {
    command[0] = '\0';
    append_quoted(command, cap, g_compiler);
    appendf(command, cap, " %s", g_link_pre);
    for (int i = 0; i < g_job_count; i++) {
        modern_compile_job_t *job = &g_jobs[i];
        if (job->sdk_index >= 0) continue;
        appendf(command, cap, " ");
        append_quoted(command, cap, job->object);
    }
    for (int i = 0; i < MODERN_SDK_OBJECTS; i++) {
        appendf(command, cap, " ");
        append_quoted(command, cap, g_cache_objects[i]);
    }
    for (int i = 0; i < g_extra_libs_count; i++) appendf(command, cap, " %s", g_extra_libs[i]);
    appendf(command, cap, " %s -o ", g_link_post);
    append_quoted(command, cap, g_link_output);
    appendf(command, cap, " 2>&1");
}

static int run_link(void) {
    export_shell_step_t process;
    memset(&process, 0, sizeof(process));
    char command[MODERN_COMMAND_CAP];
    build_link_command(command, sizeof(command));
    log_pushf("[modern] linking target executable: %s", g_link_output);
    export_trace("trace: starting clang linker");
    if (!shell_step_start(&process, command)) {
        log_push("[modern] FAILED: could not start clang linker; check compiler execution permissions.");
        return 0;
    }
    int ok = 0;
    while (shell_step_poll(&process, &ok)) { }
    if (!ok) {
        log_push("[modern] FAILED: clang linker exited unsuccessfully; compiler and linker diagnostics are shown above.");
        return 0;
    }
    if (!is_regular_file(g_link_output)) {
        log_pushf("[modern] FAILED: clang returned success but did not create the output: %s", g_link_output);
        return 0;
    }
    log_push("[modern] clang link completed successfully.");
    return 1;
}

static int is_runtime_resource(const char *path) {
    static const char *const skipped[] = {
        ".rscene", ".rebax", ".c", ".cc", ".cpp", ".cxx", ".cp", ".c++", ".h", ".hpp", ".s"
    };
    const char *dot = strrchr(path, '.');
    if (dot != NULL) {
        for (size_t i = 0; i < sizeof(skipped) / sizeof(skipped[0]); i++) {
#if defined(_WIN32)
            if (_stricmp(dot, skipped[i]) == 0) return 0;
#else
            if (strcasecmp(dot, skipped[i]) == 0) return 0;
#endif
        }
    }
    return 1;
}

static void copy_tree_file(const char *path, void *user_data) {
    modern_copy_context_t *context = (modern_copy_context_t *)user_data;
    if (context->failed) return;
    if (context->resources_only && !is_runtime_resource(path)) return;
    const char *relative = path + strlen(context->source_root);
    while (*relative == '/' || *relative == '\\') relative++;
    char destination[MODERN_PATH_CAP];
    snprintf(destination, sizeof(destination), "%s/%s", context->destination_root, relative);
    for (char *p = destination; *p; p++) if (*p == '\\') *p = '/';
    char parent[MODERN_PATH_CAP];
    snprintf(parent, sizeof(parent), "%s", destination);
    char *slash = strrchr(parent, '/');
    if (slash) {
        *slash = '\0';
        if (!rebax_fs_mkdir_p(parent)) { context->failed = 1; return; }
    }
    if (!rebax_fs_copy_file(path, destination)) context->failed = 1;
}

static int copy_tree(const char *source, const char *destination, int resources_only) {
    if (!rebax_fs_is_dir(source)) return 1;
    if (!rebax_fs_mkdir_p(destination)) return 0;
    modern_copy_context_t context;
    snprintf(context.source_root, sizeof(context.source_root), "%s", source);
    snprintf(context.destination_root, sizeof(context.destination_root), "%s", destination);
    context.failed = 0;
    context.resources_only = resources_only;
    rebax_fs_walk_files(source, copy_tree_file, &context);
    return !context.failed;
}

static int set_executable_permission(const char *path) {
#if defined(_WIN32)
    (void)path;
    return 1;
#else
    return chmod(path, 0755) == 0;
#endif
}

static int copy_checked(const char *source, const char *destination, const char *description) {
    if (!is_regular_file(source)) {
        log_pushf("[modern] FAILED: required runtime file %s is missing: %s", description, source);
        return 0;
    }
    char parent[MODERN_PATH_CAP];
    snprintf(parent, sizeof(parent), "%s", destination);
    char *slash = strrchr(parent, '/');
    if (slash) { *slash = '\0'; if (!rebax_fs_mkdir_p(parent)) return 0; }
    if (!rebax_fs_copy_file(source, destination)) {
        log_pushf("[modern] FAILED: could not copy %s to %s.", description, destination);
        return 0;
    }
    return 1;
}

static int copy_project_assets(const char *destination) {
    char source[MODERN_PATH_CAP];
    snprintf(source, sizeof(source), "%s/assets", current_project_get_path());
    if (!rebax_fs_is_dir(source)) return 1;
    return copy_tree(source, destination, 1);
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
            "</dict></plist>\n", g_product_name, g_product_name);
    return fclose(file) == 0;
}

static int package_desktop(void) {
    char bundle[MODERN_PATH_CAP], executable[MODERN_PATH_CAP], destination[MODERN_PATH_CAP];
    if (g_architecture_count > 1) snprintf(bundle, sizeof(bundle), "%s/%s-%s", g_modern_output_dir, g_product_name, g_arch);
    else snprintf(bundle, sizeof(bundle), "%s/%s", g_modern_output_dir, g_product_name);
    if (!rebax_fs_mkdir_p(bundle)) {
        log_pushf("[modern] FAILED: cannot create output directory: %s", bundle);
        return 0;
    }
    if (strcmp(g_target, "windows") == 0) snprintf(executable, sizeof(executable), "%s.exe", g_product_name);
    else snprintf(executable, sizeof(executable), "%s", g_product_name);
    snprintf(destination, sizeof(destination), "%s/%s", bundle, executable);
    if (!copy_checked(g_link_output, destination, "the game executable")) return 0;
    if (strcmp(g_target, "windows") != 0 && !set_executable_permission(destination)) {
        log_pushf("[modern] FAILED: could not mark the exported executable as runnable: %s", destination);
        return 0;
    }
    char assets_destination[MODERN_PATH_CAP];
    snprintf(assets_destination, sizeof(assets_destination), "%s/assets", bundle);
    if (!copy_project_assets(assets_destination)) {
        log_push("[modern] FAILED: could not copy the project's runtime assets.");
        return 0;
    }
    char runtime_src[MODERN_PATH_CAP], runtime_dst[MODERN_PATH_CAP];
    if (strcmp(g_target, "windows") == 0) {
        snprintf(runtime_src, sizeof(runtime_src), "%s/graphics/lib/libSDL2.a", g_sdk_root);
        if (!is_regular_file(runtime_src)) {
            snprintf(runtime_src, sizeof(runtime_src), "%s/graphics/bin/SDL2.dll", g_sdk_root);
            snprintf(runtime_dst, sizeof(runtime_dst), "%s/SDL2.dll", bundle);
            if (!copy_checked(runtime_src, runtime_dst, "SDL2.dll")) return 0;
        }
    } else if (strcmp(g_target, "linux") == 0) {
        char static_sdl[MODERN_PATH_CAP];
        snprintf(static_sdl, sizeof(static_sdl), "%s/graphics/lib/libSDL2.a", g_sdk_root);
        if (is_regular_file(static_sdl)) return 1;
        snprintf(runtime_src, sizeof(runtime_src), "%s/graphics/lib/libSDL2.so", g_sdk_root);
        if (is_regular_file(runtime_src)) {
            snprintf(runtime_dst, sizeof(runtime_dst), "%s/libSDL2.so", bundle);
            if (!copy_checked(runtime_src, runtime_dst, "SDL2 runtime")) return 0;
        }
        snprintf(runtime_src, sizeof(runtime_src), "%s/graphics/lib/libiconv.so", g_sdk_root);
        if (is_regular_file(runtime_src)) {
            snprintf(runtime_dst, sizeof(runtime_dst), "%s/libiconv.so", bundle);
            if (!copy_checked(runtime_src, runtime_dst, "iconv runtime")) return 0;
        }
    } else if (strcmp(g_target, "macos") == 0) {
        snprintf(runtime_src, sizeof(runtime_src), "%s/graphics/lib/libSDL2.dylib", g_sdk_root);
        if (is_regular_file(runtime_src)) {
            snprintf(runtime_dst, sizeof(runtime_dst), "%s/libSDL2.dylib", bundle);
            if (!copy_checked(runtime_src, runtime_dst, "SDL2 runtime")) return 0;
        }
    }
    log_pushf("[modern] desktop export ready: %s", bundle);
    return 1;
}

static int package_android(void) {
    char stage[MODERN_PATH_CAP], path[MODERN_PATH_CAP], source[MODERN_PATH_CAP], output[MODERN_PATH_CAP];
    char architecture_dir[MODERN_PATH_CAP], key_dir[MODERN_PATH_CAP];
    snprintf(stage, sizeof(stage), "%s", g_apk_stage);
    if (g_architecture_index == 0) rebax_fs_remove_recursive(stage);
    snprintf(architecture_dir, sizeof(architecture_dir), "%s/lib/%s", stage, g_arch);
    if (!rebax_fs_mkdir_p(architecture_dir)) {
        log_pushf("[modern] FAILED: cannot create APK staging directory: %s", architecture_dir);
        return 0;
    }
    if (g_architecture_index == 0) {
        snprintf(source, sizeof(source), "%s/graphics/dex/classes.dex", g_sdk_root);
        snprintf(path, sizeof(path), "%s/classes.dex", stage);
        if (!copy_checked(source, path, "Android SDL classes.dex")) return 0;
    }
    if (!g_android_static) {
        static const char *const libraries[] = { "libSDL2.so", "libiconv.so" };
        for (int i = 0; i < 2; i++) {
            snprintf(source, sizeof(source), "%s/graphics/lib/%s", g_sdk_root, libraries[i]);
            snprintf(path, sizeof(path), "%s/%s", architecture_dir, libraries[i]);
            if (!copy_checked(source, path, libraries[i])) return 0;
        }
        const modern_target_t *target = &MODERN_TARGETS[g_target_index];
        int lib_index = target_arch_index(target, g_arch);
        const char *libdir = target_libdir(target, lib_index);
        snprintf(source, sizeof(source), "%s/runtime/lib/%s/libc++_shared.so", g_sdk_root, libdir);
        snprintf(path, sizeof(path), "%s/libc++_shared.so", architecture_dir);
        if (!copy_checked(source, path, "Android libc++_shared.so")) return 0;
    }
    snprintf(path, sizeof(path), "%s/libmain.so", architecture_dir);
    if (!copy_checked(g_link_output, path, "libmain.so")) return 0;
    if (g_architecture_index == 0) {
        snprintf(path, sizeof(path), "%s/assets", stage);
        if (!copy_project_assets(path)) {
            log_push("[modern] FAILED: could not copy project assets into the APK.");
            return 0;
        }
    }
    if (g_architecture_index + 1 < g_architecture_count) {
        log_pushf("[modern] Android architecture ready: %s", g_arch);
        return 1;
    }
    if (!rebax_fs_mkdir_p(g_modern_output_dir)) {
        log_pushf("[modern] FAILED: cannot create APK output folder: %s", g_modern_output_dir);
        return 0;
    }
    snprintf(output, sizeof(output), "%s/%s.apk", g_modern_output_dir, g_product_name);
    snprintf(key_dir, sizeof(key_dir), "%s/keystore", rebax_root_dir());
    apk_pack_config_t config;
    memset(&config, 0, sizeof(config));
    config.stage_dir = stage;
    config.out_path = output;
    config.keys_dir = key_dir;
    config.package_name = "org.rebax.game";
    config.app_label = g_product_name;
    config.version_name = MODERN_SDK_VERSION;
    config.version_code = 1;
    config.min_sdk = 24;
    config.target_sdk = 28;
    config.activity_name = g_android_static ? "org.rebax.game.GameActivity" : NULL;
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

static int package_ios(void) {
    char app[MODERN_PATH_CAP], path[MODERN_PATH_CAP];
    snprintf(app, sizeof(app), "%s/%s.app", g_modern_output_dir, g_product_name);
    if (!rebax_fs_mkdir_p(app)) {
        log_pushf("[modern] FAILED: cannot create iOS app bundle: %s", app);
        return 0;
    }
    snprintf(path, sizeof(path), "%s/%s", app, g_product_name);
    if (!copy_checked(g_link_output, path, "iOS executable")) return 0;
    snprintf(path, sizeof(path), "%s/Info.plist", app);
    if (!write_ios_plist(path)) {
        log_pushf("[modern] FAILED: could not write iOS bundle metadata: %s", path);
        return 0;
    }
    snprintf(path, sizeof(path), "%s/assets", app);
    if (!copy_project_assets(path)) {
        log_push("[modern] FAILED: could not copy project assets into the iOS bundle.");
        return 0;
    }
    log_pushf("[modern] iOS app bundle ready: %s (unsigned)", app);
    return 1;
}

static int package_output(void) {
    if (strcmp(g_target, "android") == 0) return package_android();
    if (strcmp(g_target, "ios") == 0) return package_ios();
    return package_desktop();
}

int modern_export_start_multi(const char *name, int release, const char *output_dir,
                              const char *platform, const char *const *architectures, int count) {
    if (g_status == MODERN_EXPORT_RUNNING) return 0;
    log_reset();
    export_trace_begin();
    export_trace("trace: modern export operation started");
    g_status = MODERN_EXPORT_RUNNING;
    g_release = release;
    snprintf(g_target, sizeof(g_target), "%s", platform ? platform : "");
    g_architecture_count = count > 0 && count <= 4 ? count : 1;
    for (int i = 0; i < g_architecture_count; i++) snprintf(g_architectures[i], sizeof(g_architectures[i]), "%s", architectures && architectures[i] ? architectures[i] : "");
    g_architecture_index = 0;
    snprintf(g_arch, sizeof(g_arch), "%s", g_architectures[0]);
    slugify(name ? name : "game", g_product_name, sizeof(g_product_name));
    snprintf(g_modern_output_dir, sizeof(g_modern_output_dir), "%s", output_dir ? output_dir : "");
    snprintf(g_apk_stage, sizeof(g_apk_stage), "%s/modern_apk_stage", rebax_temp_export_dir());
    log_pushf("[modern] export requested: %s-%s (%s).", g_target, g_arch, g_product_name);
    if (g_modern_output_dir[0] == '\0') {
        log_push("[modern] FAILED: choose a non-empty output folder.");
        g_status = MODERN_EXPORT_FAILED;
        return 1;
    }
    if (find_target(g_target, NULL) == NULL) {
        log_pushf("[modern] FAILED: unknown target platform: %s.", g_target);
        g_status = MODERN_EXPORT_FAILED;
        return 1;
    }
    prepare_export();
    return 1;
}

int modern_export_start(const char *name, int release, const char *output_dir,
                        const char *platform, const char *architecture) {
    const char *architectures[1] = { architecture };
    return modern_export_start_multi(name, release, output_dir, platform, architectures, 1);
}

void modern_export_update(void) {
    if (g_status != MODERN_EXPORT_RUNNING) return;
    if (g_next_job < g_job_count && !start_next_jobs()) {
        cancel_active_jobs();
        g_status = MODERN_EXPORT_FAILED;
        return;
    }
    int pending = 0;
    for (int i = 0; i < g_job_count; i++) {
        modern_compile_job_t *job = &g_jobs[i];
        if (!job->started || job->done) continue;
        int ok = 0;
        if (shell_step_poll(&job->process, &ok)) { pending++; continue; }
        job->done = 1;
        g_running_jobs--;
        if (!ok) {
            g_failed_jobs++;
            log_pushf("[modern] FAILED: clang compilation failed for %s (%s-%s); complete compiler output is shown above.", job->source, g_target, g_arch);
        } else {
            log_pushf("[modern] compiled: %s", job->source);
        }
    }
    if (g_failed_jobs > 0) {
        cancel_active_jobs();
        g_status = MODERN_EXPORT_FAILED;
        return;
    }
    if (g_next_job < g_job_count || pending > 0 || g_running_jobs > 0) return;
    if (!finish_compilation()) {
        g_status = MODERN_EXPORT_FAILED;
        return;
    }
    if (!run_link()) {
        g_status = MODERN_EXPORT_FAILED;
        return;
    }
    if (!package_output()) {
        g_status = MODERN_EXPORT_FAILED;
        return;
    }
    if (g_architecture_index + 1 < g_architecture_count) {
        g_architecture_index++;
        snprintf(g_arch, sizeof(g_arch), "%s", g_architectures[g_architecture_index]);
        prepare_export();
        return;
    }
    export_trace("trace: modern export completed");
    g_status = MODERN_EXPORT_SUCCESS;
}

void modern_export_cancel(void) {
    if (g_status == MODERN_EXPORT_RUNNING) {
        cancel_active_jobs();
        log_push("[modern] export cancelled by user.");
        export_trace("trace: modern export cancelled");
    }
    g_status = MODERN_EXPORT_IDLE;
}

modern_export_status_t modern_export_get_status(void) {
    return g_status;
}

const char *modern_export_current_architecture(void) { return g_arch; }
int modern_export_architecture_index(void) { return g_architecture_index; }
int modern_export_architecture_count(void) { return g_architecture_count; }
