#include "ui_layout.h"
#include "window.h"
#include "ui_theme.h"

void ui_layout_draw_panel(int x, int y, int w, int h, int tab_height) {
    if (w <= 0 || h <= 0) return;
    window_fill_rect(x, y, w, h, UI_COLOR_PANEL_BG.r, UI_COLOR_PANEL_BG.g, UI_COLOR_PANEL_BG.b);
    if (tab_height > h) tab_height = h;
    window_fill_rect(x, y, w, tab_height, UI_COLOR_BLACK_MUTED.r, UI_COLOR_BLACK_MUTED.g, UI_COLOR_BLACK_MUTED.b);
}

void ui_layout_draw_splitter(int x, int y, int w, int h, ui_splitter_visual_t state) {
    if (w <= 0 || h <= 0) return;
    const ui_color_t *color = &UI_COLOR_BLACK_MUTED;
    if (state == UI_SPLITTER_HOVER) color = &UI_COLOR_HOVER_BLUE;
    else if (state == UI_SPLITTER_ACTIVE) color = &UI_COLOR_BUTTON_BLUE;
    window_fill_rect(x, y, w, h, color->r, color->g, color->b);
}
