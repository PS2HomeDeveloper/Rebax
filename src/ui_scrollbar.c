#include "ui_scrollbar.h"
#include "window.h"

#define SCROLLBAR_MIN_THUMB  18
#define SCROLLBAR_GRAB_PAD    3 /* هامش إضافي لتسهيل الإمساك بالمقبض */

/* يحسب ارتفاع المقبض وموضعه العمودي */
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

    if (window_mouse_left_just_pressed() &&
        mx >= x + w - UI_SCROLLBAR_WIDTH - SCROLLBAR_GRAB_PAD && mx < x + w &&
        my >= thumb_y && my < thumb_y + thumb_h) {
        *dragging = 1;
    }
    if (!window_mouse_left_down()) *dragging = 0;
    if (*dragging && window_mouse_delta_y() != 0 && viewport_h > thumb_h) {
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
    window_fill_rect(x + w - UI_SCROLLBAR_WIDTH, viewport_y, UI_SCROLLBAR_WIDTH, viewport_h, 48, 48, 48);
    window_fill_rect(x + w - UI_SCROLLBAR_WIDTH, thumb_y, UI_SCROLLBAR_WIDTH, thumb_h, 112, 112, 112);
}
