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

typedef struct {
    char source_root[MODERN_PATH_CAP];
    char destination_root[MODERN_PATH_CAP];
    int failed;
    int resources_only;
} modern_copy_context_t;

void modern_appendf(char *out, size_t cap, const char *format, ...) {
    size_t used = strlen(out);
    if (used >= cap) return;
    va_list args;
    va_start(args, format);
    vsnprintf(out + used, cap - used, format, args);
    va_end(args);
}

int modern_is_regular_file(const char *path) {
    return rebax_fs_exists(path) && !rebax_fs_is_dir(path);
}

int modern_require_file(const char *path, const char *description) {
    if (modern_is_regular_file(path)) return 1;
    log_pushf("[modern] FAILED: %s is missing: %s", description, path);
    g_modern_status = MODERN_EXPORT_FAILED;
    return 0;
}

int modern_fail(const char *message) {
    log_pushf("[modern] FAILED: %s", message);
    g_modern_status = MODERN_EXPORT_FAILED;
    return 0;
}

const char *modern_target_libdir(const modern_target_t *target, int arch_index) {
    static const char *const android[] = { "aarch64-linux-android", "arm-linux-androideabi", "i686-linux-android", "x86_64-linux-android" };
    return strcmp(target->name, "android") == 0 ? android[arch_index] : "";
}

int modern_target_arch_index(const modern_target_t *target, const char *architecture) {
    if (target == NULL || architecture == NULL) return -1;
    for (int i = 0; i < target->arch_count; i++) {
        if (strcmp(target->architectures[i], architecture) == 0) return i;
    }
    return -1;
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

int modern_locate_builtins(const char *sdk_root, const char *libdir, char *out, size_t cap) {
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

int modern_set_executable_permission(const char *path) {
#if defined(_WIN32)
    (void)path;
    return 1;
#else
    return chmod(path, 0755) == 0;
#endif
}

int modern_copy_checked(const char *source, const char *destination, const char *description) {
    if (!modern_is_regular_file(source)) {
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

int modern_copy_project_assets(const char *destination) {
    char source[MODERN_PATH_CAP];
    snprintf(source, sizeof(source), "%s/assets", current_project_get_path());
    if (!rebax_fs_is_dir(source)) return 1;
    return copy_tree(source, destination, 1);
}

int modern_package_desktop(const char *executable, int executable_bit, char *bundle, size_t bundle_cap) {
    char destination[MODERN_PATH_CAP], assets_destination[MODERN_PATH_CAP];
    if (g_modern_architecture_count > 1) snprintf(bundle, bundle_cap, "%s/%s-%s", g_modern_output_dir, g_modern_product_name, g_modern_arch);
    else snprintf(bundle, bundle_cap, "%s/%s", g_modern_output_dir, g_modern_product_name);
    if (!rebax_fs_mkdir_p(bundle)) {
        log_pushf("[modern] FAILED: cannot create output directory: %s", bundle);
        return 0;
    }
    snprintf(destination, sizeof(destination), "%s/%s", bundle, executable);
    if (!modern_copy_checked(g_modern_link_output, destination, "the game executable")) return 0;
    if (executable_bit && !modern_set_executable_permission(destination)) {
        log_pushf("[modern] FAILED: could not mark the exported executable as runnable: %s", destination);
        return 0;
    }
    snprintf(assets_destination, sizeof(assets_destination), "%s/assets", bundle);
    if (!modern_copy_project_assets(assets_destination)) {
        log_push("[modern] FAILED: could not copy the project's runtime assets.");
        return 0;
    }
    return 1;
}
