/*
 * ============================================================
 * sprite2d.c (nodes_editor)
 * ============================================================
 * Visual representation of Sprite2D in the editor - loads the real
 * image via the unified image_loader.h (editor variant) which
 * supports exactly the same formats as the real PS2 build
 * (PNG/JPEG/BMP/TGA/TIFF via stb_image internally, and RAW/TIM2/TIM
 * via our own decoder) - and draws it at its actual position and size,
 * with a selection marker (a faint white dot + green plus when selected)
 * in the same style as the other nodes_editor nodes.
 *
 * Order: [0]=Position X, [1]=Position Y, [2]=Rotation, [3]=Scale X,
 * [4]=Scale Y, [5]=Image Path, [6]=Raw Width, [7]=Raw Height,
 * [8]=Raw Format - fixed here because this file itself is the source of
 * Sprite2D's property ordering (src/nodes/sprite2d.c).
 *
 * On-path cache: the first time an image is requested by a given path
 * it is loaded and stored, so any later request for the same path
 * (even from another node) reuses the same texture directly - without
 * rereading the disk each frame. A maximum of 32 different images may be
 * open simultaneously (a reasonable fixed capacity, no automatic
 * eviction yet - known first simplification, sufficient for now).
 * ============================================================
 */

#include <string.h>
#include <stdlib.h>
#include <math.h>

#include "node_editor_interface.h"
#include "viewport_2d.h"
#include "sprite2d.h"
#include "window.h"
#include "image_loader.h" /* Unified loader for the editor build - supports PNG/JPEG/BMP/TGA/TIFF (via stb_image internally) + RAW/TIM2/TIM (our own decoder) */

#define MARKER_HALF        3
#define SELECT_CROSS_HALF  6
#define SPRITE_CACHE_MAX   32

typedef struct {
    char path[256];
    window_texture_t *tex;
    int width, height;
    int valid;
    int load_failed; /* 1 if we tried loading and failed - avoid retrying every frame */
} sprite_cache_entry_t;

static sprite_cache_entry_t g_cache[SPRITE_CACHE_MAX];
static int g_cache_count = 0;

static sprite_cache_entry_t *sprite_cache_find(const char *path) {
    for (int i = 0; i < g_cache_count; i++) {
        if (g_cache[i].valid && strcmp(g_cache[i].path, path) == 0) return &g_cache[i];
    }
    return NULL;
}

/* Loads a new image into the cache (or returns an entry with a previous failure state) - NULL
 * if the cache is completely full and there's no room for a new image. raw_width/height/psm
 * are passed only to the RAW loader (ignored for any other format - see image_loader.h comment) */
static sprite_cache_entry_t *sprite_cache_load(const char *path, int raw_width, int raw_height, int raw_psm) {
    if (g_cache_count >= SPRITE_CACHE_MAX) return NULL;

    sprite_cache_entry_t *entry = &g_cache[g_cache_count];
    strncpy(entry->path, path, sizeof(entry->path) - 1);
    entry->path[sizeof(entry->path) - 1] = '\0';

    int w, h;
    unsigned char *pixels = image_loader_editor_load(path, &w, &h, raw_width, raw_height, raw_psm);

    if (pixels == NULL) {
        entry->valid = 1;
        entry->load_failed = 1;
        entry->tex = NULL;
        g_cache_count++;
        return entry;
    }

    entry->tex = window_create_texture(pixels, w, h);
    entry->width = w;
    entry->height = h;
    entry->valid = 1;
    entry->load_failed = 0;
    free(pixels); /* Allocated with malloc by image_loader_editor_load - free with normal free, not stbi_image_free */

    g_cache_count++;
    return entry;
}

void sprite2d_editor_draw(const node_property_value_t *values, int property_count,
                           int cam_x, int cam_y, int cam_w, int cam_h, int selected) {
    if (property_count < 9) return; /* Minimum: all nine Sprite2D properties (see src/nodes/sprite2d.c) */

    float pos_x = values[0].f;
    float pos_y = values[1].f;
    float scale_x = values[3].f;
    float scale_y = values[4].f;
    float zoom = viewport_2d_get_zoom();
    const char *path = values[5].s;
    int raw_width = values[6].i;
    int raw_height = values[7].i;
    int raw_format = values[8].i;

    int sx, sy;
    viewport_2d_project(pos_x, pos_y, cam_x, cam_y, cam_w, cam_h, &sx, &sy);

    if (path != NULL && path[0] != '\0') {
        sprite_cache_entry_t *entry = sprite_cache_find(path);
        if (entry == NULL) entry = sprite_cache_load(path, raw_width, raw_height, raw_format);

        if (entry != NULL && !entry->load_failed && entry->tex != NULL) {
            int draw_w = (int)((float)entry->width  * scale_x * zoom);
            int draw_h = (int)((float)entry->height * scale_y * zoom);
            if (draw_w < 1) draw_w = 1;
            if (draw_h < 1) draw_h = 1;
            /* (0,0) is the image center, and rotation is in radians like the PS2 build.
             * Using the same transform here prevents the preview from differing from the
             * Native output at export time. */
            window_draw_texture_region_rotated(entry->tex, 0, 0,
                                               entry->width, entry->height,
                                               sx - draw_w / 2, sy - draw_h / 2, draw_w, draw_h,
                                               (double)values[2].f * 180.0 / 3.14159265358979323846);
        }
    }

    /* A faint white dot always indicates the node's position (its origin) - even
     * if the image hasn't loaded or failed yet - exactly like Element2D */
    window_fill_rect(sx - MARKER_HALF, sy - MARKER_HALF, MARKER_HALF * 2, MARKER_HALF * 2, 225, 225, 225);

    if (selected) {
        window_fill_rect(sx - SELECT_CROSS_HALF, sy - 1, SELECT_CROSS_HALF * 2, 2, 70, 220, 120);
        window_fill_rect(sx - 1, sy - SELECT_CROSS_HALF, 2, SELECT_CROSS_HALF * 2, 70, 220, 120);
    }
}

int sprite2d_editor_hit_test(const node_property_value_t *values, int property_count,
                              int screen_x, int screen_y, int pivot_x, int pivot_y,
                              float viewport_zoom) {
    if (values == NULL || property_count < 9) return 0;
    const char *path = values[5].s;
    if (path == NULL || path[0] == '\0') {
        return abs(screen_x - pivot_x) <= 10 && abs(screen_y - pivot_y) <= 10;
    }
    sprite_cache_entry_t *entry = sprite_cache_find(path);
    if (entry == NULL) entry = sprite_cache_load(path, values[6].i, values[7].i, values[8].i);
    if (entry == NULL || entry->load_failed || entry->width <= 0 || entry->height <= 0) {
        return abs(screen_x - pivot_x) <= 10 && abs(screen_y - pivot_y) <= 10;
    }
    double angle = (double)values[2].f;
    double cs = cos(angle), sn = sin(angle);
    double dx = (double)(screen_x - pivot_x), dy = (double)(screen_y - pivot_y);
    double local_x = cs * dx + sn * dy;
    double local_y = -sn * dx + cs * dy;
    double half_w = (double)entry->width * values[3].f * viewport_zoom * 0.5;
    double half_h = (double)entry->height * values[4].f * viewport_zoom * 0.5;
    if (half_w < 8.0) half_w = 8.0;
    if (half_h < 8.0) half_h = 8.0;
    return fabs(local_x) <= half_w && fabs(local_y) <= half_h;
}

/* @NODE_EDITOR type=NODE_TYPE_SPRITE_2D draw_2d=sprite2d_editor_draw */
