/* ui_tabs.h - general tabs with selection, hover and optional close button. */
#ifndef UI_TABS_H
#define UI_TABS_H

#include "window.h"

typedef struct {
    const char *label;
    int closable;
} ui_tab_item_t;

typedef struct {
    int height;
    int padding_x;
    int label_gap;
    int close_size;
    int font_pixel_size;
} ui_tabs_style_t;

typedef struct {
    char *label;
    int closable;
    window_texture_t *label_tex;
    int label_w;
    int label_h;
} ui_tab_cache_t;

typedef struct {
    ui_tab_item_t *items;
    int item_count;
    ui_tab_cache_t *cache;
    int capacity;
    ui_tabs_style_t style;
    int selected_index;
    int hovered_index;
    int clicked_index;
    int close_clicked_index;
    window_texture_t *close_tex;
    int close_w;
    int close_h;
    int ready;
} ui_tabs_t;

ui_tabs_style_t ui_tabs_default_style(void);
void ui_tabs_init(ui_tabs_t *tabs, int capacity, const ui_tabs_style_t *style);
void ui_tabs_set_items(ui_tabs_t *tabs, const ui_tab_item_t *items, int item_count);
void ui_tabs_set_selected(ui_tabs_t *tabs, int index);
int ui_tabs_clicked_index(const ui_tabs_t *tabs);
int ui_tabs_close_clicked_index(const ui_tabs_t *tabs);
void ui_tabs_update(ui_tabs_t *tabs, int x, int y, int w);
void ui_tabs_draw(const ui_tabs_t *tabs, int x, int y, int w);
void ui_tabs_shutdown(ui_tabs_t *tabs);

#endif /* UI_TABS_H */
