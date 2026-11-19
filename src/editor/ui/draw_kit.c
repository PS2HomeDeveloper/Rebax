/*
 *  ____  _____ ____    _    __  __
 * |  _ \| ____| __ )  / \   \ \/ /
 * | |_) | _| | _ \ / _ \   \  /
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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "editor/ui/draw_kit.h"
#include "editor/ui/base.h"
#include "editor/ui/font.h"
#include "editor/ui/shape_provider.h"
#include "platform/common/window.h"

#define TEXT_CACHE 48
#define SHAPE_CACHE 48

typedef struct {
    char text[200];
    int size;
    int used;
    window_texture_t *tex;
    int w, h;
} text_entry_t;

typedef struct {
    int w, h, radius, r, g, b, ring;
    int used;
    window_texture_t *tex;
} shape_entry_t;

static text_entry_t g_text[TEXT_CACHE];
static int g_text_next = 0;
static shape_entry_t g_shape[SHAPE_CACHE];
static int g_shape_next = 0;

static text_entry_t *text_entry(const char *text, int size) {
    for (int i = 0; i < TEXT_CACHE; i++)
        if (g_text[i].used && g_text[i].size == size && strcmp(g_text[i].text, text) == 0) return &g_text[i];
    text_entry_t *e = &g_text[g_text_next];
    g_text_next = (g_text_next + 1) % TEXT_CACHE;
    if (e->tex) window_destroy_texture(e->tex);
    memset(e, 0, sizeof(*e));
    snprintf(e->text, sizeof(e->text), "%s", text);
    e->size = size;
    e->used = 1;
    e->tex = ui_make_text_texture(text, FONT_WEIGHT_REGULAR, size, &e->w, &e->h);
    return e;
}

int ui_kit_text_width(const char *text, int size) {
    if (!text || !text[0]) return 0;
    return text_entry(text, size)->w;
}

int ui_kit_text_height(const char *text, int size) {
    if (!text || !text[0]) return 0;
    return text_entry(text, size)->h;
}

void ui_kit_text(const char *text, int size, int x, int y, int r, int g, int b) {
    if (!text || !text[0]) return;
    text_entry_t *e = text_entry(text, size);
    if (e->tex) window_draw_texture_tinted(e->tex, x, y, e->w, e->h, (unsigned char)r, (unsigned char)g, (unsigned char)b);
}

static window_texture_t *shape_texture(int w, int h, int radius, int r, int g, int b, int ring) {
    for (int i = 0; i < SHAPE_CACHE; i++) {
        shape_entry_t *s = &g_shape[i];
        if (s->used && s->w == w && s->h == h && s->radius == radius && s->r == r && s->g == g && s->b == b && s->ring == ring) return s->tex;
    }
    shape_entry_t *s = &g_shape[g_shape_next];
    g_shape_next = (g_shape_next + 1) % SHAPE_CACHE;
    if (s->tex) window_destroy_texture(s->tex);
    memset(s, 0, sizeof(*s));
    s->w = w; s->h = h; s->radius = radius; s->r = r; s->g = g; s->b = b; s->ring = ring; s->used = 1;
    shape_image_t outer = shape_provider_render_rect(w, h, radius, (unsigned char)r, (unsigned char)g, (unsigned char)b);
    if (!outer.pixels) return NULL;
    if (ring && w > 4 && h > 4) {
        shape_image_t inner = shape_provider_render_rect(w - 2, h - 2, radius > 1 ? radius - 1 : 0, 0, 0, 0);
        if (inner.pixels) {
            for (int y = 0; y < inner.height; y++)
                for (int x = 0; x < inner.width; x++) {
                    unsigned char *dst = outer.pixels + ((y + 1) * outer.width + (x + 1)) * 4;
                    unsigned char a = inner.pixels[(y * inner.width + x) * 4 + 3];
                    dst[3] = (unsigned char)(dst[3] > a ? dst[3] - a : 0);
                }
            shape_provider_free_image(&inner);
        }
    }
    s->tex = window_create_texture(outer.pixels, outer.width, outer.height);
    shape_provider_free_image(&outer);
    return s->tex;
}

void ui_kit_rect(int x, int y, int w, int h, int radius, int r, int g, int b) {
    if (w <= 0 || h <= 0) return;
    window_texture_t *t = shape_texture(w, h, radius, r, g, b, 0);
    if (t) window_draw_texture(t, x, y, w, h);
}

void ui_kit_outline(int x, int y, int w, int h, int radius, int r, int g, int b) {
    if (w <= 4 || h <= 4) return;
    window_texture_t *t = shape_texture(w, h, radius, r, g, b, 1);
    if (t) window_draw_texture(t, x, y, w, h);
}

void ui_kit_button(const char *text, int x, int y, int w, int h, int enabled, int primary) {
    int hover = enabled && ui_kit_hover(x, y, w, h);
    int r, g, b;
    if (!enabled) { r = 44; g = 46; b = 50; }
    else if (primary) { r = hover ? 0x4A : 0x3B; g = hover ? 0x70 : 0x5B; b = hover ? 0x90 : 0x74; }
    else { r = hover ? 70 : 56; g = hover ? 72 : 58; b = hover ? 78 : 63; }
    ui_kit_rect(x, y, w, h, 7, r, g, b);
    int tw = ui_kit_text_width(text, 14), th = ui_kit_text_height(text, 14);
    int c = enabled ? 240 : 115;
    ui_kit_text(text, 14, x + (w - tw) / 2, y + (h - th) / 2, c, c, c);
}

int ui_kit_hover(int x, int y, int w, int h) {
    return ui_point_in_rect(window_mouse_x(), window_mouse_y(), x, y, w, h);
}

int ui_kit_clicked(int x, int y, int w, int h) {
    return window_mouse_left_just_pressed() && ui_kit_hover(x, y, w, h);
}

void ui_kit_shutdown(void) {
    for (int i = 0; i < TEXT_CACHE; i++) { if (g_text[i].tex) window_destroy_texture(g_text[i].tex); memset(&g_text[i], 0, sizeof(g_text[i])); }
    for (int i = 0; i < SHAPE_CACHE; i++) { if (g_shape[i].tex) window_destroy_texture(g_shape[i].tex); memset(&g_shape[i], 0, sizeof(g_shape[i])); }
}
