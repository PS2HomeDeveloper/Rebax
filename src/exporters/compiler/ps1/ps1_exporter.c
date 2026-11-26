/*
 *  ____  _____ ____    _    __  __
 * |  _ \| ____| __ )  / \   \ \/ /
 * | |_) |  _| |  _ \ / _ \   \  / 
 * |  _ <| |___| |_) / ___ \  /  \ 
 * |_| \_\_____|____/_/   \_\/_/\_\
 *
 * Rebax Engine
 * Copyright (c) 2026 PS1HomeDeveloper
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <ctype.h>

#if defined(_WIN32)
#include <io.h>
#endif

#include <stdarg.h>

#include "exporters/compiler/ps1/ps1_exporter.h"
#include "exporters/compiler/ps1/ps1_build.h"
#include "exporters/compiler/ps1/ps1_exe.h"
#include "project/current_project.h"
#include "filesystem/storage_paths.h"
#include "filesystem/rebax_fs.h"
#include "exporters/common/node_source.h"
#include "filesystem/rebax_fs.h"

#pragma GCC diagnostic ignored "-Wformat-truncation"

#include "exporters/common/export_internal.h"

static int g_build_started;
static int g_convert_started;
static char g_toolchain_root[1536];
static char g_sdk_standard_root[1536];
static char g_sdk_graphics_root[1536];

static void resolve_ps1_roots(void) {
    snprintf(g_toolchain_root, sizeof(g_toolchain_root), "%s/gcc-ps1", rebax_toolchains_dir());
    snprintf(g_sdk_standard_root, sizeof(g_sdk_standard_root), "%s/ps1/standard", rebax_sdks_dir());
    snprintf(g_sdk_graphics_root, sizeof(g_sdk_graphics_root), "%s/ps1/graphics", rebax_sdks_dir());
}

int ps1_export_start(const char *exe_name, int is_release, const char *output_dir) {
    if (g_state != EXPORT_STATE_IDLE && g_state != EXPORT_STATE_SUCCESS && g_state != EXPORT_STATE_FAILED) {
        return 0;
    }
    if (!current_project_is_open()) {
        return 0;
    }

    log_reset();
    g_build_started = 0;
    g_convert_started = 0;
    export_trace_begin();
    export_trace("trace: export start");

    strncpy(g_exe_name, exe_name, sizeof(g_exe_name) - 1);
    g_exe_name[sizeof(g_exe_name) - 1] = '\0';
    g_is_release = is_release;
    strncpy(g_output_dir, output_dir, sizeof(g_output_dir) - 1);
    g_output_dir[sizeof(g_output_dir) - 1] = '\0';

    resolve_ps1_roots();
    {
        char sdk_root[1536];
        snprintf(sdk_root, sizeof(sdk_root), "%s/ps1", rebax_sdks_dir());
        node_source_use("ps1", sdk_root);
        snprintf(g_nodes_src_dir, sizeof(g_nodes_src_dir), "%s/rebax/systems/nodes/src", sdk_root);
    }

    strncpy(g_build_dir, rebax_temp_export_dir(), sizeof(g_build_dir) - 1);
    g_build_dir[sizeof(g_build_dir) - 1] = '\0';

    {
        if (!rebax_fs_mkdir_p(g_build_dir)) { g_state=EXPORT_STATE_FAILED; return 0; }
        DIR *old=opendir(g_build_dir);
        if (old) { struct dirent *e; while((e=readdir(old))) { if(!strcmp(e->d_name,".")||!strcmp(e->d_name,"..")) continue; char q[1600]; snprintf(q,sizeof(q),"%s/%s",g_build_dir,e->d_name); rebax_fs_remove_recursive(q); } closedir(old); }
    }

    export_trace("trace: workspace cleaned");
    export_scene_reset();
    g_used_types_count = 0;
    g_sdk_types_count = 0;
    g_needs_image_loader = 0;
    g_needs_engine_context_stub = 0;
    g_extra_libs_count = 0;
    g_extra_incs_count = 0;
    g_image_paths_count = 0;
    memset(g_image_specs, 0, sizeof(g_image_specs));

    log_pushf("[exporter] starting export: %s (%s)", g_exe_name, g_is_release ? "Release" : "Debug");
    g_state = EXPORT_STATE_PREPARE_BUILD;
    return 1;
}

const char *ps2_export_poll_next_line(void);

const char *ps1_export_poll_next_line(void) {
    return ps2_export_poll_next_line();
}

void ps1_export_cancel(void) {
    shell_step_cancel(&g_step);
    ps1_build_cancel();
    g_build_started = 0;
    g_convert_started = 0;
    if (g_state != EXPORT_STATE_IDLE) {
        log_push("[exporter] export cancelled by user.");
    }
    g_state = EXPORT_STATE_IDLE;
}

ps1_export_status_t ps1_export_get_status(void) {
    switch (g_state) {
        case EXPORT_STATE_IDLE:    return PS1_EXPORT_STATUS_IDLE;
        case EXPORT_STATE_SUCCESS: return PS1_EXPORT_STATUS_SUCCESS;
        case EXPORT_STATE_FAILED:  return PS1_EXPORT_STATUS_FAILED;
        default:            return PS1_EXPORT_STATUS_RUNNING;
    }
}

void ps1_export_update(void) {
    int ok;

    switch (g_state) {

        case EXPORT_STATE_IDLE:
        case EXPORT_STATE_SUCCESS:
        case EXPORT_STATE_FAILED:
            return;

        case EXPORT_STATE_PREPARE_BUILD: {
            log_push("[exporter] scanning project scenes and generating typed node data...");
            const char *project_root = current_project_get_path();

            char src_dir[1536];
            snprintf(src_dir, sizeof(src_dir), "%s/src", g_build_dir);
            if (!rebax_fs_mkdir_p(src_dir)) { g_state=EXPORT_STATE_FAILED; return; }

            export_trace("trace: writing runtime files");
            export_codegen_write_runtime_files(src_dir);
            export_trace("trace: walking scenes");

            char main_path[1600], table_path[1600];
            snprintf(main_path, sizeof(main_path), "%s/scene_data.c", src_dir);
            snprintf(table_path, sizeof(table_path), "%s/.scene_table_tmp", src_dir);

            g_codegen_main = fopen(main_path, "w");
            g_codegen_table = fopen(table_path, "w");
            if (g_codegen_main == NULL || g_codegen_table == NULL) {
                log_push("[exporter] FAILED: could not create scene_data.c.");
                g_state = EXPORT_STATE_FAILED;
                return;
            }

            fputs("/* مولَّد تلقائياً وقت التصدير من مشاهد المشروع - لا تعدّله يدوياً */\n"
                  "#include \"scene_runtime.h\"\n\n", g_codegen_main);

            g_declared_interfaces_count = 0;
            g_used_types_count = 0;

            log_pushf("[exporter] project folder: %s", project_root);
            export_scene_walk_rscene_files(project_root, export_scene_generate_from_file);

            fclose(g_codegen_main);
            fclose(g_codegen_table);
            g_codegen_main = NULL;
            g_codegen_table = NULL;

            export_trace("trace: scenes parsed");
            if (g_used_types_count == 0) {
                log_push("[exporter] FAILED: no valid nodes found in any project scene.");
                remove(table_path);
                g_state = EXPORT_STATE_FAILED;
                return;
            }
            for (int i = 0; i < g_used_types_count; i++) {
                log_pushf("[exporter] scene usage found: %s", g_used_types[i]);
            }

            FILE *append_target = fopen(main_path, "a");
            FILE *table_src = fopen(table_path, "r");
            if (append_target != NULL && table_src != NULL) {
                fprintf(append_target, "const scene_table_entry_t g_scenes[] = {\n");
                char line_buf[256];
                while (fgets(line_buf, sizeof(line_buf), table_src) != NULL) {
                    fputs(line_buf, append_target);
                }
                fprintf(append_target, "};\nconst int g_scene_count = sizeof(g_scenes) / sizeof(g_scenes[0]);\n");
            }
            if (append_target != NULL) fclose(append_target);
            if (table_src != NULL) fclose(table_src);
            remove(table_path);

            export_trace("trace: copying node sources");
            if (!export_nodes_copy_matched(src_dir)) {
                log_push("[exporter] FAILED: one or more used node types have no matching source file.");
                g_state = EXPORT_STATE_FAILED;
                return;
            }
            export_trace("trace: writing sdk");
            if (!export_codegen_write_sdk_files(src_dir)) {
                log_push("[exporter] FAILED: could not generate the Native Rebax SDK.");
                g_state = EXPORT_STATE_FAILED;
                return;
            }
            export_trace("trace: copying project sources");
            if (!export_project_copy_sources(project_root, src_dir)) {
                g_state = EXPORT_STATE_FAILED;
                return;
            }

            if (g_image_paths_count > 0) {
                log_push("[exporter] converting project images to embedded raw pixel data...");
                if (!export_assets_convert_images(src_dir)) {
                    log_push("[exporter] FAILED: one or more image paths could not be read/decoded.");
                    g_state = EXPORT_STATE_FAILED;
                    return;
                }
            }

            if (g_needs_engine_context_stub) {
                export_codegen_write_engine_context_impl(src_dir);
            }

            log_push("[exporter] build directory ready - starting compilation...");
            g_state = EXPORT_STATE_BUILD;
            return;
        }

        case EXPORT_STATE_BUILD: {
            if (!g_build_started) {
                char src_dir[1536], output[1600], include_project[1600];
                snprintf(src_dir, sizeof(src_dir), "%s/src", g_build_dir);
                snprintf(output, sizeof(output), "%s/%s.elf", src_dir, g_exe_name);
                snprintf(include_project, sizeof(include_project), "-I%s/project", src_dir);

                const char *incs[EXPORT_MAX_EXTRA_FLAGS + 1];
                const char *libs[EXPORT_MAX_EXTRA_FLAGS];
                int inc_count = 0, lib_count = 0;
                incs[inc_count++] = include_project;
                for (int i = 0; i < g_extra_incs_count; i++) incs[inc_count++] = g_extra_incs[i];
                for (int i = 0; i < g_extra_libs_count; i++) libs[lib_count++] = g_extra_libs[i];

                ps1_build_config_t config;
                memset(&config, 0, sizeof(config));
                config.kind = PS1_BUILD_EE_ELF;
                config.toolchain_root = g_toolchain_root;
                config.sdk_standard_root = g_sdk_standard_root;
                config.sdk_graphics_root = g_sdk_graphics_root;
                config.work_dir = src_dir;
                config.output = output;
                config.release = g_is_release;
                config.incs = incs;
                config.inc_count = inc_count;
                config.libs = libs;
                config.lib_count = lib_count;

                export_trace("trace: starting build");
                if (!ps1_build_start(&config)) {
                    export_trace("trace: build start failed");
                    log_push("[exporter] FAILED: could not prepare the build.");
                    g_state = EXPORT_STATE_FAILED;
                    return;
                }
                g_build_started = 1;
            }

            if (!ps1_build_poll(&ok)) {
                g_build_started = 0;
                if (!ok) {
                    log_push("[exporter] FAILED: build failed - see output above for the exact error.");
                    g_state = EXPORT_STATE_FAILED;
                    return;
                }
                log_push("[exporter] build succeeded.");
                g_state = EXPORT_STATE_COPY_OUTPUT;
            }
            return;
        }

        case EXPORT_STATE_STRIP:
            g_state = EXPORT_STATE_COPY_OUTPUT;
            return;

        case EXPORT_STATE_COPY_OUTPUT: {
            char src_dir[1536];
            snprintf(src_dir, sizeof(src_dir), "%s/src", g_build_dir);

            if (!rebax_fs_mkdir_p(g_output_dir)) { g_state=EXPORT_STATE_FAILED; return; }

            char elf_src[1600], elf_dst[1600], convert_error[256];
            snprintf(elf_src,sizeof(elf_src),"%s/%s.elf",src_dir,g_exe_name);
            snprintf(elf_dst,sizeof(elf_dst),"%s/%s.exe",g_output_dir,g_exe_name);
            log_push("[ps1] converting ELF to PS-EXE");
            if (!ps1_exe_convert(elf_src, elf_dst, convert_error, sizeof(convert_error))) {
                log_pushf("[exporter] FAILED: %s (%s)", convert_error, elf_src);
                g_state = EXPORT_STATE_FAILED;
                return;
            }
            log_pushf("[exporter] done - %s/%s.exe", g_output_dir, g_exe_name);
            g_state = EXPORT_STATE_SUCCESS;
            return;
        }
    }
}
