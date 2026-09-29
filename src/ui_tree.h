/*
 * ui_tree.h - شجرة واجهة عامة: الرسم، التحديد، التحويم والتمرير.
 * تمرر اللوحة بيانات الصفوف فقط، وتبقى دلالتها ومنطقها خارج المكتبة.
 */
#ifndef UI_TREE_H
#define UI_TREE_H

#include "window.h"

#define UI_TREE_ROW_HEIGHT 22
#define UI_TREE_ICON_SIZE 16
#define UI_TREE_ROW_PAD 6
#define UI_TREE_INDENT_STEP 14
#define UI_TREE_ARROW_ICON_GAP 2

typedef struct {
    const char *name;
    int icon_id;
    int depth;
    int has_children;
    int expanded;
} ui_tree_item_t;

typedef struct {
    int row_height;
    int icon_size;
    int row_pad;
    int indent_step;
    int arrow_icon_gap;
    int highlight_indented; /* 1: يبدأ عند السهم، 0: يغطي عرض الصف. */
    int highlight_radius;
    int font_pixel_size;
} ui_tree_style_t;

typedef struct {
    char *name;
    window_texture_t *name_tex;
    int name_w;
    int name_h;
} ui_tree_cache_item_t;

typedef struct {
    ui_tree_item_t *items;
    int item_count;
    ui_tree_cache_item_t *cache;
    int cache_capacity;
    ui_tree_style_t style;
    int selected_index;
    int hovered_index;
    int clicked_index;
    int toggled_index;
    int scroll_offset;
    int scroll_dragging;
    window_texture_t *hover_tex;
    window_texture_t *selected_tex;
    int highlight_width;
    int ready;
} ui_tree_t;

ui_tree_style_t ui_tree_default_style(void);
void ui_tree_init(ui_tree_t *tree, int max_items, const ui_tree_style_t *style);
void ui_tree_set_items(ui_tree_t *tree, const ui_tree_item_t *items, int item_count);
void ui_tree_set_selected(ui_tree_t *tree, int index);
int ui_tree_get_selected(const ui_tree_t *tree);
int ui_tree_clicked_index(const ui_tree_t *tree);
int ui_tree_toggled_index(const ui_tree_t *tree);
void ui_tree_update(ui_tree_t *tree, int x, int y, int w, int h);
void ui_tree_draw(const ui_tree_t *tree, int x, int y, int w, int h);
void ui_tree_shutdown(ui_tree_t *tree);

#endif /* UI_TREE_H */
