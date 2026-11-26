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
#include <stdlib.h>
#include <string.h>

#include "editor/ui/tree.h"
#include "editor/ui/font.h"
#include "editor/ui/icon_atlas.h"
#include "editor/ui/scrollbar.h"
#include "editor/ui/selection.h"
#include "editor/ui/base.h"
#include "editor/ui/draw_kit.h"

static void ui_tree_free_cache_item(ui_tree_cache_item_t *item) {
    if (item->name_tex != NULL) window_destroy_texture(item->name_tex);
    free(item->name);
    item->name = NULL;
    item->name_tex = NULL;
    item->name_w = 0;
    item->name_h = 0;
    item->fit_w = 0;
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
    tree->press_row = -1;
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
}

static void ui_tree_build_name_texture(const ui_tree_t *tree, int index, int max_w) {
    ui_tree_cache_item_t *cached = &tree->cache[index];
    char fitted[512];
    if (cached->name == NULL) return;
    if (cached->name_tex != NULL && cached->fit_w == max_w) return;
    if (cached->name_tex != NULL) window_destroy_texture(cached->name_tex);
    cached->name_tex = NULL;
    ui_kit_fit_text(cached->name, tree->style.font_pixel_size, max_w, fitted, (int)sizeof(fitted));
    font_text_image_t text = font_render_text(fitted, FONT_WEIGHT_REGULAR, tree->style.font_pixel_size);
    cached->name_tex = (text.pixels != NULL) ? window_create_texture(text.pixels, text.px_w, text.px_h) : NULL;
    cached->name_w = text.width;
    cached->name_h = text.height;
    cached->fit_w = max_w;
    font_free_text_image(&text);
}

static void ui_tree_compute_last_children(ui_tree_t *tree) {
    int stack[64];
    if (tree->last_child_capacity < tree->item_count) {
        int *grown = realloc(tree->last_child, (size_t)tree->item_count * sizeof(int));
        if (grown == NULL) return;
        tree->last_child = grown;
        tree->last_child_capacity = tree->item_count;
    }
    for (int i = 0; i < tree->item_count; i++) {
        int depth = tree->items[i].depth;
        tree->last_child[i] = i;
        if (depth < 0) depth = 0;
        if (depth > 63) depth = 63;
        if (depth > 0) tree->last_child[stack[depth - 1]] = i;
        stack[depth] = i;
    }
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
    if (tree->press_row >= item_count) tree->press_row = -1;
    if (items != NULL) ui_tree_compute_last_children(tree);
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
    int touch = window_pointer_is_touch();
    int pressed = window_mouse_left_just_pressed() && !tree->scroll_dragging;
    int activate_row = -1;
    int activate_toggle = 0;
    tree->hovered_index = -1;
    for (int i = 0; i < tree->item_count; i++) {
        const ui_tree_item_t *item = &tree->items[i];
        int row_y = y + i * tree->style.row_height - tree->scroll_offset;
        if (row_y + tree->style.row_height < y || row_y >= y + viewport_h) continue;
        int base_x = x + tree->style.row_pad + item->depth * tree->style.indent_step;
        int row_start = tree->style.highlight_indented ? base_x : x;
        int inside = my >= y && my < y + viewport_h;

        if (item->has_children && inside && ui_point_in_rect(mx, my, base_x, row_y, tree->style.icon_size, tree->style.row_height)) {
            if (pressed) {
                activate_row = i;
                activate_toggle = 1;
            }
            break;
        }
        if (inside && ui_point_in_rect(mx, my, row_start, row_y, x + w - row_start - UI_SCROLLBAR_WIDTH, tree->style.row_height)) {
            tree->hovered_index = touch ? -1 : i;
            if (pressed) {
                activate_row = i;
                activate_toggle = 0;
            }
        }
    }
    if (touch) {
        if (activate_row >= 0) {
            tree->press_row = activate_row;
            tree->press_toggle = activate_toggle;
            tree->press_moved = 0;
            activate_row = -1;
        }
        if (tree->scroll_dragging == 2) tree->press_moved = 1;
        if (tree->press_row >= 0 && !window_mouse_left_down()) {
            if (!tree->press_moved) {
                activate_row = tree->press_row;
                activate_toggle = tree->press_toggle;
            }
            tree->press_row = -1;
        }
    }
    if (activate_row >= 0 && activate_row < tree->item_count) {
        if (activate_toggle) {
            tree->toggled_index = activate_row;
        } else {
            tree->selected_index = activate_row;
            tree->clicked_index = activate_row;
        }
    }
}

void ui_tree_draw(const ui_tree_t *tree, int x, int y, int w, int h) {
    if (tree == NULL || !tree->ready || tree->items == NULL) return;
    int row_h = tree->style.row_height;
    if (tree->last_child != NULL) {
        for (int i = 0; i < tree->item_count; i++) {
            const ui_tree_item_t *item = &tree->items[i];
            if (!item->has_children || !item->expanded || tree->last_child[i] <= i) continue;
            int base_x = x + tree->style.row_pad + item->depth * tree->style.indent_step;
            int line_x = base_x + tree->style.icon_size / 2;
            int line_top = y + (i + 1) * row_h - tree->scroll_offset;
            int line_bottom = y + tree->last_child[i] * row_h + row_h / 2 - tree->scroll_offset;
            if (line_top < y) line_top = y;
            if (line_bottom > y + h) line_bottom = y + h;
            if (line_bottom > line_top) {
                window_fill_rect(line_x, line_top, 1, line_bottom - line_top, UI_RGB(UI_COLOR_TEXT_BRIGHT));
            }
        }
    }
    window_set_clip_rect(x, y, w, h);
    for (int i = 0; i < tree->item_count; i++) {
        const ui_tree_item_t *item = &tree->items[i];
        int row_y = y + i * row_h - tree->scroll_offset;
        if (row_y + row_h < y || row_y >= y + h) continue;

        int base_x = x + tree->style.row_pad + item->depth * tree->style.indent_step;
        int row_start = tree->style.highlight_indented ? base_x : x;
        int highlight_w = x + w - row_start;
        if (i == tree->selected_index && tree->selected_tex != NULL) {
            window_draw_texture(tree->selected_tex, row_start, row_y, highlight_w, row_h);
        } else if (i == tree->hovered_index && tree->hover_tex != NULL) {
            window_draw_texture(tree->hover_tex, row_start, row_y, highlight_w, row_h);
        }

        if (item->depth > 0) {
            int parent_line_x = base_x - tree->style.indent_step + tree->style.icon_size / 2;
            int mid_y = row_y + row_h / 2;
            window_fill_rect(parent_line_x, mid_y, base_x - parent_line_x, 1, UI_RGB(UI_COLOR_TEXT_BRIGHT));
        }

        int icon_x = base_x + tree->style.icon_size + tree->style.arrow_icon_gap;
        if (item->has_children) {
            icon_atlas_draw_rotated(ICON_arrow_right, base_x,
                                    row_y + (row_h - tree->style.icon_size) / 2,
                                    tree->style.icon_size, item->expanded ? 90.0 : 0.0);
        }
        int icon_y = row_y + (row_h - tree->style.icon_size) / 2;
        if (item->thumbnail_tex != NULL) {
            window_draw_texture(item->thumbnail_tex, icon_x, icon_y,
                                tree->style.icon_size, tree->style.icon_size);
        } else if (item->icon_id == ICON_folder) {
            icon_atlas_draw_tinted(item->icon_id, icon_x, icon_y,
                                   tree->style.icon_size, UI_RGB(UI_COLOR_WARNING));
        } else {
            icon_atlas_draw(item->icon_id, icon_x, icon_y, tree->style.icon_size);
        }

        if (i < tree->cache_capacity) {
            int name_x = icon_x + tree->style.icon_size + tree->style.row_pad;
            int name_max = x + w - UI_SCROLLBAR_WIDTH - 4 - name_x;
            if (name_max < 24) name_max = 24;
            ui_tree_build_name_texture(tree, i, name_max);
            const ui_tree_cache_item_t *cached = &tree->cache[i];
            if (cached->name_tex != NULL) {
                window_draw_texture(cached->name_tex, name_x,
                                    row_y + (row_h - cached->name_h) / 2,
                                    cached->name_w, cached->name_h);
            }
        }
    }
    window_clear_clip_rect();
    int viewport_h = h;
    if (viewport_h < row_h) viewport_h = row_h;
    ui_scrollbar_draw(tree->scroll_offset, x, w, y, viewport_h, tree->item_count * row_h);
}

void ui_tree_shutdown(ui_tree_t *tree) {
    if (tree == NULL) return;
    for (int i = 0; i < tree->cache_capacity; i++) ui_tree_free_cache_item(&tree->cache[i]);
    free(tree->cache);
    free(tree->last_child);
    if (tree->hover_tex != NULL) window_destroy_texture(tree->hover_tex);
    if (tree->selected_tex != NULL) window_destroy_texture(tree->selected_tex);
    memset(tree, 0, sizeof(*tree));
}
