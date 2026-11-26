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

#include <stddef.h>

#include "editor/ui/theme.h"
#include "editor/ui/scrollbar.h"
#include "platform/common/window.h"

#define SCROLLBAR_MIN_THUMB  18
#define SCROLLBAR_GRAB_PAD    3
#define TOUCH_SCROLL_THRESHOLD 10
#define TOUCH_FLING_DECAY 0.93f
#define TOUCH_FLING_MIN 0.6f

typedef struct {
    int *owner;
    int phase;
    int start_y;
    int start_offset;
    int last_offset;
    float velocity;
    int *fling_owner;
    float fling_velocity;
} touch_scroll_t;

static touch_scroll_t g_touch;

static int clamp_offset(int value, int max_scroll) {
    if (value < 0) return 0;
    if (value > max_scroll) return max_scroll;
    return value;
}

static void touch_scroll_update(int *scroll_offset, int *dragging, int x, int w, int viewport_y,
                                int viewport_h, int max_scroll, int on_thumb) {
    int mx = window_mouse_x();
    int my = window_mouse_y();
    int inside = mx >= x && mx < x + w - UI_SCROLLBAR_WIDTH - SCROLLBAR_GRAB_PAD &&
                 my >= viewport_y && my < viewport_y + viewport_h;
    if (g_touch.fling_owner == scroll_offset) {
        *scroll_offset = clamp_offset(*scroll_offset + (int)g_touch.fling_velocity, max_scroll);
        g_touch.fling_velocity *= TOUCH_FLING_DECAY;
        if (g_touch.fling_velocity > -TOUCH_FLING_MIN && g_touch.fling_velocity < TOUCH_FLING_MIN) {
            g_touch.fling_owner = NULL;
        }
        if (*scroll_offset == 0 || *scroll_offset == max_scroll) g_touch.fling_owner = NULL;
    }
    if (!window_pointer_is_touch() || max_scroll <= 0) return;
    if (window_mouse_left_just_pressed() && inside && !on_thumb && *dragging == 0) {
        g_touch.owner = scroll_offset;
        g_touch.phase = 1;
        g_touch.start_y = my;
        g_touch.start_offset = *scroll_offset;
        g_touch.last_offset = *scroll_offset;
        g_touch.velocity = 0.0f;
        if (g_touch.fling_owner == scroll_offset) g_touch.fling_owner = NULL;
    }
    if (g_touch.owner != scroll_offset || g_touch.phase == 0) return;
    if (!window_mouse_left_down()) {
        if (g_touch.phase == 2 && (g_touch.velocity > TOUCH_FLING_MIN * 2 || g_touch.velocity < -TOUCH_FLING_MIN * 2)) {
            g_touch.fling_owner = scroll_offset;
            g_touch.fling_velocity = g_touch.velocity;
        }
        g_touch.phase = 0;
        g_touch.owner = NULL;
        return;
    }
    if (g_touch.phase == 1 && my - g_touch.start_y >= TOUCH_SCROLL_THRESHOLD) g_touch.phase = 2;
    if (g_touch.phase == 1 && g_touch.start_y - my >= TOUCH_SCROLL_THRESHOLD) g_touch.phase = 2;
    if (g_touch.phase == 2) {
        int next = clamp_offset(g_touch.start_offset - (my - g_touch.start_y), max_scroll);
        g_touch.velocity = g_touch.velocity * 0.5f + (float)(next - g_touch.last_offset) * 0.5f;
        g_touch.last_offset = next;
        *scroll_offset = next;
        *dragging = 2;
    }
}

int ui_scrollbar_touch_active(const int *dragging) {
    return dragging != NULL && *dragging == 2;
}

static void compute_thumb(int scroll_offset, int viewport_y, int viewport_h,
                           int content_h, int max_scroll,
                           int *out_thumb_y, int *out_thumb_h) {
    int thumb_h = content_h > 0 ? (viewport_h * viewport_h) / content_h : viewport_h;
    if (thumb_h < SCROLLBAR_MIN_THUMB) thumb_h = SCROLLBAR_MIN_THUMB;
    if (thumb_h > viewport_h) thumb_h = viewport_h;
    *out_thumb_h = thumb_h;
    *out_thumb_y = viewport_y + (max_scroll ? (scroll_offset * (viewport_h - thumb_h)) / max_scroll : 0);
}

void ui_scrollbar_update(int *scroll_offset, int *dragging,
                          int x, int w, int viewport_y, int viewport_h,
                          int content_h, int wheel_step) {
    int mx = window_mouse_x(), my = window_mouse_y();
    int max_scroll = content_h - viewport_h;
    if (max_scroll < 0) max_scroll = 0;

    if (mx >= x && mx < x + w && my >= viewport_y && my < viewport_y + viewport_h) {
        int wheel = window_mouse_wheel_delta();
        if (wheel) *scroll_offset -= wheel * wheel_step;
    }
    if (*scroll_offset < 0) *scroll_offset = 0;
    if (*scroll_offset > max_scroll) *scroll_offset = max_scroll;

    int thumb_y, thumb_h;
    compute_thumb(*scroll_offset, viewport_y, viewport_h, content_h, max_scroll, &thumb_y, &thumb_h);

    int on_thumb = mx >= x + w - UI_SCROLLBAR_WIDTH - SCROLLBAR_GRAB_PAD && mx < x + w &&
                   my >= thumb_y && my < thumb_y + thumb_h;
    if (window_mouse_left_just_pressed() && on_thumb) {
        *dragging = 1;
    }
    if (!window_mouse_left_down()) *dragging = 0;
    touch_scroll_update(scroll_offset, dragging, x, w, viewport_y, viewport_h, max_scroll, on_thumb);
    if (*dragging == 1 && window_mouse_delta_y() != 0 && viewport_h > thumb_h) {
        *scroll_offset += window_mouse_delta_y() * max_scroll / (viewport_h - thumb_h);
        if (*scroll_offset < 0) *scroll_offset = 0;
        if (*scroll_offset > max_scroll) *scroll_offset = max_scroll;
    }
}

void ui_scrollbar_draw(int scroll_offset, int x, int w,
                        int viewport_y, int viewport_h, int content_h) {
    int max_scroll = content_h - viewport_h;
    if (max_scroll < 0) max_scroll = 0;
    int thumb_y, thumb_h;
    compute_thumb(scroll_offset, viewport_y, viewport_h, content_h, max_scroll, &thumb_y, &thumb_h);
    window_fill_rect(x + w - UI_SCROLLBAR_WIDTH, viewport_y, UI_SCROLLBAR_WIDTH, viewport_h, UI_RGB(UI_COLOR_SURFACE_CONTROL));
    window_fill_rect(x + w - UI_SCROLLBAR_WIDTH, thumb_y, UI_SCROLLBAR_WIDTH, thumb_h, UI_RGB(UI_COLOR_SCROLL_THUMB));
}
