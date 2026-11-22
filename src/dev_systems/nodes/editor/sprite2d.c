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
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "editor/ui/theme.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>

#include "dev_systems/nodes/editor/node_editor_interface.h"
#include "editor/viewport/2d.h"
#include "dev_systems/nodes/editor/sprite2d.h"
#include "platform/common/window.h"
#include "formats/image_decoder.h"

#define MARKER_HALF        3
#define SELECT_CROSS_HALF  6
#define SPRITE_CACHE_MAX   32

typedef struct {
    char path[256];
    window_texture_t *tex;
    int width, height;
    int valid;
    int load_failed;
} sprite_cache_entry_t;

static sprite_cache_entry_t g_cache[SPRITE_CACHE_MAX];
static int g_cache_count = 0;

static sprite_cache_entry_t *sprite_cache_find(const char *path) {
    for (int i = 0; i < g_cache_count; i++) {
        if (g_cache[i].valid && strcmp(g_cache[i].path, path) == 0) return &g_cache[i];
    }
    return NULL;
}

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
    free(pixels);

    g_cache_count++;
    return entry;
}

void sprite2d_editor_draw(const node_property_value_t *values, int property_count,
                           int cam_x, int cam_y, int cam_w, int cam_h, int selected) {
    if (property_count < 9) return;

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
            window_draw_texture_region_rotated(entry->tex, 0, 0,
                                               entry->width, entry->height,
                                               sx - draw_w / 2, sy - draw_h / 2, draw_w, draw_h,
                                               (double)values[2].f * 180.0 / 3.14159265358979323846);
        }
    }

    window_fill_rect(sx - MARKER_HALF, sy - MARKER_HALF, MARKER_HALF * 2, MARKER_HALF * 2, UI_RGB(UI_COLOR_TEXT_PRIMARY));

    if (selected) {
        window_fill_rect(sx - SELECT_CROSS_HALF, sy - 1, SELECT_CROSS_HALF * 2, 2, UI_RGB(UI_COLOR_SUCCESS));
        window_fill_rect(sx - 1, sy - SELECT_CROSS_HALF, 2, SELECT_CROSS_HALF * 2, UI_RGB(UI_COLOR_SUCCESS));
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
