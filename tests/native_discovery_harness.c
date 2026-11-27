#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "exporters/common/native_discovery.h"
#include "exporters/common/export_internal.h"

export_state_t g_state;
export_shell_step_t g_step;
char g_exe_name[128], g_output_dir[1024], g_build_dir[1536], g_nodes_src_dir[1536];
int g_is_release, g_used_types_count, g_sdk_types_count, g_image_paths_count;
char g_used_types[EXPORT_MAX_USED_TYPES][64], g_sdk_types[EXPORT_MAX_USED_TYPES][64];
char g_image_paths[EXPORT_MAX_IMAGE_PATHS][1024];
export_image_spec_t g_image_specs[EXPORT_MAX_IMAGE_PATHS];
char g_extra_libs[EXPORT_MAX_EXTRA_FLAGS][160], g_extra_incs[EXPORT_MAX_EXTRA_FLAGS][160];
int g_extra_libs_count, g_extra_incs_count, g_needs_image_loader, g_needs_engine_context_stub;
int g_declared_interfaces_count;
char g_declared_interfaces[EXPORT_MAX_USED_TYPES][64];
FILE *g_codegen_main, *g_codegen_table;
export_rscene_node_t g_parsed_nodes[EXPORT_MAX_NODES_PER_SCENE];
int g_parsed_node_count;
void log_push(const char *text) { (void)text; }
void log_pushf(const char *format, ...) { (void)format; }
static void make_dir(const char *path) { if (mkdir(path, 0700) != 0) { perror(path); exit(2); } }
static void write_file(const char *path, const char *text) { FILE *f = fopen(path, "w"); if (!f) { perror(path); exit(2); } fputs(text, f); fclose(f); }
int main(void) {
    char root[] = "/tmp/rebax-discovery-XXXXXX";
    char sdk[256], section[256], include[256], source[256], header[256], manifest[256];
    if (!mkdtemp(root)) return 2;
    snprintf(sdk, sizeof(sdk), "%s/sdk", root); snprintf(section, sizeof(section), "%s/custom", sdk);
    snprintf(include, sizeof(include), "%s/include", section); snprintf(source, sizeof(source), "%s/source", root);
    snprintf(header, sizeof(header), "%s/include/demo.h", section); snprintf(manifest, sizeof(manifest), "%s/rebax-libs.manifest", section);
    make_dir(sdk); make_dir(section); make_dir(include); make_dir(source);
    write_file(header, "#define DEMO 1\n");
    write_file(manifest, "format=1\nsection=custom\ninclude=include\nheader=demo.h|demo\nlibrary=libdemo.a\n");
    snprintf(source, sizeof(source), "%s/source/main.c", root);
    write_file(source, "#include <demo.h>\nint main(void) { return DEMO; }\n");
    {
        char source_root[256]; snprintf(source_root, sizeof(source_root), "%s/source", root);
        if (!export_native_discover(source_root, sdk) || g_extra_incs_count != 1 || g_extra_libs_count != 1 || strcmp(g_extra_libs[0], "-ldemo") != 0) return 3;
        {
            char sub[256], sdl[256], man[256];
            snprintf(sub, sizeof(sub), "%s/include/SDL2", section); make_dir(sub);
            snprintf(sdl, sizeof(sdl), "%s/include/SDL2/SDL.h", section); write_file(sdl, "");
            snprintf(man, sizeof(man), "%s/rebax-libs.manifest", section);
            write_file(man, "format=1\nsection=custom\nheader=demo.h|demo\nheader=SDL2/SDL.h|demo\n");
            write_file(source, "#include <SDL.h>\n#include <vector>\n#include <demo.h>\n");
            if (!export_native_discover(source_root, sdk) || g_extra_libs_count != 1) return 5;
        }
        write_file(source, "#include <missing_demo.h>\n");
        if (export_native_discover(source_root, sdk) || export_native_unresolved_count() != 1) return 4;
    }
    puts("native discovery harness: ok");
    return 0;
}
