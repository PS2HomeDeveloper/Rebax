/*
 *  ____  _____ ____    _    __  __
 * |  _ \| ____| __ )  / \   \ \/ /
 * | |_) |  _| |  _ \ / _ \   \  / 
 * |  _ <| |___| |_) / ___ \  /  \ 
 * |_| \_\_____|____/_/   \_\/_/\_\
 *
 * Rebax Engine
 * Copyright (c) 2026 PS2HomeDeveloper
 *
 * Licensed under the MIT License.
 * See the LICENSE file for details.
 */

/* Project asset conversion for native exports (PS2 target format: RGBA8 pixels). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdint.h>
#include "exporters/common/export_internal.h"
#include "exporters/common/node_source.h"
#include "formats/image_decoder.h"
#include "formats/tim/tim_codec.h"

static void export_slashes(char *path) {
    for (char *p = path; *p != '\0'; p++) if (*p == '\\') *p = '/';
}

static int export_write_image_files(const char *dest_src_dir, int index,
                                    const unsigned char *pixels, long bytes) {
    char data_path[1700];
    snprintf(data_path, sizeof(data_path), "%s/image_%d.rgba", dest_src_dir, index);

    FILE *df = fopen(data_path, "wb");
    if (df == NULL) return 0;
    int written = fwrite(pixels, 1, (size_t)bytes, df) == (size_t)bytes;
    fclose(df);
    if (!written) return 0;

    return 1;
}

static int export_write_ps1_tim(const char *dest_src_dir, int index,
                                const unsigned char *pixels, int width, int height) {
    char path[1700];
    snprintf(path, sizeof(path), "%s/image_%d.tim", dest_src_dir, index);
    return rebax_tim_write_ps1_16(path, pixels, width, height);
}

int export_assets_convert_images(const char *dest_src_dir) {
    int portable_data = node_source_is_modern();
    int ps1_data = node_source_is_ps1();
    char header_path[1700];
    snprintf(header_path, sizeof(header_path), "%s/embedded_images.h", dest_src_dir);
    FILE *hf = fopen(header_path, "w");
    if (hf == NULL) return 0;
    fputs("#ifndef EMBEDDED_IMAGES_H\n#define EMBEDDED_IMAGES_H\n\n#include <stddef.h>\n\n"
          "typedef struct {\n"
          "    const char *path;\n"
          "    int width;\n"
          "    int height;\n"
          "    const unsigned char *data;\n"
          "    size_t data_size;\n"
          "} embedded_image_t;\n\n"
          "const embedded_image_t *embedded_image_find(const char *path);\n\n"
          "#endif\n", hf);
    fclose(hf);

    char source_path[1700];
    snprintf(source_path, sizeof(source_path), "%s/embedded_images.c", dest_src_dir);
    FILE *cf = fopen(source_path, "w");
    if (cf == NULL) return 0;
    fputs("#include <string.h>\n#include <stddef.h>\n#include \"embedded_images.h\"\n\n", cf);

    int ok_count = 0;
    int widths[EXPORT_MAX_IMAGE_PATHS] = {0}, heights[EXPORT_MAX_IMAGE_PATHS] = {0};
    int ready[EXPORT_MAX_IMAGE_PATHS] = {0};
    long sizes[EXPORT_MAX_IMAGE_PATHS] = {0};
    for (int i = 0; i < g_image_paths_count; i++) {
        const export_image_spec_t *spec = &g_image_specs[i];
        int w = 0, h = 0;
        unsigned char *pixels = image_loader_editor_load(g_image_paths[i], &w, &h,
                                                         spec->raw_width, spec->raw_height,
                                                         spec->raw_format);
        if (pixels == NULL) {
            log_pushf("[exporter] WARNING: could not read/decode image: %s", g_image_paths[i]);
            continue;
        }
        if (w <= 0 || h <= 0 || (long)w > LONG_MAX / 4 / (long)h) {
            free(pixels);
            continue;
        }
        long pixel_bytes = (long)w * (long)h * 4;
        int written = portable_data || (ps1_data ? export_write_ps1_tim(dest_src_dir, i, pixels, w, h) : export_write_image_files(dest_src_dir, i, pixels, pixel_bytes));
        if (!written) {
            free(pixels);
            log_pushf("[exporter] WARNING: could not write embedded data for: %s", g_image_paths[i]);
            continue;
        }
        widths[i] = w;
        heights[i] = h;
        ready[i] = 1;
        ok_count++;
        sizes[i] = pixel_bytes;
        if (portable_data) {
            fprintf(cf, "static const unsigned char rebax_image_%d[] = {\n", i);
            for (long b = 0; b < pixel_bytes; b++) {
                if (b % 16 == 0) fputs("    ", cf);
                fprintf(cf, "0x%02x,", pixels[b]);
                if (b % 16 == 15 || b + 1 == pixel_bytes) fputc('\n', cf);
            }
            fputs("};\n", cf);
        } else {
            char data_path[1700];
            snprintf(data_path, sizeof(data_path), "%s/image_%d.%s", dest_src_dir, i, ps1_data ? "tim" : "rgba");
            if (ps1_data) {
                FILE *size_file = fopen(data_path, "rb");
                if (size_file != NULL) {
                    if (fseek(size_file, 0, SEEK_END) == 0) {
                        long length = ftell(size_file);
                        if (length > 0) sizes[i] = length;
                    }
                    fclose(size_file);
                }
            }
            export_slashes(data_path);
            fprintf(cf, "extern const unsigned char rebax_image_%d[];\n"
                        "extern const unsigned char rebax_image_%d_end[];\n"
                        "__asm__(\".section .rodata\\n\"\n"
                        "        \".balign 64\\n\"\n"
                        "        \".globl rebax_image_%d\\n\"\n"
                        "        \"rebax_image_%d:\\n\"\n"
                        "        \".incbin \\\"%s\\\"\\n\"\n"
                        "        \".globl rebax_image_%d_end\\n\"\n"
                        "        \"rebax_image_%d_end:\\n\"\n"
                        "        \".previous\\n\");\n",
                    i, i, i, i, data_path, i, i);
        }
        free(pixels);
        log_pushf("[exporter] embedded image: %s (%dx%d)", g_image_paths[i], w, h);
    }

    fputs("\nstatic const embedded_image_t g_embedded_images[] = {\n", cf);
    for (int i = 0; i < g_image_paths_count; i++) {
        if (!ready[i]) continue;
        fprintf(cf, "    { \"%s\", %d, %d, rebax_image_%d, %ld },\n", export_public_path(g_image_paths[i]), widths[i], heights[i], i, sizes[i]);
    }
    fputs("    { NULL, 0, 0, NULL, 0 }\n};\n\n", cf);

    fputs("const embedded_image_t *embedded_image_find(const char *path) {\n"
          "    for (size_t i = 0; g_embedded_images[i].path != NULL; i++) {\n"
          "        if (strcmp(g_embedded_images[i].path, path) == 0) return &g_embedded_images[i];\n"
          "    }\n"
          "    const char *name = path;\n"
          "    for (const char *p = path; *p != '\\0'; p++) {\n"
          "        if (*p == '/' || *p == '\\\\') name = p + 1;\n"
          "    }\n"
          "    for (size_t i = 0; g_embedded_images[i].path != NULL; i++) {\n"
          "        const char *entry = g_embedded_images[i].path;\n"
          "        const char *entry_name = entry;\n"
          "        for (const char *p = entry; *p != '\\0'; p++) {\n"
          "            if (*p == '/' || *p == '\\\\') entry_name = p + 1;\n"
          "        }\n"
          "        if (name[0] != '\\0' && strcmp(entry_name, name) == 0) return &g_embedded_images[i];\n"
          "    }\n"
          "    return NULL;\n"
          "}\n", cf);
    fclose(cf);
    return ok_count == g_image_paths_count;
}
