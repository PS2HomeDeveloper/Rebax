/* Runtime loader and renderer for the generated multi-page icon atlas. */
#include <stddef.h>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "icon_atlas_pages.h"
#ifdef __ANDROID__
#include <stdio.h>
#include <stdlib.h>
#include "asset_files.h"
#endif
#include "icon_atlas.h"
#include "window.h"

#define ATLAS_COLS 16
#define CELL_SIZE 16

static window_texture_t *g_atlas_tex[ICON_ATLAS_PAGE_COUNT];
static int g_initialized = 0;

int icon_atlas_init(void) {
    if (g_initialized) return 1;
    for (int page = 0; page < ICON_ATLAS_PAGE_COUNT; page++) {
        int w = 0, h = 0, channels = 0;
#ifdef __ANDROID__
        char asset_name[96];
        size_t asset_size = 0;
        snprintf(asset_name, sizeof(asset_name), "resources/images/icons/icons%d.png", page + 1);
        unsigned char *asset_data = asset_file_read(asset_name, &asset_size);
        unsigned char *pixels = asset_data
            ? stbi_load_from_memory(asset_data, (int)asset_size, &w, &h, &channels, 4)
            : NULL;
        free(asset_data);
#else
        unsigned char *pixels = stbi_load_from_memory(
            icon_atlas_page_data(page), (int)icon_atlas_page_size(page),
            &w, &h, &channels, 4);
#endif
        if (pixels == NULL || w != 256 || h != 256) {
            if (pixels != NULL) stbi_image_free(pixels);
            icon_atlas_shutdown();
            return 0;
        }
        g_atlas_tex[page] = window_create_texture(pixels, w, h);
        stbi_image_free(pixels);
        if (g_atlas_tex[page] == NULL) {
            icon_atlas_shutdown();
            return 0;
        }
    }
    g_initialized = 1;
    return 1;
}

static int atlas_for_icon(int icon_id, int *out_local_id) {
    int page_size = ATLAS_COLS * ATLAS_COLS;
    if (icon_id < 0 || icon_id >= ICON_COUNT) return -1;
    int page = icon_id / page_size;
    *out_local_id = icon_id % page_size;
    return page < ICON_ATLAS_PAGE_COUNT ? page : -1;
}

static void cell_position(int local_id, int *out_x, int *out_y) {
    *out_x = (local_id % ATLAS_COLS) * CELL_SIZE;
    *out_y = (local_id / ATLAS_COLS) * CELL_SIZE;
}

void icon_atlas_draw(int icon_id, int x, int y, int size) {
    if (!g_initialized) return;
    int local_id, page = atlas_for_icon(icon_id, &local_id);
    if (page < 0) return;
    int sx, sy;
    cell_position(local_id, &sx, &sy);
    window_draw_texture_region(g_atlas_tex[page], sx, sy, CELL_SIZE, CELL_SIZE,
                               x, y, size, size);
}

void icon_atlas_draw_rotated(int icon_id, int x, int y, int size, double angle_degrees) {
    if (!g_initialized) return;
    int local_id, page = atlas_for_icon(icon_id, &local_id);
    if (page < 0) return;
    int sx, sy;
    cell_position(local_id, &sx, &sy);
    window_draw_texture_region_rotated(g_atlas_tex[page], sx, sy, CELL_SIZE, CELL_SIZE,
                                       x, y, size, size, angle_degrees);
}

void icon_atlas_draw_tinted(int icon_id, int x, int y, int size,
                            unsigned char r, unsigned char g, unsigned char b) {
    if (!g_initialized) return;
    int local_id, page = atlas_for_icon(icon_id, &local_id);
    if (page < 0) return;
    int sx, sy;
    cell_position(local_id, &sx, &sy);
    window_draw_texture_region_tinted(g_atlas_tex[page], sx, sy, CELL_SIZE, CELL_SIZE,
                                      x, y, size, size, r, g, b);
}

void icon_atlas_shutdown(void) {
    for (int page = 0; page < ICON_ATLAS_PAGE_COUNT; page++) {
        if (g_atlas_tex[page] != NULL) {
            window_destroy_texture(g_atlas_tex[page]);
            g_atlas_tex[page] = NULL;
        }
    }
    g_initialized = 0;
}
