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

#include <string.h>
#include <stddef.h>

#include "editor/ui/dialog.h"
#include "editor/ui/shape_provider.h"
#include "editor/ui/base.h"
#include "editor/ui/theme.h"

static window_texture_t *g_backdrop_tex = NULL;
static window_texture_t *g_close_bg_tex = NULL;
static window_texture_t *g_close_x_tex = NULL;
static int g_close_size = 0;
static int g_close_x_w = 0, g_close_x_h = 0;

#define DRAG_CLOSE_ZONE 52
#define EDGE_ZONE 12
#define EDGE_L 1
#define EDGE_R 2
#define EDGE_T 4
#define EDGE_B 8
#define SCREEN_MARGIN 8

void ui_dialog_drag_reset(ui_dialog_drag_t *drag) {
    memset(drag, 0, sizeof(*drag));
}

static int edges_at(const ui_dialog_drag_t *d, int mx, int my) {
    int e = 0;
    if (mx < d->x - 6 || mx >= d->x + d->w + 6 || my < d->y - 6 || my >= d->y + d->h + 6) return 0;
    if (mx < d->x + EDGE_ZONE) e |= EDGE_L;
    if (mx >= d->x + d->w - EDGE_ZONE) e |= EDGE_R;
    if (my < d->y + EDGE_ZONE) e |= EDGE_T;
    if (my >= d->y + d->h - EDGE_ZONE) e |= EDGE_B;
    return e;
}

void ui_dialog_place(ui_dialog_drag_t *d, int interactive, int window_w, int window_h,
                     int def_w, int def_h, int min_w, int min_h, int title_h, int resizable,
                     int *out_x, int *out_y, int *out_w, int *out_h) {
    int max_w = window_w - SCREEN_MARGIN * 2;
    int max_h = window_h - SCREEN_MARGIN * 2;
    if (!d->placed) {
        d->w = def_w;
        d->h = def_h;
        d->x = (window_w - def_w) / 2;
        d->y = (window_h - def_h) / 2;
        d->placed = 1;
    }
    if (!resizable) {
        d->w = def_w;
        d->h = def_h;
    }
    if (interactive) {
        int mx = window_mouse_x(), my = window_mouse_y();
        int down = window_mouse_left_down();
        if (d->mode == 0 && window_mouse_left_just_pressed()) {
            int e = resizable ? edges_at(d, mx, my) : 0;
            if (e != 0) {
                d->mode = 2;
                d->edges = e;
            } else if (mx >= d->x && mx < d->x + d->w - DRAG_CLOSE_ZONE && my >= d->y && my < d->y + title_h) {
                d->mode = 1;
            }
            if (d->mode != 0) {
                d->grab_x = mx;
                d->grab_y = my;
                d->start_x = d->x;
                d->start_y = d->y;
                d->start_w = d->w;
                d->start_h = d->h;
            }
        }
        if (d->mode != 0 && !down) d->mode = 0;
        if (d->mode == 1) {
            d->x = d->start_x + (mx - d->grab_x);
            d->y = d->start_y + (my - d->grab_y);
        } else if (d->mode == 2) {
            int ddx = mx - d->grab_x, ddy = my - d->grab_y;
            int nx = d->start_x, ny = d->start_y, nw = d->start_w, nh = d->start_h;
            if (d->edges & EDGE_R) nw = d->start_w + ddx;
            if (d->edges & EDGE_L) { nw = d->start_w - ddx; }
            if (d->edges & EDGE_B) nh = d->start_h + ddy;
            if (d->edges & EDGE_T) { nh = d->start_h - ddy; }
            if (nw < min_w) nw = min_w;
            if (nh < min_h) nh = min_h;
            if (nw > max_w) nw = max_w;
            if (nh > max_h) nh = max_h;
            if (d->edges & EDGE_L) nx = d->start_x + d->start_w - nw;
            if (d->edges & EDGE_T) ny = d->start_y + d->start_h - nh;
            d->x = nx; d->y = ny; d->w = nw; d->h = nh;
        }
    }
    if (d->w > max_w && max_w >= min_w) d->w = max_w;
    if (d->h > max_h && max_h >= min_h) d->h = max_h;
    if (d->w <= window_w) { if (d->x < 0) d->x = 0; if (d->x + d->w > window_w) d->x = window_w - d->w; }
    else d->x = (window_w - d->w) / 2;
    if (d->h <= window_h) { if (d->y < 0) d->y = 0; if (d->y + d->h > window_h) d->y = window_h - d->h; }
    else d->y = (window_h - d->h) / 2;
    *out_x = d->x;
    *out_y = d->y;
    *out_w = d->w;
    *out_h = d->h;
}

void ui_dialog_draw_backdrop(int window_w, int window_h) {
    if (g_backdrop_tex == NULL) {
        unsigned char pixels[4 * 4 * 4];
        for (int i = 0; i < 16; i++) {
            pixels[i * 4] = 8; pixels[i * 4 + 1] = 8; pixels[i * 4 + 2] = 8; pixels[i * 4 + 3] = 70;
        }
        g_backdrop_tex = window_create_texture(pixels, 4, 4);
    }
    if (g_backdrop_tex != NULL) window_draw_texture(g_backdrop_tex, 0, 0, window_w, window_h);
}

#define FRAME_RADIUS 8
#define FRAME_BORDER 2
static window_texture_t *g_frame_tex = NULL;
static int g_frame_w = 0, g_frame_h = 0;

void ui_dialog_draw_frame(int x, int y, int w, int h) {
    if (g_frame_tex == NULL || g_frame_w != w || g_frame_h != h) {
        if (g_frame_tex != NULL) window_destroy_texture(g_frame_tex);
        g_frame_tex = NULL;
        g_frame_w = w;
        g_frame_h = h;
        shape_image_t outer = shape_provider_render_rect(w + FRAME_BORDER * 2, h + FRAME_BORDER * 2, FRAME_RADIUS + FRAME_BORDER, UI_RGB(UI_COLOR_BACKDROP));
        shape_image_t inner = shape_provider_render_rect(w, h, FRAME_RADIUS, UI_COLOR_PANEL_BG.r, UI_COLOR_PANEL_BG.g, UI_COLOR_PANEL_BG.b);
        if (outer.pixels != NULL && inner.pixels != NULL) {
            for (int row = 0; row < inner.height; row++) {
                for (int col = 0; col < inner.width; col++) {
                    unsigned char *src = &inner.pixels[(row * inner.width + col) * 4];
                    unsigned char *dst = &outer.pixels[((row + FRAME_BORDER) * outer.width + col + FRAME_BORDER) * 4];
                    if (src[3] == 255) { dst[0] = src[0]; dst[1] = src[1]; dst[2] = src[2]; dst[3] = 255; }
                    else if (src[3] > 0 && dst[3] > 0) {
                        int a = src[3];
                        dst[0] = (unsigned char)((src[0] * a + dst[0] * (255 - a)) / 255);
                        dst[1] = (unsigned char)((src[1] * a + dst[1] * (255 - a)) / 255);
                        dst[2] = (unsigned char)((src[2] * a + dst[2] * (255 - a)) / 255);
                    }
                }
            }
            g_frame_tex = window_create_texture(outer.pixels, outer.width, outer.height);
        }
        shape_provider_free_image(&outer);
        shape_provider_free_image(&inner);
    }
    if (g_frame_tex != NULL) {
        window_draw_texture(g_frame_tex, x - FRAME_BORDER, y - FRAME_BORDER, w + FRAME_BORDER * 2, h + FRAME_BORDER * 2);
    } else {
        window_fill_rect(x, y, w, h, UI_COLOR_PANEL_BG.r, UI_COLOR_PANEL_BG.g, UI_COLOR_PANEL_BG.b);
    }
}

void ui_dialog_draw_title_left(window_texture_t *title, int title_w, int title_h,
                               int x, int y, int titlebar_height, int padding) {
    if (title != NULL) window_draw_texture(title, x + padding,
                                           y + (titlebar_height - title_h) / 2, title_w, title_h);
}

void ui_dialog_draw_title_centered(window_texture_t *title, int title_w, int title_h,
                                   int x, int y, int w, int titlebar_height) {
    if (title != NULL) window_draw_texture(title, x + (w - title_w) / 2,
                                           y + (titlebar_height - title_h) / 2, title_w, title_h);
}

static void ensure_close_button(int size) {
    if (g_close_size == size && g_close_bg_tex != NULL && g_close_x_tex != NULL) return;
    if (g_close_bg_tex != NULL) window_destroy_texture(g_close_bg_tex);
    if (g_close_x_tex != NULL) window_destroy_texture(g_close_x_tex);
    g_close_bg_tex = g_close_x_tex = NULL;
    g_close_size = size;
    shape_image_t shape = shape_provider_render_rect(size, size, 5, UI_RGB(UI_COLOR_DANGER_BG));
    if (shape.pixels != NULL) {
        g_close_bg_tex = window_create_texture(shape.pixels, shape.width, shape.height);
        shape_provider_free_image(&shape);
    }
    g_close_x_tex = ui_make_text_texture("X", FONT_WEIGHT_BOLD, 14, &g_close_x_w, &g_close_x_h);
}

void ui_dialog_draw_close_button(int x, int y, int size) {
    ensure_close_button(size);
    if (g_close_bg_tex != NULL) window_draw_texture(g_close_bg_tex, x, y, size, size);
    if (g_close_x_tex != NULL) window_draw_texture(g_close_x_tex,
                                                    x + (size - g_close_x_w) / 2,
                                                    y + (size - g_close_x_h) / 2,
                                                    g_close_x_w, g_close_x_h);
}

int ui_dialog_close_button_hit(int mouse_x, int mouse_y, int x, int y, int size) {
    return mouse_x >= x && mouse_x < x + size && mouse_y >= y && mouse_y < y + size;
}

void ui_dialog_shutdown(void) {
    if (g_backdrop_tex != NULL) window_destroy_texture(g_backdrop_tex);
    if (g_frame_tex != NULL) window_destroy_texture(g_frame_tex);
    g_frame_tex = NULL;
    g_frame_w = g_frame_h = 0;
    if (g_close_bg_tex != NULL) window_destroy_texture(g_close_bg_tex);
    if (g_close_x_tex != NULL) window_destroy_texture(g_close_x_tex);
    g_backdrop_tex = g_close_bg_tex = g_close_x_tex = NULL;
    g_close_size = g_close_x_w = g_close_x_h = 0;
}
