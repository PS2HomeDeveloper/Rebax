/*
 * Rebax PS2 Exporter
 *
 * The exporter converts the project's .rscene files into native PS2 C code
 * during the export process.
 *
 * The PS2 executable does NOT receive or parse the original .rscene files.
 * Rebax reads the scenes on the host side, resolves the used node types,
 * converts their properties and assets into the appropriate PS2-side data
 * and code, and generates the temporary source files required to build
 * the final ELF.
 *
 * Export flow:
 *
 *   Project .rscene files
 *          |
 *          v
 *   Rebax exporter
 *          |
 *          +--> Resolve used node types
 *          |
 *          +--> Read scene properties
 *          |
 *          +--> Convert project assets
 *          |
 *          +--> Generate project-specific C source
 *          |
 *          v
 *   Temporary PS2 source tree
 *          |
 *          v
 *   PS2Dev toolchain
 *          |
 *          v
 *   Final PS2 ELF
 *
 * The goal is to make the generated PS2 program as close as possible to
 * a manually written PS2 program. Engine/editor concepts that are only
 * required during development should be resolved by the exporter whenever
 * possible instead of being carried into the final PS2 executable.
 *
 * The exporter is responsible for project-specific generation.
 * Generic PS2 runtime code and node implementations remain in their
 * respective source files and are copied/assembled according to the
 * nodes actually used by the project.
 *
 * The temporary export directory is:
 *
 *   Rebax/Temp/export/
 *
 * It contains only the files required for the current export operation
 * and is cleaned after the export is completed.
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
#define REBAX_SETENV(name, value) _putenv_s(name, value)
#else
#define REBAX_SETENV(name, value) setenv(name, value, 1)
#endif

#include <stdarg.h>

#include "ps2_exporter.h"
#include "current_project.h"
#include "rebax_paths.h"
#include "rebax_fs.h"

/* -Wformat-truncation: all warnings here are purely theoretical (GCC assumes the worst
 * case for the length of any source char[] regardless of its actual runtime content)
 * - all paths here are in practice much shorter than the buffer sizes (from
 * readlink/current_project_get_path, not from an unbounded user input),
 * and any theoretical truncation here will show as a clear build failure in the output log, not a silent
 * dangerous bug - disabling this warning here is clearer than inflating dozens of arrays
 * unnecessarily */
#pragma GCC diagnostic ignored "-Wformat-truncation"

/* Export orchestration state. Implementation details live in export_* modules. */
#include "export_internal.h"

export_state_t g_state = EXPORT_STATE_IDLE;
export_shell_step_t g_step;
char g_exe_name[128];
int g_is_release;
char g_output_dir[1024];
char g_ps2dev_root[1536];
char g_build_dir[1536];
char g_nodes_src_dir[1536];
char g_used_types[EXPORT_MAX_USED_TYPES][64];
int g_used_types_count;
char g_sdk_types[EXPORT_MAX_USED_TYPES][64];
int g_sdk_types_count;
char g_image_paths[EXPORT_MAX_IMAGE_PATHS][1024];
int g_image_paths_count;
export_image_spec_t g_image_specs[EXPORT_MAX_IMAGE_PATHS];
char g_extra_libs[EXPORT_MAX_EXTRA_FLAGS][160];
int g_extra_libs_count;
char g_extra_incs[EXPORT_MAX_EXTRA_FLAGS][160];
int g_extra_incs_count;
int g_needs_image_loader;
int g_needs_engine_context_stub;

int ps2_export_start(const char *exe_name, int is_release, const char *output_dir) {
    if (g_state != EXPORT_STATE_IDLE && g_state != EXPORT_STATE_SUCCESS && g_state != EXPORT_STATE_FAILED) {
        return 0; /* A working export already exists */
    }
    if (!current_project_is_open()) {
        return 0;
    }

    log_reset();
    export_trace_begin();
    export_trace("trace: export start");

    strncpy(g_exe_name, exe_name, sizeof(g_exe_name) - 1);
    g_exe_name[sizeof(g_exe_name) - 1] = '\0';
    g_is_release = is_release;
    strncpy(g_output_dir, output_dir, sizeof(g_output_dir) - 1);
    g_output_dir[sizeof(g_output_dir) - 1] = '\0';

    /* The ps2dev environment and node sources are prepared in advance - extracted once
     * at the engine's first run (see rebax_paths.h) - this file no longer extracts
     * anything itself, it only reads from their fixed paths */
    snprintf(g_ps2dev_root, sizeof(g_ps2dev_root), "%s/ps2dev", rebax_toolchain_dir());
    strncpy(g_nodes_src_dir, rebax_node_resources_dir(), sizeof(g_nodes_src_dir) - 1);
    g_nodes_src_dir[sizeof(g_nodes_src_dir) - 1] = '\0';

    /* Export workspace - a fixed folder in the rebax tree (Temp/export), not
     * next to the executable nor inside the project folder - ensures reliable
     * write permissions across environments (see rebax_paths.h for the rationale) */
    strncpy(g_build_dir, rebax_temp_export_dir(), sizeof(g_build_dir) - 1);
    g_build_dir[sizeof(g_build_dir) - 1] = '\0';

    /* Fully clean the workspace before anything - without this, files from a previous
     * export attempt (e.g. image_loader_*.c from an older design) remain and are
     * picked up automatically by write_project_makefile (any .c found is added to
     * EE_OBJS), linking something that no longer exists with the current node sources.
     * Every export now starts from a completely empty folder */
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

void ps2_export_cancel(void) {
    shell_step_cancel(&g_step);
    if (g_state != EXPORT_STATE_IDLE) {
        log_push("[exporter] export cancelled by user.");
    }
    g_state = EXPORT_STATE_IDLE;
}

ps2_export_status_t ps2_export_get_status(void) {
    switch (g_state) {
        case EXPORT_STATE_IDLE:    return PS2_EXPORT_STATUS_IDLE;
        case EXPORT_STATE_SUCCESS: return PS2_EXPORT_STATUS_SUCCESS;
        case EXPORT_STATE_FAILED:  return PS2_EXPORT_STATUS_FAILED;
        default:            return PS2_EXPORT_STATUS_RUNNING;
    }
}

void ps2_export_update(void) {
    char cmd[3200];
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

            /* Generated scene_data.c - the "original" part (extern decls +
             * value/node arrays) is written directly while we parse each scene,
             * the g_scenes[] table is constructed in a separate temporary file
             * (it needs to reference all scene arrays that aren't complete yet when
             * the first file is written) */
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

            /* Append the g_scenes[] table to the end of scene_data.c - now all
             * node arrays referenced by it are written and present above */
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

            export_trace("trace: writing makefile");
            export_makefile_write(src_dir, ".");
            export_trace("trace: makefile written");

            log_push("[exporter] build directory ready - starting compilation...");
            g_state = EXPORT_STATE_BUILD;
            return;
        }

        case EXPORT_STATE_BUILD: {
            if (!g_step.active) {
                char src_dir[1536];
                snprintf(src_dir, sizeof(src_dir), "%s/src", g_build_dir);

                /* One shell command that builds everything: export PS2SDK environment variables
                 * (must be in the same single shell invocation - see ps2_exporter.h comment),
                 * prepend the mips64r5900el-ps2-elf-* tools to PATH, then make.
                 * scene_data.c is now plain C code (regular data, no raw objcopy needed -
                 * see the design comment at the top of the file) */
                REBAX_SETENV("PS2DEV",g_ps2dev_root);
                char sdk[1600],gskit[1600],pathv[3600];
                snprintf(sdk,sizeof(sdk),"%s/ps2sdk",g_ps2dev_root);
                snprintf(gskit,sizeof(gskit),"%s/gsKit",g_ps2dev_root);
                REBAX_SETENV("PS2SDK",sdk); REBAX_SETENV("GSKIT",gskit);
                snprintf(pathv,sizeof(pathv),"%s/bin:%s/ee/bin:%s/iop/bin:%s/bin:%s",g_ps2dev_root,g_ps2dev_root,g_ps2dev_root,sdk,getenv("PATH")?getenv("PATH"):"");
                REBAX_SETENV("PATH",pathv);
                snprintf(cmd,sizeof(cmd),"cd '%s' && '%s' 2>&1",src_dir,rebax_make_path());
                export_trace("trace: starting build command:");
                export_trace(cmd);

                if (!shell_step_start(&g_step, cmd)) {
                    export_trace("trace: popen failed");
                    log_push("[exporter] FAILED: could not start build process.");
                    g_state = EXPORT_STATE_FAILED;
                    return;
                }
            }

            if (!shell_step_poll(&g_step, &ok)) {
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
            /* Symbol stripping in the Release build is actually included in the generated
             * Makefile "all:" rule (EE_STRIP) - this case is not used currently,
             * reserved if we need a separate strip step later */
            g_state = EXPORT_STATE_COPY_OUTPUT;
            return;

        case EXPORT_STATE_COPY_OUTPUT: {
            char src_dir[1536];
            snprintf(src_dir, sizeof(src_dir), "%s/src", g_build_dir);

            if (!rebax_fs_mkdir_p(g_output_dir)) { g_state=EXPORT_STATE_FAILED; return; }

            char elf_src[1600], elf_dst[1600];
            snprintf(elf_src,sizeof(elf_src),"%s/%s.elf",src_dir,g_exe_name);
            snprintf(elf_dst,sizeof(elf_dst),"%s/%s.elf",g_output_dir,g_exe_name);
            if (!rebax_fs_copy_file(elf_src,elf_dst)) { g_state=EXPORT_STATE_FAILED; return; }

            log_pushf("[exporter] done - %s/%s.elf", g_output_dir, g_exe_name);
            g_state = EXPORT_STATE_SUCCESS;
            return;
        }
    }
}
