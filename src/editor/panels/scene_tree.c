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

#include "editor/panels/scene_tree.h"
#include "scene/scene_data.h"
#include "editor/ui/tree.h"
#include "editor/ui/icon_atlas.h"
#include "dev_systems/nodes/runtime/node_registry.h"
#include "editor/panels/properties.h"
#include "editor/dialogs/add_node.h"
#include "platform/common/window.h"
#include "editor/dialogs/context_menu.h"
#include "editor/dialogs/prompt.h"

#define HEADER_HEIGHT 32

static ui_tree_t g_tree;
static ui_tree_item_t g_items[SCENE_DATA_MAX_NODES];
static int g_indices[SCENE_DATA_MAX_NODES];
static int g_depths[SCENE_DATA_MAX_NODES];
static int g_item_count = 0;
static int g_ready = 0;

enum { NODE_MENU_ADD = 1, NODE_MENU_COPY_PATH, NODE_MENU_CUT, NODE_MENU_COPY, NODE_MENU_PASTE,
       NODE_MENU_DUPLICATE, NODE_MENU_RENAME, NODE_MENU_DELETE };
static int g_menu_node = -1;

static void on_rename(const char *text, void *user_data) {
    (void)user_data;
    scene_data_rename(g_menu_node, text);
}

static void on_delete(const char *text, void *user_data) {
    (void)text; (void)user_data;
    scene_data_delete(g_menu_node);
    properties_panel_set_selected(-1);
}

static void on_node_menu(int id, void *user_data) {
    (void)user_data;
    int node = g_menu_node;
    if (node < 0 || node >= scene_data_get_node_count()) return;
    switch (id) {
        case NODE_MENU_ADD: scene_data_select_index(node); add_node_dialog_open(); break;
        case NODE_MENU_COPY_PATH: {
            char path[256];
            scene_data_get_path(node, path, sizeof(path));
            window_set_clipboard_text(path);
            break;
        }
        case NODE_MENU_CUT: scene_data_cut(node); properties_panel_set_selected(-1); break;
        case NODE_MENU_COPY: scene_data_copy(node); break;
        case NODE_MENU_PASTE: scene_data_paste(node); properties_panel_set_selected(scene_data_get_selected_index()); break;
        case NODE_MENU_DUPLICATE: scene_data_duplicate(node); properties_panel_set_selected(scene_data_get_selected_index()); break;
        case NODE_MENU_RENAME: prompt_input_open("Rename", scene_data_get_name(node), "Rename", on_rename, NULL); break;
        case NODE_MENU_DELETE: prompt_confirm_open("Delete", "This node and everything under it will be deleted.", "Delete", 1, on_delete, NULL); break;
        default: break;
    }
}

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
        g_items[i].plain_icon = 1;
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
    int menu_x, menu_y;
    int menu_row = ui_tree_menu_index(&g_tree, &menu_x, &menu_y);
    if (menu_row >= 0 && menu_row < g_item_count) {
        g_menu_node = g_indices[menu_row];
        scene_data_select_index(g_menu_node);
        properties_panel_set_selected(g_menu_node);
        context_menu_item_t items[8] = {
            { "Add Child Node", NODE_MENU_ADD, 1, 0 },
            { "Copy Path", NODE_MENU_COPY_PATH, 1, 1 },
            { "Cut", NODE_MENU_CUT, 1, 1 },
            { "Copy", NODE_MENU_COPY, 1, 0 },
            { "Paste", NODE_MENU_PASTE, scene_data_clipboard_has(), 0 },
            { "Duplicate", NODE_MENU_DUPLICATE, 1, 1 },
            { "Rename", NODE_MENU_RENAME, 1, 0 },
            { "Delete", NODE_MENU_DELETE, 1, 1 }
        };
        context_menu_open(menu_x, menu_y, items, 8, on_node_menu, NULL);
    }
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
