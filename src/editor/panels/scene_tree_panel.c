/* Scene tree display panel. Data and saving are in scene_data, general rendering in ui_tree. */
#include "scene_tree_panel.h"
#include "scene_data.h"
#include "ui_tree.h"
#include "icon_atlas.h"
#include "node_registry.h"
#include "properties_panel.h"
#include "add_node_dialog.h"
#include "window.h"

#define HEADER_HEIGHT 28

static ui_tree_t g_tree;
static ui_tree_item_t g_items[SCENE_DATA_MAX_NODES];
static int g_indices[SCENE_DATA_MAX_NODES];
static int g_depths[SCENE_DATA_MAX_NODES];
static int g_item_count = 0;
static int g_ready = 0;

static void ensure_ready(void) {
    if (g_ready) return;
    ui_tree_style_t style = ui_tree_default_style();
    style.font_pixel_size = 15;
    style.highlight_indented = 1;
    ui_tree_init(&g_tree, SCENE_DATA_MAX_NODES, &style);
    g_ready = 1;
}

static void rebuild_tree_items(void) {
    g_item_count = scene_data_build_visible(g_indices, g_depths, SCENE_DATA_MAX_NODES);
    for (int i = 0; i < g_item_count; i++) {
        int node_index = g_indices[i];
        const node_registry_entry_t *entry = node_registry_get(scene_data_get_type(node_index));
        g_items[i].name = scene_data_get_name(node_index);
        g_items[i].icon_id = entry ? entry->icon_id : ICON_file;
        g_items[i].depth = g_depths[i];
        g_items[i].has_children = scene_data_has_children(node_index);
        g_items[i].expanded = scene_data_is_expanded(node_index);
    }
    ui_tree_set_items(&g_tree, g_items, g_item_count);
    int selected = scene_data_get_selected_index();
    int visual_selected = -1;
    for (int i = 0; i < g_item_count; i++) if (g_indices[i] == selected) { visual_selected = i; break; }
    ui_tree_set_selected(&g_tree, visual_selected);
}

void scene_tree_panel_update(int x, int y, int w, int h) {
    ensure_ready();
    rebuild_tree_items();
    int add_x = x + w - UI_TREE_ICON_SIZE - UI_TREE_ROW_PAD;
    int add_y = y + (HEADER_HEIGHT - UI_TREE_ICON_SIZE) / 2;
    if (window_mouse_left_just_pressed() &&
        window_mouse_x() >= add_x && window_mouse_x() < add_x + UI_TREE_ICON_SIZE &&
        window_mouse_y() >= add_y && window_mouse_y() < add_y + UI_TREE_ICON_SIZE) add_node_dialog_open();

    ui_tree_update(&g_tree, x, y + HEADER_HEIGHT, w, h - HEADER_HEIGHT);
    int toggled = ui_tree_toggled_index(&g_tree);
    if (toggled >= 0 && toggled < g_item_count) scene_data_toggle_expanded(g_indices[toggled]);
    int clicked = ui_tree_clicked_index(&g_tree);
    if (clicked >= 0 && clicked < g_item_count) {
        scene_data_select_index(g_indices[clicked]);
        properties_panel_set_selected(g_indices[clicked]);
    }
}

void scene_tree_panel_draw(int x, int y, int w, int h) {
    icon_atlas_draw(ICON_add, x + w - UI_TREE_ICON_SIZE - UI_TREE_ROW_PAD,
                    y + (HEADER_HEIGHT - UI_TREE_ICON_SIZE) / 2, UI_TREE_ICON_SIZE);
    ui_tree_draw(&g_tree, x, y + HEADER_HEIGHT, w, h - HEADER_HEIGHT);
}

void scene_tree_panel_shutdown(void) {
    ui_tree_shutdown(&g_tree);
    g_ready = 0;
    g_item_count = 0;
}
