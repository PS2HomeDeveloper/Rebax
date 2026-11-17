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
#define MODERN_SDK_OBJECTS 7

typedef struct {
    char source[MODERN_PATH_CAP];
    char object[MODERN_PATH_CAP];
    char command[MODERN_COMMAND_CAP];
    export_shell_step_t process;
    int sdk_index;
    int started;
    int done;
} modern_compile_job_t;

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
static int g_sdk_seen[MODERN_SDK_OBJECTS];
static char g_cache_objects[MODERN_SDK_OBJECTS][MODERN_PATH_CAP];
static char g_cache_root[MODERN_PATH_CAP];
static char g_cache_work[MODERN_PATH_CAP];
char g_modern_source_dir[MODERN_PATH_CAP];
char g_modern_sdk_root[MODERN_PATH_CAP];
char g_modern_output_dir[MODERN_PATH_CAP];
char g_modern_product_name[128];
static char g_modern_compiler[MODERN_PATH_CAP];
char g_modern_compile_flags[8192];
char g_modern_link_pre[8192];
char g_modern_link_post[12288];
char g_modern_target[24];
char g_modern_arch[32];
char g_modern_link_output[MODERN_PATH_CAP];
int g_modern_release;
int g_modern_target_index;
static int g_modern_arch_index;
modern_export_status_t g_modern_status = MODERN_EXPORT_IDLE;
static char g_architectures[4][32];
int g_modern_architecture_count;
int g_modern_architecture_index;
char g_modern_apk_stage[MODERN_PATH_CAP];


static void append_quoted(char *out, size_t cap, const char *value) {
#if defined(_WIN32)
    modern_appendf(out, cap, "\"");
    for (const char *p = value; *p; p++) {
        if (*p == '"') modern_appendf(out, cap, "\\\"");
        else modern_appendf(out, cap, "%c", *p);
    }
    modern_appendf(out, cap, "\"");
#else
    modern_appendf(out, cap, "'");
    for (const char *p = value; *p; p++) {
        if (*p == '\'') modern_appendf(out, cap, "'\\''");
        else modern_appendf(out, cap, "%c", *p);
    }
    modern_appendf(out, cap, "'");
#endif
}

static void append_sdk_expanded_flag(char *out, size_t cap, const char *flag) {
    static const char marker[] = "${REBAX_SDK_ROOT}";
    const char *p = flag;
    while (*p) {
        if (strncmp(p, marker, sizeof(marker) - 1) == 0) {
            modern_appendf(out, cap, "\"%s\"", g_modern_sdk_root);
            p += sizeof(marker) - 1;
        } else {
            modern_appendf(out, cap, "%c", *p++);
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

static int set_compiler(void) {
    char path[MODERN_PATH_CAP], target_path[MODERN_PATH_CAP];
#if defined(_WIN32)
    const char *suffix = ".exe";
#else
    const char *suffix = "";
#endif
    snprintf(target_path, sizeof(target_path), "%s/%s-%s/clang/bin/clang%s", rebax_toolchains_dir(), g_modern_target, g_modern_arch, suffix);
    snprintf(path, sizeof(path), "%s", target_path);
    if (!modern_is_regular_file(path)) snprintf(path, sizeof(path), "%s/clang/bin/clang%s", rebax_toolchains_dir(), suffix);
    if (!modern_is_regular_file(path)) {
        log_pushf("[modern] FAILED: clang is missing for %s-%s. Install a target compiler at %s or the shared host compiler at %s/clang/bin/clang%s.",
                  g_modern_target, g_modern_arch, target_path, rebax_toolchains_dir(), suffix);
        g_modern_status = MODERN_EXPORT_FAILED;
        return 0;
    }
    snprintf(g_modern_compiler, sizeof(g_modern_compiler), "%s", path);
    return 1;
}

static const modern_platform_t *current_platform(void) {
    static const modern_platform_t *const platforms[] = { &modern_platform_android, &modern_platform_linux, &modern_platform_windows, &modern_platform_macos, &modern_platform_ios };
    return platforms[g_modern_target_index];
}

static int configure_flags(const modern_target_t *target, int arch_index) {
    modern_config_t config;
    const modern_platform_t *platform = current_platform();
    config.triple = target_triple(target, arch_index);
    config.libdir = modern_target_libdir(target, arch_index);
    config.arch_index = arch_index;
    snprintf(config.standard, sizeof(config.standard), "%s/standard", g_modern_sdk_root);
    snprintf(config.runtime, sizeof(config.runtime), "%s/runtime", g_modern_sdk_root);
    snprintf(config.graphics, sizeof(config.graphics), "%s/graphics", g_modern_sdk_root);
    snprintf(config.empty_sysroot, sizeof(config.empty_sysroot), "%s/Temp/modern-empty-sysroot", rebax_root_dir());
    g_modern_compile_flags[0] = g_modern_link_pre[0] = g_modern_link_post[0] = '\0';
    rebax_fs_mkdir_p(config.empty_sysroot);

    modern_appendf(g_modern_compile_flags, sizeof(g_modern_compile_flags), "--target=%s -std=c11 -Wall -Wextra", config.triple);
    if (g_modern_release) modern_appendf(g_modern_compile_flags, sizeof(g_modern_compile_flags), " -Os -DNDEBUG -ffunction-sections -fdata-sections");
    else modern_appendf(g_modern_compile_flags, sizeof(g_modern_compile_flags), " -O0 -g -ffunction-sections -fdata-sections");
    if (!platform->configure(&config)) return 0;
    modern_appendf(g_modern_compile_flags, sizeof(g_modern_compile_flags), " -I\"%s\" -I\"%s/graphics/include\" -I\"%s/graphics/include/SDL2\"", g_modern_source_dir, g_modern_sdk_root, g_modern_sdk_root);
    for (int i = 0; i < g_extra_incs_count; i++) {
        modern_appendf(g_modern_compile_flags, sizeof(g_modern_compile_flags), " ");
        append_sdk_expanded_flag(g_modern_compile_flags, sizeof(g_modern_compile_flags), g_extra_incs[i]);
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
            const char *relative = path + strlen(g_modern_source_dir);
            if (*relative == '/') relative++;
            if (strchr(relative, '/') == NULL) return i;
        }
    }
    return -1;
}

static void build_compile_command(modern_compile_job_t *job) {
    job->command[0] = '\0';
    append_quoted(job->command, sizeof(job->command), g_modern_compiler);
    modern_appendf(job->command, sizeof(job->command), " %s -c ", g_modern_compile_flags);
    append_quoted(job->command, sizeof(job->command), job->source);
    modern_appendf(job->command, sizeof(job->command), " -o ");
    append_quoted(job->command, sizeof(job->command), job->object);
    modern_appendf(job->command, sizeof(job->command), " 2>&1");
}

static int add_compile_job(const char *source, const char *object, int sdk_index) {
    if (g_job_count >= MODERN_MAX_SOURCES) {
        log_pushf("[modern] FAILED: the export contains more than %d C source files.", MODERN_MAX_SOURCES);
        g_modern_status = MODERN_EXPORT_FAILED;
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
        g_modern_status = MODERN_EXPORT_FAILED;
        return;
    }
    if (length < 3 || strcmp(path + length - 2, ".c") != 0 || g_modern_status == MODERN_EXPORT_FAILED) return;
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

static int prepare_cache(void) {
    char key[1024];
    snprintf(key, sizeof(key), "%s|%s|%s|%s|%s|%s", g_modern_target, g_modern_arch, MODERN_SDK_VERSION,
             g_modern_compiler, g_modern_compile_flags, g_modern_link_post);
    unsigned long long hash = hash_flags(key);
    snprintf(g_cache_root, sizeof(g_cache_root), "%s/Cache/modern_sdk/%s-%s-%s-%016llx",
             rebax_root_dir(), g_modern_target, g_modern_arch, MODERN_SDK_VERSION, hash);
    snprintf(g_cache_work, sizeof(g_cache_work), "%s/cache_work", g_build_dir);
    rebax_fs_mkdir_p(g_cache_work);
    char ready[MODERN_PATH_CAP];
    snprintf(ready, sizeof(ready), "%s/.ready", g_cache_root);
    g_cache_ready = modern_is_regular_file(ready);
    for (int i = 0; i < MODERN_SDK_OBJECTS; i++) {
        snprintf(g_cache_objects[i], sizeof(g_cache_objects[i]), "%s/sdk_%d.o", g_cache_root, i);
        if (g_cache_ready && !modern_is_regular_file(g_cache_objects[i])) g_cache_ready = 0;
    }
    if (g_cache_ready) log_pushf("[modern] SDK cache hit: %s", g_cache_root);
    else log_pushf("[modern] SDK cache miss; building platform SDK objects for %s-%s.", g_modern_target, g_modern_arch);
    return 1;
}

static int write_scene_data(const char *project_root) {
    char main_path[MODERN_PATH_CAP], table_path[MODERN_PATH_CAP];
    snprintf(main_path, sizeof(main_path), "%s/scene_data.c", g_modern_source_dir);
    snprintf(table_path, sizeof(table_path), "%s/.scene_table_tmp", g_modern_source_dir);
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
    const modern_target_t *target = find_target(g_modern_target, &g_modern_target_index);
    g_modern_arch_index = modern_target_arch_index(target, g_modern_arch);
    if (settings == NULL || !current_project_is_open()) {
        log_push("[modern] FAILED: open a project before exporting.");
        g_modern_status = MODERN_EXPORT_FAILED;
        return;
    }
    if (settings->type != PROJECT_TYPE_MODERN) {
        log_push("[modern] FAILED: the modern exporter only accepts projects whose project_type is modern.");
        g_modern_status = MODERN_EXPORT_FAILED;
        return;
    }
    if (settings->export_method != EXPORT_METHOD_COMPILER) {
        log_push("[modern] FAILED: this project selects a non-compiler export_method; choose the compiler method in project settings.");
        g_modern_status = MODERN_EXPORT_FAILED;
        return;
    }
    if (target == NULL || g_modern_arch_index < 0) {
        log_pushf("[modern] FAILED: unsupported target architecture: %s-%s.", g_modern_target, g_modern_arch);
        g_modern_status = MODERN_EXPORT_FAILED;
        return;
    }
    if ((settings->platforms & target->project_flag) == 0) {
        log_pushf("[modern] FAILED: project.rebax does not enable the %s platform in its platforms field.", g_modern_target);
        g_modern_status = MODERN_EXPORT_FAILED;
        return;
    }
    if (current_platform()->check_host != NULL && !current_platform()->check_host()) {
        log_push("[modern] FAILED: iOS export requires a macOS host with Apple platform tools and frameworks; this host cannot produce an iOS app.");
        g_modern_status = MODERN_EXPORT_FAILED;
        return;
    }
    if (!set_compiler()) return;
    snprintf(g_modern_sdk_root, sizeof(g_modern_sdk_root), "%s/%s", rebax_sdks_dir(), platform_sdk_name(target, g_modern_arch_index));
    if (!rebax_fs_is_dir(g_modern_sdk_root)) {
        log_pushf("[modern] FAILED: the %s-%s SDK package is missing: %s. Install that architecture's SDK package.", g_modern_target, g_modern_arch, g_modern_sdk_root);
        g_modern_status = MODERN_EXPORT_FAILED;
        return;
    }
    char sdk_part[MODERN_PATH_CAP];
    snprintf(sdk_part, sizeof(sdk_part), "%s/standard", g_modern_sdk_root);
    if (!rebax_fs_is_dir(sdk_part)) { log_pushf("[modern] FAILED: SDK prerequisite folder is missing: %s", sdk_part); g_modern_status = MODERN_EXPORT_FAILED; return; }
    snprintf(sdk_part, sizeof(sdk_part), "%s/runtime", g_modern_sdk_root);
    if (!rebax_fs_is_dir(sdk_part)) { log_pushf("[modern] FAILED: SDK prerequisite folder is missing: %s", sdk_part); g_modern_status = MODERN_EXPORT_FAILED; return; }
    snprintf(sdk_part, sizeof(sdk_part), "%s/graphics", g_modern_sdk_root);
    if (!rebax_fs_is_dir(sdk_part)) { log_pushf("[modern] FAILED: SDL2 graphics SDK folder is missing: %s", sdk_part); g_modern_status = MODERN_EXPORT_FAILED; return; }
    snprintf(sdk_part, sizeof(sdk_part), "%s/graphics/include/SDL2/SDL.h", g_modern_sdk_root);
    if (!modern_require_file(sdk_part, "SDL2 development header")) return;
    char old_workspace[MODERN_PATH_CAP];
    snprintf(old_workspace, sizeof(old_workspace), "%s/modern_export", rebax_temp_export_dir());
    snprintf(g_build_dir, sizeof(g_build_dir), "%s", old_workspace);
    if (!rebax_fs_remove_recursive(g_build_dir) || !rebax_fs_mkdir_p(g_build_dir)) {
        log_pushf("[modern] FAILED: cannot reset the temporary export workspace: %s", g_build_dir);
        g_modern_status = MODERN_EXPORT_FAILED;
        return;
    }
    snprintf(g_modern_source_dir, sizeof(g_modern_source_dir), "%s/src", g_build_dir);
    if (!rebax_fs_mkdir_p(g_modern_source_dir)) {
        log_pushf("[modern] FAILED: cannot create temporary source directory: %s", g_modern_source_dir);
        g_modern_status = MODERN_EXPORT_FAILED;
        return;
    }
    {
        char config_path[MODERN_PATH_CAP];
        const project_settings_t *project = current_project_get_settings();
        snprintf(config_path, sizeof(config_path), "%s/rebax_project_config.h", g_modern_source_dir);
        FILE *config = fopen(config_path, "w");
        if (config == NULL) {
            log_push("[modern] FAILED: could not create project display configuration.");
            g_modern_status = MODERN_EXPORT_FAILED;
            return;
        }
        fprintf(config, "#ifndef REBAX_PROJECT_CONFIG_H\n#define REBAX_PROJECT_CONFIG_H\n#define REBAX_PROJECT_WIDTH %d\n#define REBAX_PROJECT_HEIGHT %d\n#endif\n",
                project->display_width, project->display_height);
        fclose(config);
    }
    if (!configure_flags(target, g_modern_arch_index)) return;

    reset_shared_export_data();
    node_source_use("modern");
    snprintf(g_nodes_src_dir, sizeof(g_nodes_src_dir), "%s/rebax/modern", rebax_sdks_dir());
    const char *project_root = current_project_get_path();
    log_pushf("[modern] preparing %s-%s export for %s (%s).", g_modern_target, g_modern_arch, g_modern_product_name, g_modern_release ? "Release" : "Debug");
    if (!write_scene_data(project_root)) {
        g_modern_status = MODERN_EXPORT_FAILED;
        return;
    }
    if (!export_nodes_copy_matched(g_modern_source_dir)) {
        log_push("[modern] FAILED: one or more scene node types have no matching modern SDK source.");
        g_modern_status = MODERN_EXPORT_FAILED;
        return;
    }
    if (!export_codegen_write_sdk_files(g_modern_source_dir)) {
        log_push("[modern] FAILED: could not generate the Native Rebax SDK source.");
        g_modern_status = MODERN_EXPORT_FAILED;
        return;
    }
    if (!export_project_copy_sources(project_root, g_modern_source_dir)) {
        g_modern_status = MODERN_EXPORT_FAILED;
        return;
    }
    if (current_platform()->prepare_sources != NULL && !current_platform()->prepare_sources()) {
        g_modern_status = MODERN_EXPORT_FAILED;
        return;
    }
    if (!prepare_cache()) {
        log_push("[modern] FAILED: cannot prepare the SDK object cache.");
        g_modern_status = MODERN_EXPORT_FAILED;
        return;
    }
    memset(g_sdk_seen, 0, sizeof(g_sdk_seen));
    memset(g_jobs, 0, sizeof(g_jobs));
    g_job_count = g_next_job = g_running_jobs = g_failed_jobs = 0;
    rebax_fs_walk_files(g_modern_source_dir, collect_source, NULL);
    if (g_modern_status == MODERN_EXPORT_FAILED) return;
    for (int i = 0; i < MODERN_SDK_OBJECTS; i++) {
        if (!g_sdk_seen[i]) {
            log_pushf("[modern] FAILED: required modern SDK source is missing from the export tree: %s", SDK_LIBRARY_FILES[i]);
            g_modern_status = MODERN_EXPORT_FAILED;
            return;
        }
    }
    if (g_job_count == 0) {
        log_push("[modern] FAILED: there are no application sources to compile.");
        g_modern_status = MODERN_EXPORT_FAILED;
        return;
    }
    current_platform()->link_output(g_modern_link_output, sizeof(g_modern_link_output));
    log_pushf("[modern] queued %d compiler jobs with up to %d parallel clang processes.", g_job_count, MODERN_MAX_ACTIVE);
    g_modern_status = MODERN_EXPORT_RUNNING;
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
    append_quoted(command, cap, g_modern_compiler);
    modern_appendf(command, cap, " %s", g_modern_link_pre);
    for (int i = 0; i < g_job_count; i++) {
        modern_compile_job_t *job = &g_jobs[i];
        if (job->sdk_index >= 0) continue;
        modern_appendf(command, cap, " ");
        append_quoted(command, cap, job->object);
    }
    for (int i = 0; i < MODERN_SDK_OBJECTS; i++) {
        modern_appendf(command, cap, " ");
        append_quoted(command, cap, g_cache_objects[i]);
    }
    for (int i = 0; i < g_extra_libs_count; i++) modern_appendf(command, cap, " %s", g_extra_libs[i]);
    modern_appendf(command, cap, " %s -o ", g_modern_link_post);
    append_quoted(command, cap, g_modern_link_output);
    modern_appendf(command, cap, " 2>&1");
}

static int run_link(void) {
    export_shell_step_t process;
    memset(&process, 0, sizeof(process));
    char command[MODERN_COMMAND_CAP];
    build_link_command(command, sizeof(command));
    log_pushf("[modern] linking target executable: %s", g_modern_link_output);
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
    if (!modern_is_regular_file(g_modern_link_output)) {
        log_pushf("[modern] FAILED: clang returned success but did not create the output: %s", g_modern_link_output);
        return 0;
    }
    log_push("[modern] clang link completed successfully.");
    return 1;
}

static int package_output(void) {
    return current_platform()->package();
}

int modern_export_start_multi(const char *name, int release, const char *output_dir,
                              const char *platform, const char *const *architectures, int count) {
    if (g_modern_status == MODERN_EXPORT_RUNNING) return 0;
    log_reset();
    export_trace_begin();
    export_trace("trace: modern export operation started");
    g_modern_status = MODERN_EXPORT_RUNNING;
    g_modern_release = release;
    snprintf(g_modern_target, sizeof(g_modern_target), "%s", platform ? platform : "");
    g_modern_architecture_count = count > 0 && count <= 4 ? count : 1;
    for (int i = 0; i < g_modern_architecture_count; i++) snprintf(g_architectures[i], sizeof(g_architectures[i]), "%s", architectures && architectures[i] ? architectures[i] : "");
    g_modern_architecture_index = 0;
    snprintf(g_modern_arch, sizeof(g_modern_arch), "%s", g_architectures[0]);
    slugify(name ? name : "game", g_modern_product_name, sizeof(g_modern_product_name));
    snprintf(g_modern_output_dir, sizeof(g_modern_output_dir), "%s", output_dir ? output_dir : "");
    snprintf(g_modern_apk_stage, sizeof(g_modern_apk_stage), "%s/modern_apk_stage", rebax_temp_export_dir());
    log_pushf("[modern] export requested: %s-%s (%s).", g_modern_target, g_modern_arch, g_modern_product_name);
    if (g_modern_output_dir[0] == '\0') {
        log_push("[modern] FAILED: choose a non-empty output folder.");
        g_modern_status = MODERN_EXPORT_FAILED;
        return 1;
    }
    if (find_target(g_modern_target, NULL) == NULL) {
        log_pushf("[modern] FAILED: unknown target platform: %s.", g_modern_target);
        g_modern_status = MODERN_EXPORT_FAILED;
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
    if (g_modern_status != MODERN_EXPORT_RUNNING) return;
    if (g_next_job < g_job_count && !start_next_jobs()) {
        cancel_active_jobs();
        g_modern_status = MODERN_EXPORT_FAILED;
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
            log_pushf("[modern] FAILED: clang compilation failed for %s (%s-%s); complete compiler output is shown above.", job->source, g_modern_target, g_modern_arch);
        } else {
            log_pushf("[modern] compiled: %s", job->source);
        }
    }
    if (g_failed_jobs > 0) {
        cancel_active_jobs();
        g_modern_status = MODERN_EXPORT_FAILED;
        return;
    }
    if (g_next_job < g_job_count || pending > 0 || g_running_jobs > 0) return;
    if (!finish_compilation()) {
        g_modern_status = MODERN_EXPORT_FAILED;
        return;
    }
    if (!run_link()) {
        g_modern_status = MODERN_EXPORT_FAILED;
        return;
    }
    if (!package_output()) {
        g_modern_status = MODERN_EXPORT_FAILED;
        return;
    }
    if (g_modern_architecture_index + 1 < g_modern_architecture_count) {
        g_modern_architecture_index++;
        snprintf(g_modern_arch, sizeof(g_modern_arch), "%s", g_architectures[g_modern_architecture_index]);
        prepare_export();
        return;
    }
    export_trace("trace: modern export completed");
    g_modern_status = MODERN_EXPORT_SUCCESS;
}

void modern_export_cancel(void) {
    if (g_modern_status == MODERN_EXPORT_RUNNING) {
        cancel_active_jobs();
        log_push("[modern] export cancelled by user.");
        export_trace("trace: modern export cancelled");
    }
    g_modern_status = MODERN_EXPORT_IDLE;
}

modern_export_status_t modern_export_get_status(void) {
    return g_modern_status;
}

const char *modern_export_current_architecture(void) { return g_modern_arch; }
int modern_export_architecture_index(void) { return g_modern_architecture_index; }
int modern_export_architecture_count(void) { return g_modern_architecture_count; }
