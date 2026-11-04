#include <stdlib.h>
#include <string.h>

#include "ui_tabs.h"
#include "ui_common.h"
#include "ui_theme.h"

static void free_cache(ui_tab_cache_t *cache) {
    if (cache->label_tex != NULL) window_destroy_texture(cache->label_tex);
    free(cache->label);
    memset(cache, 0, sizeof(*cache));
}

ui_tabs_style_t ui_tabs_default_style(void) {
    ui_tabs_style_t style = {20, 10, 8, 14, 13};
    return style;
}

void ui_tabs_init(ui_tabs_t *tabs, int capacity, const ui_tabs_style_t *style) {
    if (tabs == NULL) return;
    memset(tabs, 0, sizeof(*tabs));
    if (capacity < 1) capacity = 1;
    tabs->cache = calloc((size_t)capacity, sizeof(*tabs->cache));
    tabs->capacity = tabs->cache ? capacity : 0;
    tabs->style = style ? *style : ui_tabs_default_style();
    tabs->selected_index = -1;
    tabs->hovered_index = -1;
    tabs->clicked_index = -1;
    tabs->close_clicked_index = -1;
    tabs->ready = 1;
}

static void cache_label(ui_tabs_t *tabs, int index, const ui_tab_item_t *item) {
    if (index < 0 || index >= tabs->capacity) return;
    ui_tab_cache_t *cache = &tabs->cache[index];
    const char *label = (item && item->label) ? item->label : "";
    if (cache->label != NULL && strcmp(cache->label, label) == 0 && cache->closable == item->closable) return;
    free_cache(cache);
    cache->label = strdup(label);
    cache->closable = item->closable;
    cache->label_tex = ui_make_text_texture(label, FONT_WEIGHT_REGULAR, tabs->style.font_pixel_size,
                                             &cache->label_w, &cache->label_h);
}

void ui_tabs_set_items(ui_tabs_t *tabs, const ui_tab_item_t *items, int item_count) {
    if (tabs == NULL || !tabs->ready) return;
    if (item_count < 0) item_count = 0;
    if (item_count > tabs->capacity) item_count = tabs->capacity;
    for (int i = 0; i < item_count; i++) cache_label(tabs, i, &items[i]);
    for (int i = item_count; i < tabs->item_count; i++) free_cache(&tabs->cache[i]);
    tabs->items = (ui_tab_item_t *)items;
    tabs->item_count = item_count;
    if (tabs->selected_index >= item_count) tabs->selected_index = -1;
}

void ui_tabs_set_selected(ui_tabs_t *tabs, int index) {
    if (tabs == NULL) return;
    tabs->selected_index = (index >= 0 && index < tabs->item_count) ? index : -1;
}

static int segment_width(const ui_tabs_t *tabs, int index) {
    const ui_tab_cache_t *cache = &tabs->cache[index];
    return tabs->style.padding_x * 2 + cache->label_w +
           (cache->closable ? tabs->style.label_gap + tabs->style.close_size : 0);
}

static void ensure_close_texture(ui_tabs_t *tabs) {
    if (tabs->close_tex != NULL) return;
    tabs->close_tex = ui_make_text_texture("x", FONT_WEIGHT_REGULAR, tabs->style.font_pixel_size,
                                            &tabs->close_w, &tabs->close_h);
}

void ui_tabs_update(ui_tabs_t *tabs, int x, int y, int w) {
    if (tabs == NULL || !tabs->ready) return;
    tabs->clicked_index = -1;
    tabs->close_clicked_index = -1;
    tabs->hovered_index = -1;
    int mx = window_mouse_x(), my = window_mouse_y();
    int tab_x = x;
    for (int i = 0; i < tabs->item_count; i++) {
        int width = segment_width(tabs, i);
        if (tab_x >= x + w) break;
        if (ui_point_in_rect(mx, my, tab_x, y, width, tabs->style.height)) {
            tabs->hovered_index = i;
            if (window_mouse_left_just_pressed()) {
                int close_x = tab_x + width - tabs->style.padding_x - tabs->style.close_size;
                if (tabs->cache[i].closable && ui_point_in_rect(mx, my, close_x, y,
                                                                  tabs->style.close_size, tabs->style.height)) {
                    tabs->close_clicked_index = i;
                } else {
                    tabs->selected_index = i;
                    tabs->clicked_index = i;
                }
            }
        }
        tab_x += width;
    }
}

void ui_tabs_draw(const ui_tabs_t *tabs, int x, int y, int w) {
    if (tabs == NULL || !tabs->ready || w <= 0) return;
    int tab_x = x;
    for (int i = 0; i < tabs->item_count; i++) {
        const ui_tab_cache_t *cache = &tabs->cache[i];
        int width = segment_width(tabs, i);
        if (tab_x >= x + w) break;
        const ui_color_t *background = NULL;
        if (i == tabs->selected_index) background = &UI_COLOR_TAB_ACTIVE;
        else if (i == tabs->hovered_index) background = &UI_COLOR_TAB_HOVER;
        if (background != NULL) window_fill_rect(tab_x, y, width, tabs->style.height,
                                                   background->r, background->g, background->b);
        if (cache->label_tex != NULL) {
            window_draw_texture(cache->label_tex, tab_x + tabs->style.padding_x,
                                y + (tabs->style.height - cache->label_h) / 2,
                                cache->label_w, cache->label_h);
        }
        if (cache->closable) {
            ui_tabs_t *mutable_tabs = (ui_tabs_t *)tabs;
            ensure_close_texture(mutable_tabs);
            if (mutable_tabs->close_tex != NULL) {
                int close_x = tab_x + width - tabs->style.padding_x - tabs->style.close_size;
                window_draw_texture(mutable_tabs->close_tex,
                                    close_x + (tabs->style.close_size - mutable_tabs->close_w) / 2,
                                    y + (tabs->style.height - mutable_tabs->close_h) / 2,
                                    mutable_tabs->close_w, mutable_tabs->close_h);
            }
        }
        tab_x += width;
    }
}

int ui_tabs_clicked_index(const ui_tabs_t *tabs) { return tabs ? tabs->clicked_index : -1; }
int ui_tabs_close_clicked_index(const ui_tabs_t *tabs) { return tabs ? tabs->close_clicked_index : -1; }

void ui_tabs_shutdown(ui_tabs_t *tabs) {
    if (tabs == NULL) return;
    for (int i = 0; i < tabs->capacity; i++) free_cache(&tabs->cache[i]);
    free(tabs->cache);
    if (tabs->close_tex != NULL) window_destroy_texture(tabs->close_tex);
    memset(tabs, 0, sizeof(*tabs));
}
