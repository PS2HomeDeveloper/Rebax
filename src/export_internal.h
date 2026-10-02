/* Internal exporter contract. Keep this header private to exporter modules. */
#ifndef REBAX_EXPORT_INTERNAL_H
#define REBAX_EXPORT_INTERNAL_H

#include <stdio.h>
#include <stddef.h>
#include <sys/types.h>
#include "node_registry.h"

enum { EXPORT_MAX_USED_TYPES = 64, EXPORT_MAX_IMAGE_PATHS = 64,
       EXPORT_MAX_PROPS_PER_NODE = 32, EXPORT_MAX_NODES_PER_SCENE = 128,
       EXPORT_MAX_EXTRA_FLAGS = 32, EXPORT_LOG_LINE_MAX = 200,
       EXPORT_LOG_QUEUE_SIZE = 1024 };

typedef struct {
    FILE *pipe;
    int fd;
    int active;
    char partial[EXPORT_LOG_LINE_MAX];
    int partial_len;
} export_shell_step_t;

typedef struct {
    char key[64];
    char value[160];
} export_rscene_kv_t;

typedef struct {
    char type[64];
    char name[128];
    export_rscene_kv_t props[EXPORT_MAX_PROPS_PER_NODE];
    int prop_count;
} export_rscene_node_t;

typedef struct {
    char path[1024];
    int raw_width;
    int raw_height;
    int raw_format;
} export_image_spec_t;

typedef enum {
    EXPORT_STATE_IDLE,
    EXPORT_STATE_PREPARE_BUILD,
    EXPORT_STATE_BUILD,
    EXPORT_STATE_STRIP,
    EXPORT_STATE_COPY_OUTPUT,
    EXPORT_STATE_SUCCESS,
    EXPORT_STATE_FAILED
} export_state_t;

extern export_state_t g_state;
extern export_shell_step_t g_step;
extern char g_exe_name[128];
extern int g_is_release;
extern char g_output_dir[1024];
extern char g_ps2dev_root[1536];
extern char g_build_dir[1536];
extern char g_nodes_src_dir[1536];
extern char g_used_types[EXPORT_MAX_USED_TYPES][64];
extern int g_used_types_count;
extern char g_sdk_types[EXPORT_MAX_USED_TYPES][64];
extern int g_sdk_types_count;
extern char g_image_paths[EXPORT_MAX_IMAGE_PATHS][1024];
extern int g_image_paths_count;
extern export_image_spec_t g_image_specs[EXPORT_MAX_IMAGE_PATHS];
extern char g_extra_libs[EXPORT_MAX_EXTRA_FLAGS][160];
extern int g_extra_libs_count;
extern char g_extra_incs[EXPORT_MAX_EXTRA_FLAGS][160];
extern int g_extra_incs_count;
extern int g_needs_image_loader;
extern int g_needs_engine_context_stub;
extern char g_declared_interfaces[EXPORT_MAX_USED_TYPES][64];
extern int g_declared_interfaces_count;
extern FILE *g_codegen_main;
extern FILE *g_codegen_table;
extern export_rscene_node_t g_parsed_nodes[EXPORT_MAX_NODES_PER_SCENE];
extern int g_parsed_node_count;

void log_reset(void);
void log_push(const char *text);
void log_pushf(const char *fmt, ...);
int shell_step_start(export_shell_step_t *step, const char *command);
int shell_step_poll(export_shell_step_t *step, int *out_ok);
void shell_step_cancel(export_shell_step_t *step);

void export_scene_reset(void);
void export_scene_walk_rscene_files(const char *dir_path, void (*on_file)(const char *path));
void export_scene_generate_from_file(const char *path);
void export_scene_add_image_path(const char *value);
void export_scene_add_image_path_with_raw(const char *value, int raw_width,
                                          int raw_height, int raw_format);

int export_nodes_copy_matched(const char *dest_src_dir);
void export_scan_file_for_export_flags(const char *path);
int export_project_copy_sources(const char *project_root, const char *dest_src_dir);
int export_assets_convert_images(const char *dest_src_dir);
void export_codegen_write_runtime_files(const char *src_dir);
void export_codegen_write_engine_context_impl(const char *src_dir);
int export_codegen_write_sdk_files(const char *src_dir);
void export_makefile_write(const char *build_dir, const char *src_dir_name);

char *export_read_whole_file(const char *path, long *out_size);

#endif
