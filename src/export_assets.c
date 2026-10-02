/* Project asset conversion for native PS2 exports. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "export_internal.h"
#include "stb_image.h"

static int export_path_has_ext(const char *path, const char *ext) {
    size_t path_len = strlen(path), ext_len = strlen(ext);
    return path_len >= ext_len && strcasecmp(path + path_len - ext_len, ext) == 0;
}

static void export_unpack_5551(unsigned short value, unsigned char *rgba) {
    unsigned int r = value & 0x1F;
    unsigned int g = (value >> 5) & 0x1F;
    unsigned int b = (value >> 10) & 0x1F;
    rgba[0] = (unsigned char)((r << 3) | (r >> 2));
    rgba[1] = (unsigned char)((g << 3) | (g >> 2));
    rgba[2] = (unsigned char)((b << 3) | (b >> 2));
    rgba[3] = (value & 0x8000) ? 255 : 0;
}

/* RAW files have no header, so their dimensions and GS pixel format come
 * from Sprite2D properties. The supported formats match the editor loader:
 * GS_PSM_32 (0), GS_PSM_24 (1), GS_PSM_16 (2), and GS_PSM_16S (0x0A). */
static unsigned char *export_load_raw(const char *path, int width, int height,
                                      int psm) {
    if (width <= 0 || height <= 0) return NULL;
    FILE *f = fopen(path, "rb");
    if (f == NULL) return NULL;
    size_t pixels = (size_t)width * (size_t)height;
    size_t bytes_per_pixel = psm == 0 ? 4 : (psm == 1 ? 3 : 2);
    if (psm != 0 && psm != 1 && psm != 2 && psm != 0x0A) {
        fclose(f);
        return NULL;
    }
    unsigned char *raw = malloc(pixels * bytes_per_pixel);
    unsigned char *rgba = malloc(pixels * 4);
    if (raw == NULL || rgba == NULL ||
        fread(raw, 1, pixels * bytes_per_pixel, f) != pixels * bytes_per_pixel) {
        free(raw);
        free(rgba);
        fclose(f);
        return NULL;
    }
    fclose(f);
    for (size_t i = 0; i < pixels; i++) {
        if (psm == 0) {
            memcpy(rgba + i * 4, raw + i * 4, 4);
        } else if (psm == 1) {
            memcpy(rgba + i * 4, raw + i * 3, 3);
            rgba[i * 4 + 3] = 255;
        } else {
            unsigned short value = (unsigned short)(raw[i * 2] | (raw[i * 2 + 1] << 8));
            export_unpack_5551(value, rgba + i * 4);
        }
    }
    free(raw);
    return rgba;
}

static unsigned char *export_load_image(int index, int *out_width, int *out_height) {
    if (export_path_has_ext(g_image_paths[index], ".raw")) {
        const export_image_spec_t *spec = &g_image_specs[index];
        *out_width = spec->raw_width;
        *out_height = spec->raw_height;
        return export_load_raw(g_image_paths[index], spec->raw_width,
                               spec->raw_height, spec->raw_format);
    }
    int channels;
    return stbi_load(g_image_paths[index], out_width, out_height, &channels, 4);
}

/* Converts each referenced project image exactly once into RGBA8 data,
 * matching the format expected by the native gsKit texture upload path.
 * Generic images use stb_image on the host; headerless RAW images use the
 * dimensions and pixel format exported from Sprite2D properties. */
int export_assets_convert_images(const char *dest_src_dir) {
    char header_path[1700];
    snprintf(header_path, sizeof(header_path), "%s/embedded_images.h", dest_src_dir);
    FILE *hf = fopen(header_path, "w");
    if (hf == NULL) return 0;
    fputs("/* مولَّد تلقائياً وقت التصدير من صور المشروع - لا تعدّله يدوياً */\n"
          "#ifndef EMBEDDED_IMAGES_H\n#define EMBEDDED_IMAGES_H\n\n"
          "typedef struct {\n"
          "    const char *path;\n"
          "    int width;\n"
          "    int height;\n"
          "    const unsigned char *data; /* RGBA8 خام، width*height*4 بايت */\n"
          "} embedded_image_t;\n\n"
          "const embedded_image_t *embedded_image_find(const char *path);\n\n"
          "#endif\n", hf);
    fclose(hf);

    char source_path[1700];
    snprintf(source_path, sizeof(source_path), "%s/embedded_images.c", dest_src_dir);
    FILE *cf = fopen(source_path, "w");
    if (cf == NULL) return 0;
    fputs("/* مولَّد تلقائياً وقت التصدير من صور المشروع - لا تعدّله يدوياً */\n"
          "#include <string.h>\n#include <stddef.h>\n"
          "#include \"embedded_images.h\"\n\n", cf);

    int ok_count = 0;
    for (int i = 0; i < g_image_paths_count; i++) {
        int w = 0, h = 0;
        unsigned char *pixels = export_load_image(i, &w, &h);
        if (pixels == NULL) {
            log_pushf("[exporter] WARNING: could not read/decode image: %s", g_image_paths[i]);
            continue;
        }
        fprintf(cf, "static const unsigned char image_%d_data[] = {\n", i);
        long total = (long)w * h * 4;
        for (long b = 0; b < total; b++) {
            fprintf(cf, "%u,", pixels[b]);
            if ((b % 20) == 19) fputc('\n', cf);
        }
        fputs("\n};\n\n", cf);
        free(pixels);
        ok_count++;
        log_pushf("[exporter] converted image: %s (%dx%d)", g_image_paths[i], w, h);
    }

    fprintf(cf, "static const embedded_image_t g_embedded_images[] = {\n");
    for (int i = 0; i < g_image_paths_count; i++) {
        int w = 0, h = 0;
        unsigned char *pixels = export_load_image(i, &w, &h);
        if (pixels == NULL) continue;
        fprintf(cf, "    { \"%s\", %d, %d, image_%d_data },\n",
                g_image_paths[i], w, h, i);
        free(pixels);
    }
    fprintf(cf, "};\n\n");

    fputs("const embedded_image_t *embedded_image_find(const char *path) {\n"
          "    size_t count = sizeof(g_embedded_images) / sizeof(g_embedded_images[0]);\n"
          "    for (size_t i = 0; i < count; i++) {\n"
          "        if (strcmp(g_embedded_images[i].path, path) == 0) return &g_embedded_images[i];\n"
          "    }\n"
          "    return NULL;\n"
          "}\n", cf);
    fclose(cf);
    return (ok_count == g_image_paths_count);
}
