#include <stdlib.h>
#include <string.h>

#include "ui_tree.h"
#include "font.h"
#include "icon_atlas.h"
#include "ui_scrollbar.h"
#include "ui_selection.h"
#include "ui_common.h"

static void ui_tree_free_cache_item(ui_tree_cache_item_t *item) {
    if (item->name_tex != NULL) window_destroy_texture(item->name_tex);
    free(item->name);
    item->name = NULL;
    item->name_tex = NULL;
    item->name_w = 0;
    item->name_h = 0;
}

ui_tree_style_t ui_tree_default_style(void) {
    ui_tree_style_t style;
    style.row_height = UI_TREE_ROW_HEIGHT;
    style.icon_size = UI_TREE_ICON_SIZE;
    style.row_pad = UI_TREE_ROW_PAD;
    style.indent_step = UI_TREE_INDENT_STEP;
    style.arrow_icon_gap = UI_TREE_ARROW_ICON_GAP;
    style.highlight_indented = 1;
    style.highlight_radius = 8;
    style.font_pixel_size = 14;
    return style;
}

void ui_tree_init(ui_tree_t *tree, int max_items, const ui_tree_style_t *style) {
    if (tree == NULL) return;
    memset(tree, 0, sizeof(*tree));
    if (max_items < 1) max_items = 1;
    tree->cache = calloc((size_t)max_items, sizeof(*tree->cache));
    tree->cache_capacity = (tree->cache != NULL) ? max_items : 0;
    tree->style = (style != NULL) ? *style : ui_tree_default_style();
    tree->selected_index = -1;
    tree->hovered_index = -1;
    tree->clicked_index = -1;
    tree->toggled_index = -1;
    tree->ready = 1;
    icon_atlas_init();
}

static void ui_tree_ensure_cache(ui_tree_t *tree, int needed) {
    if (needed <= tree->cache_capacity) return;
    ui_tree_cache_item_t *grown = realloc(tree->cache, (size_t)needed * sizeof(*grown));
    if (grown == NULL) return;
    memset(grown + tree->cache_capacity, 0,
           (size_t)(needed - tree->cache_capacity) * sizeof(*grown));
    tree->cache = grown;
    tree->cache_capacity = needed;
}

static void ui_tree_cache_name(ui_tree_t *tree, int index, const char *name) {
    if (index < 0 || index >= tree->cache_capacity) return;
    ui_tree_cache_item_t *cached = &tree->cache[index];
    if (cached->name != NULL && strcmp(cached->name, name) == 0) return;

    ui_tree_free_cache_item(cached);
    cached->name = strdup(name);
    font_text_image_t text = font_render_text(name, FONT_WEIGHT_REGULAR, tree->style.font_pixel_size);
    cached->name_tex = (text.pixels != NULL) ? window_create_texture(text.pixels, text.width, text.height) : NULL;
    cached->name_w = text.width;
    cached->name_h = text.height;
    font_free_text_image(&text);
}

void ui_tree_set_items(ui_tree_t *tree, const ui_tree_item_t *items, int item_count) {
    if (tree == NULL || !tree->ready) return;
    if (item_count < 0) item_count = 0;
    ui_tree_ensure_cache(tree, item_count);
    if (item_count > tree->cache_capacity) item_count = tree->cache_capacity;

    for (int i = 0; i < item_count; i++) {
        ui_tree_cache_name(tree, i, (items != NULL && items[i].name != NULL) ? items[i].name : "");
    }
    for (int i = item_count; i < tree->item_count && i < tree->cache_capacity; i++) {
        ui_tree_free_cache_item(&tree->cache[i]);
    }

    tree->items = (ui_tree_item_t *)items;
    tree->item_count = item_count;
    if (tree->selected_index >= item_count) tree->selected_index = -1;
    if (tree->hovered_index >= item_count) tree->hovered_index = -1;
}

void ui_tree_set_selected(ui_tree_t *tree, int index) {
    if (tree == NULL) return;
    tree->selected_index = (index >= 0 && index < tree->item_count) ? index : -1;
}

int ui_tree_get_selected(const ui_tree_t *tree) {
    return (tree != NULL) ? tree->selected_index : -1;
}

int ui_tree_clicked_index(const ui_tree_t *tree) {
    return (tree != NULL) ? tree->clicked_index : -1;
}

int ui_tree_toggled_index(const ui_tree_t *tree) {
    return (tree != NULL) ? tree->toggled_index : -1;
}

static void ui_tree_update_highlights(ui_tree_t *tree, int width) {
    if (width <= 0 || tree->highlight_width == width) return;
    if (tree->hover_tex != NULL) window_destroy_texture(tree->hover_tex);
    if (tree->selected_tex != NULL) window_destroy_texture(tree->selected_tex);
    tree->hover_tex = ui_selection_make_hover(width, tree->style.row_height, tree->style.highlight_radius);
    tree->selected_tex = ui_selection_make_selected(width, tree->style.row_height, tree->style.highlight_radius);
    tree->highlight_width = width;
}

void ui_tree_update(ui_tree_t *tree, int x, int y, int w, int h) {
    if (tree == NULL || !tree->ready || tree->items == NULL) return;
    tree->clicked_index = -1;
    tree->toggled_index = -1;
    ui_tree_update_highlights(tree, w);

    int viewport_h = h;
    if (viewport_h < tree->style.row_height) viewport_h = tree->style.row_height;
    int content_h = tree->item_count * tree->style.row_height;
    ui_scrollbar_update(&tree->scroll_offset, &tree->scroll_dragging,
                        x, w, y, viewport_h, content_h, tree->style.row_height);

    int mx = window_mouse_x();
    int my = window_mouse_y();
    tree->hovered_index = -1;
    for (int i = 0; i < tree->item_count; i++) {
        const ui_tree_item_t *item = &tree->items[i];
        int row_y = y + i * tree->style.row_height - tree->scroll_offset;
        int base_x = x + tree->style.row_pad + item->depth * tree->style.indent_step;
        int row_start = tree->style.highlight_indented ? base_x : x;

        if (item->has_children && window_mouse_left_just_pressed() && !tree->scroll_dragging &&
            ui_point_in_rect(mx, my, base_x, row_y, tree->style.icon_size, tree->style.row_height)) {
            tree->toggled_index = i;
            break;
        }
        if (ui_point_in_rect(mx, my, row_start, row_y, x + w - row_start, tree->style.row_height)) {
            tree->hovered_index = i;
            if (window_mouse_left_just_pressed() && !tree->scroll_dragging) {
                tree->selected_index = i;
                tree->clicked_index = i;
            }
        }
    }
}

void ui_tree_draw(const ui_tree_t *tree, int x, int y, int w, int h) {
    if (tree == NULL || !tree->ready || tree->items == NULL) return;
    for (int i = 0; i < tree->item_count; i++) {
        const ui_tree_item_t *item = &tree->items[i];
        int row_y = y + i * tree->style.row_height - tree->scroll_offset;
        if (row_y + tree->style.row_height < y || row_y >= y + h) continue;

        int base_x = x + tree->style.row_pad + item->depth * tree->style.indent_step;
        int row_start = tree->style.highlight_indented ? base_x : x;
        int highlight_w = x + w - row_start;
        if (i == tree->selected_index && tree->selected_tex != NULL) {
            window_draw_texture(tree->selected_tex, row_start, row_y, highlight_w, tree->style.row_height);
        } else if (i == tree->hovered_index && tree->hover_tex != NULL) {
            window_draw_texture(tree->hover_tex, row_start, row_y, highlight_w, tree->style.row_height);
        }

        if (item->has_children && item->expanded) {
            int line_x = base_x + tree->style.icon_size / 2;
            int line_top = row_y + tree->style.row_height;
            int line_bottom = line_top;
            for (int j = i + 1; j < tree->item_count && tree->items[j].depth > item->depth; j++) {
                if (tree->items[j].depth == item->depth + 1) {
                    line_bottom = y + j * tree->style.row_height + tree->style.row_height / 2;
                }
            }
            window_fill_rect(line_x, line_top, 1, line_bottom - line_top, 255, 255, 255);
        }
        if (item->depth > 0) {
            int parent_line_x = base_x - tree->style.indent_step + tree->style.icon_size / 2;
            int mid_y = row_y + tree->style.row_height / 2;
            window_fill_rect(parent_line_x, mid_y, base_x - parent_line_x, 1, 255, 255, 255);
        }

        int icon_x = base_x + tree->style.icon_size + tree->style.arrow_icon_gap;
        if (item->has_children) {
            icon_atlas_draw_rotated(ICON_tree_expand_collapse, base_x,
                                    row_y + (tree->style.row_height - tree->style.icon_size) / 2,
                                    tree->style.icon_size, item->expanded ? 90.0 : 0.0);
        }
        int icon_y = row_y + (tree->style.row_height - tree->style.icon_size) / 2;
        if (item->thumbnail_tex != NULL) {
            window_draw_texture(item->thumbnail_tex, icon_x, icon_y,
                                tree->style.icon_size, tree->style.icon_size);
        } else if (item->icon_id == ICON_Folder) {
            icon_atlas_draw_tinted(item->icon_id, icon_x, icon_y,
                                   tree->style.icon_size, 230, 200, 60);
        } else {
            icon_atlas_draw(item->icon_id, icon_x, icon_y, tree->style.icon_size);
        }

        if (i < tree->cache_capacity && tree->cache[i].name_tex != NULL) {
            const ui_tree_cache_item_t *cached = &tree->cache[i];
            int name_x = icon_x + tree->style.icon_size + tree->style.row_pad;
            window_draw_texture(cached->name_tex, name_x,
                                row_y + (tree->style.row_height - cached->name_h) / 2,
                                cached->name_w, cached->name_h);
        }
    }
    int viewport_h = h;
    if (viewport_h < tree->style.row_height) viewport_h = tree->style.row_height;
    ui_scrollbar_draw(tree->scroll_offset, x, w, y, viewport_h,
                      tree->item_count * tree->style.row_height);
}

void ui_tree_shutdown(ui_tree_t *tree) {
    if (tree == NULL) return;
    for (int i = 0; i < tree->cache_capacity; i++) ui_tree_free_cache_item(&tree->cache[i]);
    free(tree->cache);
    if (tree->hover_tex != NULL) window_destroy_texture(tree->hover_tex);
    if (tree->selected_tex != NULL) window_destroy_texture(tree->selected_tex);
    memset(tree, 0, sizeof(*tree));
}
