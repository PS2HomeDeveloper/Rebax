/* Node selection and dependency discovery for native exports. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include "export_internal.h"
#include "rebax_fs.h"
#include "node_source.h"
#pragma GCC diagnostic ignored "-Wformat-truncation"
#define MAX_EXTRA_FLAGS EXPORT_MAX_EXTRA_FLAGS

/* ------------------------------------------------------------
 * Scans g_nodes_src_dir for filenames matching the node types used
 * (the "@NODE ... name=<X>" line in each .c file) - and copies
 * the file (+ its .h if present) to the export build folder. Returns 1 if
 * every used type matched an actual file, 0 if at least one type
 * had no file (a real error - a project node without an export source) */

/* Additional libraries/include paths (non-core) that the copied node files
 * declare themselves via @PS2_EXPORT_LIBS/@PS2_EXPORT_INCLUDES comments
 * anywhere in their file - with no knowledge of the library or node name in the
 * exporter code itself. This means: a future node that needs a new ps2sdk
 * library (audio, network...) declares its requirement in its own file - the
 * exporter needs no changes at all */

static void add_unique_flag(char list[][160], int *count, const char *value) {
    for (int i = 0; i < *count; i++) {
        if (strcmp(list[i], value) == 0) return;
    }
    if (*count >= MAX_EXTRA_FLAGS) return;
    strncpy(list[*count], value, 159);
    list[*count][159] = '\0';
    (*count)++;
}

/* Reads a line of the form "... @PS2_EXPORT_LIBS: <flags> ..." (a regular C
 * comment; everything after ":" is taken literally up to the first "*" (for
 * block comments) or the end of line) and adds it to the corresponding array -
 * making no assumptions about any particular node or library */
static void scan_line_for_marker(const char *line, const char *marker,
                                  char list[][160], int *count) {
    const char *p = strstr(line, marker);
    if (p == NULL) return;
    p += strlen(marker);
    while (*p == ' ' || *p == '\t') p++;

    char value[160];
    strncpy(value, p, sizeof(value) - 1);
    value[sizeof(value) - 1] = '\0';

    char *end_comment = strstr(value, "*/");
    if (end_comment != NULL) *end_comment = '\0';

    size_t vlen = strlen(value);
    while (vlen > 0 && (value[vlen - 1] == ' ' || value[vlen - 1] == '\t'
                         || value[vlen - 1] == '\n' || value[vlen - 1] == '\r')) {
        value[--vlen] = '\0';
    }
    if (vlen > 0) add_unique_flag(list, count, value);
}

/* Scans a single source file (i.e. a file we copied for export) for
 * @PS2_EXPORT_LIBS/@PS2_EXPORT_INCLUDES comments and records them. Called for every
 * .c/.h file actually copied - no exceptions, no prior knowledge of its contents */
static void scan_content_for_export_flags(char *content) {

    char *line = strtok(content, "\n");
    while (line != NULL) {
        scan_line_for_marker(line, "@PS2_EXPORT_LIBS:", g_extra_libs, &g_extra_libs_count);
        scan_line_for_marker(line, "@PS2_EXPORT_INCLUDES:", g_extra_incs, &g_extra_incs_count);
        if (strstr(line, "#include \"image_loader.h\"") != NULL) {
            g_needs_image_loader = 1;
        }
        if (strstr(line, "#include \"engine_context.h\"") != NULL) {
            g_needs_engine_context_stub = 1;
        }
        line = strtok(NULL, "\n");
    }
    free(content);
}

void export_scan_file_for_export_flags(const char *path) {
    long size = 0;
    char *content = export_read_whole_file(path, &size);
    if (content == NULL) return;
    scan_content_for_export_flags(content);
}

static void scan_node_source_for_export_flags(const char *name) {
    long size = 0;
    char *content = node_source_read(name, &size);
    if (content == NULL) return;
    scan_content_for_export_flags(content);
}

typedef struct {
    char (*names)[96];
    int count;
    int capacity;
} node_name_list_t;

static void collect_node_name(const char *name, void *user) {
    node_name_list_t *list = (node_name_list_t *)user;
    size_t len = strlen(name);
    if (len < 3 || strcmp(name + len - 2, ".c") != 0 || len >= 96) return;
    if (list->count == list->capacity) {
        list->capacity = list->capacity ? list->capacity * 2 : 32;
        list->names = realloc(list->names, (size_t)list->capacity * sizeof(list->names[0]));
        if (list->names == NULL) { list->count = 0; list->capacity = 0; return; }
    }
    strcpy(list->names[list->count++], name);
}

int export_nodes_copy_matched(const char *dest_src_dir) {
    node_name_list_t node_names = {0};
    if (!node_source_list(collect_node_name, &node_names)) {
        log_pushf("[exporter] failed to open node resources: %s", g_nodes_src_dir);
        return 0;
    }

    int matched_count = 0;
    for (int n = 0; n < node_names.count; n++) {
        const char *entry_name = node_names.names[n];

        long size = 0;
        char *content = node_source_read(entry_name, &size);
        if (content == NULL) continue;

        char *at_node = strstr(content, "@NODE");
        if (at_node == NULL) { free(content); continue; }

        char *name_kv = strstr(at_node, "name=");
        if (name_kv == NULL) { free(content); continue; }
        name_kv += 5;

        char type_name[64];
        int i = 0;
        while (name_kv[i] != '\0' && name_kv[i] != ' ' && name_kv[i] != '\n'
               && name_kv[i] != '*' && i < 63) {
            type_name[i] = name_kv[i];
            i++;
        }
        type_name[i] = '\0';
        free(content);

        int sdk_known = 0;
        for (int t = 0; t < g_sdk_types_count; t++) {
            if (strcmp(g_sdk_types[t], type_name) == 0) { sdk_known = 1; break; }
        }
        if (!sdk_known && g_sdk_types_count < EXPORT_MAX_USED_TYPES) {
            strncpy(g_sdk_types[g_sdk_types_count], type_name,
                    sizeof(g_sdk_types[g_sdk_types_count]) - 1);
            g_sdk_types[g_sdk_types_count][sizeof(g_sdk_types[0]) - 1] = '\0';
            g_sdk_types_count++;
        }

        int is_used = 0;
        for (int t = 0; t < g_used_types_count; t++) {
            if (strcmp(g_used_types[t], type_name) == 0) { is_used = 1; break; }
        }
        /* The SDK exposes the complete current node catalog, so its native
         * implementations are copied once even when a scene does not use
         * them. Scene-only exports still validate against g_used_types. */
        if (!is_used && !sdk_known && g_sdk_types_count == 0) continue;

        /* Copy the .c file itself plus the matching .h if present (no failure if absent -
         * some nodes lack a dedicated header and use the shared node_interface.h) */
        char base_name[64];
        strncpy(base_name, entry_name, sizeof(base_name) - 1);
        base_name[sizeof(base_name) - 1] = '\0';
        char *ext_dot = strrchr(base_name, '.');
        if (ext_dot) *ext_dot = '\0';

        char name_c[100], name_h[100], dst_c[1600], dst_h[1600];
        snprintf(name_c, sizeof(name_c), "%s.c", base_name);
        snprintf(name_h, sizeof(name_h), "%s.h", base_name);
        snprintf(dst_c, sizeof(dst_c), "%s/%s.c", dest_src_dir, base_name);
        snprintf(dst_h, sizeof(dst_h), "%s/%s.h", dest_src_dir, base_name);

        if (!node_source_copy(name_c, dst_c)) { free(node_names.names); return 0; }
        node_source_copy(name_h, dst_h);

        scan_node_source_for_export_flags(name_c);
        scan_node_source_for_export_flags(name_h); /* No harm if the .h is missing - read_whole_file quietly returns NULL */

        log_pushf("[exporter] included node type: %s (%s.c)", type_name, base_name);
        if (is_used) matched_count++;
    }
    free(node_names.names);

    /* node_interface.h is always shared - all node files include it */
    { char dst[1600]; snprintf(dst,sizeof(dst),"%s/node_interface.h",dest_src_dir); if(!node_source_copy("node_interface.h",dst)) return 0; }
    scan_node_source_for_export_flags("node_interface.h");

    {
        char sdk_dst[1600];
        snprintf(sdk_dst, sizeof(sdk_dst), "%s/rebax_sdk.h", dest_src_dir);
        if (!node_source_copy("rebax_sdk.h", sdk_dst)) return 0;
    }

    /* engine_context.h - completely independent of g_needs_image_loader (it was
     * mistakenly tied to it before via IMAGE_LOADER_FILES below, from an older design
     * where Sprite2D went through image_loader.h - when that was removed, this copy
     * became silently dead even though g_needs_engine_context_stub still activates
     * correctly and independently. A separate condition here prevents repeating this
     * mistake with any future dependency.) */
    if (g_needs_engine_context_stub) {
        { char dst[1600]; snprintf(dst,sizeof(dst),"%s/engine_context.h",dest_src_dir); node_source_copy("engine_context.h",dst); }

        /* Actual verification (no assumptions) - clearly prints whether the copy succeeded,
         * so we don't have to guess again if a similar error occurs later */
        char ectx_dst[1600];
        struct stat st;
        snprintf(ectx_dst, sizeof(ectx_dst), "%s/engine_context.h", dest_src_dir);
        if (stat(ectx_dst, &st) == 0) {
            log_pushf("[exporter] verified: engine_context.h present at %s (%ld bytes)",
                      ectx_dst, (long)st.st_size);
        } else {
            log_pushf("[exporter] WARNING: engine_context.h missing after copy attempt "
                      "(source: %s)", g_nodes_src_dir);
        }
    }

    /* Any matching node that actually included "image_loader.h" (detected
     * during the scan above - with no special knowledge of the Sprite2D name here) -
     * copy the entire image loader unit: the shared parts + the public wrapper
     * function image_loader_load itself (called directly by sprite2d.c) + *all*
     * seven formats - because the public function references all seven with its code
     * (see the design comment in image_loader.h: the optimization "call the specific
     * format directly instead of the public function" is a separate future step, not
     * yet applied - this initial structure accepts temporary code bloat instead of
     * breaking linkage). Does not include engine_context.h - that was copied above
     * independently */
    if (g_needs_image_loader) {
        static const char *IMAGE_LOADER_FILES[] = {
            "image_loader.h", "image_loader_internal.h",
            "image_loader.c", "image_loader_common.c",
            "image_loader_png.c", "image_loader_jpeg.c", "image_loader_bmp.c",
            "image_loader_tga.c", "image_loader_tiff.c", "image_loader_raw.c",
            "image_loader_tim2.c", "image_loader_tim.c",
        };
        for (size_t f = 0; f < sizeof(IMAGE_LOADER_FILES) / sizeof(IMAGE_LOADER_FILES[0]); f++) {
            { char dst[1600]; snprintf(dst,sizeof(dst),"%s/%s",dest_src_dir,IMAGE_LOADER_FILES[f]); if(!node_source_copy(IMAGE_LOADER_FILES[f],dst)) return 0; }
            scan_node_source_for_export_flags(IMAGE_LOADER_FILES[f]);
        }
        log_push("[exporter] included full image loader subsystem (all formats - "
                 "per-format dead-code elimination is a separate future step).");
    }

    return matched_count == g_used_types_count;
}
