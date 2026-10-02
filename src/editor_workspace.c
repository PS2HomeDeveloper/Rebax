/* تخطيط المحرر: مناطق قابلة للتغيير، وتبويبات مشتركة لكل البانلات. */
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "editor_workspace.h"
#include "window.h"
#include "ui_theme.h"
#include "ui_common.h"
#include "ui_tabs.h"
#include "ui_layout.h"
#include "ui_dialog.h"
#include "scene_tree_panel.h"
#include "scene_data.h"
#include "current_scene.h"
#include "scene_tabs.h"
#include "close_scene_dialog.h"
#include "file_manager.h"
#include "add_node_dialog.h"
#include "file_system_panel.h"
#include "properties_panel.h"
#include "icon_atlas.h"
#include "export_dialog.h"
#include "viewport_2d.h"
#include "viewport_3d.h"
#include "code_editor.h"

#define TOOLBAR_HEIGHT 26
#define TAB_BAR_HEIGHT 20
#define SCENE_TABS_BAR_HEIGHT TAB_BAR_HEIGHT
#define CAMERA_TOP_BAR_HEIGHT TAB_BAR_HEIGHT
#define SPLITTER_THICKNESS 6
#define CAMERA_MIN_W 200
#define CAMERA_MIN_H 150
#define SCENE_TAB_PLUS_SIZE 14
#define SCENE_TAB_PLUS_MARGIN 8
#define PANEL_MOVE_WIDTH 440
#define PANEL_MOVE_HEIGHT 270
#define PANEL_MOVE_TITLEBAR 38
#define PANEL_MOVE_CLOSE_SIZE 22

typedef enum {
    SPLITTER_NONE = -1,
    SPLITTER_LEFT_RIGHT_COL,
    SPLITTER_CENTER_RIGHT_COL,
    SPLITTER_LEFT_INNER,
    SPLITTER_RIGHT_INNER,
    SPLITTER_CAMERA_BOTTOM
} splitter_id_t;

typedef enum { PANEL_SCENE, PANEL_FILES, PANEL_PROPERTIES, PANEL_COUNT } editor_panel_t;
typedef enum {
    PANEL_REGION_LEFT_TOP,
    PANEL_REGION_RIGHT_TOP,
    PANEL_REGION_LEFT_BOTTOM,
    PANEL_REGION_RIGHT_BOTTOM,
    PANEL_REGION_COUNT
} panel_region_t;
typedef enum { CAMERA_TAB_3D, CAMERA_TAB_2D, CAMERA_TAB_SCRIPT, CAMERA_TAB_COUNT } camera_tab_t;

typedef struct { int x, y, w, h; } workspace_rect_t;

static int g_left_width = 260;
static int g_right_width = 300;
static int g_left_split_y = 300;
static int g_right_split_y = 300;
static int g_bottom_center_height = 220;
static splitter_id_t g_dragging = SPLITTER_NONE;
static splitter_id_t g_hovered_splitter = SPLITTER_NONE;
static editor_camera_mode_t g_camera_mode = EDITOR_CAMERA_MODE_3D;
static camera_tab_t g_camera_tab = CAMERA_TAB_3D;

/* الافتراضات تطابق الواجهة السابقة تماماً. */
static panel_region_t g_panel_region[PANEL_COUNT] = {
    PANEL_REGION_LEFT_TOP, PANEL_REGION_LEFT_BOTTOM, PANEL_REGION_RIGHT_TOP
};
static int g_region_active[PANEL_REGION_COUNT] = {
    PANEL_SCENE, PANEL_PROPERTIES, PANEL_FILES, -1
};
static int g_move_panel = -1;

static ui_tabs_t g_scene_tabs;
static ui_tabs_t g_camera_tabs;
static ui_tabs_t g_region_tabs[PANEL_REGION_COUNT];
static ui_tab_item_t g_scene_tab_items[SCENE_TABS_MAX];
static char g_scene_tab_labels[SCENE_TABS_MAX][160];
static ui_tab_item_t g_camera_tab_items[CAMERA_TAB_COUNT] = {
    {"3D", 0}, {"2D", 0}, {"Script", 0}
};
static ui_tab_item_t g_region_tab_items[PANEL_REGION_COUNT][PANEL_COUNT];
static int g_region_tab_panels[PANEL_REGION_COUNT][PANEL_COUNT];
static int g_region_tab_count[PANEL_REGION_COUNT];
static const char *g_panel_labels[PANEL_COUNT] = { "Scene", "Files", "Properties" };

#define TOOLBAR_MENU_ITEM_COUNT 4
static const char *g_toolbar_menu_labels[TOOLBAR_MENU_ITEM_COUNT] = {
    "Export", "Project Settings", "Projects", "Editor Settings"
};
static window_texture_t *g_toolbar_menu_tex[TOOLBAR_MENU_ITEM_COUNT];
static int g_toolbar_menu_w[TOOLBAR_MENU_ITEM_COUNT];
static int g_toolbar_menu_h[TOOLBAR_MENU_ITEM_COUNT];
static int g_toolbar_menu_hovered = -1;

static window_texture_t *g_move_title_tex = NULL;
static int g_move_title_w = 0, g_move_title_h = 0;
static window_texture_t *g_move_label_tex[PANEL_REGION_COUNT];
static int g_move_label_w[PANEL_REGION_COUNT], g_move_label_h[PANEL_REGION_COUNT];
static const char *g_move_region_labels[PANEL_REGION_COUNT] = {
    "Top left", "Top right", "Bottom left", "Bottom right"
};
static int g_ui_ready = 0;

static void ensure_ui_ready(void) {
    if (g_ui_ready) return;
    g_ui_ready = 1;
    ui_tabs_style_t tabs_style = ui_tabs_default_style();
    tabs_style.height = TAB_BAR_HEIGHT;
    ui_tabs_init(&g_scene_tabs, SCENE_TABS_MAX, &tabs_style);
    ui_tabs_init(&g_camera_tabs, CAMERA_TAB_COUNT, &tabs_style);
    for (int i = 0; i < PANEL_REGION_COUNT; i++) ui_tabs_init(&g_region_tabs[i], PANEL_COUNT, &tabs_style);

    for (int i = 0; i < TOOLBAR_MENU_ITEM_COUNT; i++) {
        g_toolbar_menu_tex[i] = ui_make_text_texture(g_toolbar_menu_labels[i], FONT_WEIGHT_REGULAR, 13,
                                                      &g_toolbar_menu_w[i], &g_toolbar_menu_h[i]);
    }
    g_move_title_tex = ui_make_text_texture("Move panel", FONT_WEIGHT_BOLD, 17,
                                            &g_move_title_w, &g_move_title_h);
    for (int i = 0; i < PANEL_REGION_COUNT; i++) {
        g_move_label_tex[i] = ui_make_text_texture(g_move_region_labels[i], FONT_WEIGHT_REGULAR, 14,
                                                    &g_move_label_w[i], &g_move_label_h[i]);
    }
}

void editor_workspace_set_camera_mode(editor_camera_mode_t mode) {
    g_camera_mode = mode;
    g_camera_tab = (mode == EDITOR_CAMERA_MODE_3D) ? CAMERA_TAB_3D : CAMERA_TAB_2D;
}

static void toolbar_layout(int window_w, int out_x[TOOLBAR_MENU_ITEM_COUNT]) {
    int x = window_w - 16;
    for (int i = 0; i < TOOLBAR_MENU_ITEM_COUNT; i++) {
        x -= g_toolbar_menu_w[i];
        out_x[i] = x;
        x -= 24;
    }
}

static void workspace_host_rect(panel_region_t region, int window_w, int window_h, workspace_rect_t *out) {
    int col_top = TOOLBAR_HEIGHT;
    int col_h = window_h - TOOLBAR_HEIGHT;
    int right_x = window_w - g_right_width;
    switch (region) {
        case PANEL_REGION_LEFT_TOP:
            *out = (workspace_rect_t){0, col_top, g_left_width, g_left_split_y};
            break;
        case PANEL_REGION_RIGHT_TOP:
            *out = (workspace_rect_t){right_x, col_top, g_right_width, g_right_split_y};
            break;
        case PANEL_REGION_LEFT_BOTTOM:
            *out = (workspace_rect_t){0, col_top + g_left_split_y, g_left_width, col_h - g_left_split_y};
            break;
        default:
            *out = (workspace_rect_t){right_x, col_top + g_right_split_y, g_right_width, col_h - g_right_split_y};
            break;
    }
}

static int first_panel_in_region(panel_region_t region) {
    for (int panel = 0; panel < PANEL_COUNT; panel++) if (g_panel_region[panel] == region) return panel;
    return -1;
}

static void build_region_tabs(panel_region_t region) {
    int count = 0;
    int selected = -1;
    for (int panel = 0; panel < PANEL_COUNT; panel++) {
        if (g_panel_region[panel] != region) continue;
        g_region_tab_items[region][count].label = g_panel_labels[panel];
        g_region_tab_items[region][count].closable = 0;
        g_region_tab_panels[region][count] = panel;
        if (g_region_active[region] == panel) selected = count;
        count++;
    }
    if (selected < 0 && count > 0) {
        selected = 0;
        g_region_active[region] = g_region_tab_panels[region][0];
    }
    g_region_tab_count[region] = count;
    ui_tabs_set_items(&g_region_tabs[region], g_region_tab_items[region], count);
    ui_tabs_set_selected(&g_region_tabs[region], selected);
}

static void update_active_panel(editor_panel_t panel, const workspace_rect_t *host) {
    int body_y = host->y + TAB_BAR_HEIGHT;
    int body_h = host->h - TAB_BAR_HEIGHT;
    if (body_h <= 0 || host->w <= 0) return;
    if (panel == PANEL_SCENE) scene_tree_panel_update(host->x, body_y, host->w, body_h);
    else if (panel == PANEL_FILES) file_system_panel_update(host->x, body_y, host->w, body_h);
    else properties_panel_update(host->x, body_y, host->w, body_h);
}

static void draw_active_panel(editor_panel_t panel, const workspace_rect_t *host) {
    int body_y = host->y + TAB_BAR_HEIGHT;
    int body_h = host->h - TAB_BAR_HEIGHT;
    if (body_h <= 0 || host->w <= 0) return;
    if (panel == PANEL_SCENE) scene_tree_panel_draw(host->x, body_y, host->w, body_h);
    else if (panel == PANEL_FILES) file_system_panel_draw(host->x, body_y, host->w, body_h);
    else properties_panel_draw(host->x, body_y, host->w, body_h);
}

static int update_host_region(panel_region_t region, int window_w, int window_h) {
    workspace_rect_t host;
    workspace_host_rect(region, window_w, window_h, &host);
    build_region_tabs(region);
    ui_tabs_update(&g_region_tabs[region], host.x, host.y, host.w);
    int clicked = ui_tabs_clicked_index(&g_region_tabs[region]);
    if (clicked >= 0 && clicked < g_region_tab_count[region]) {
        int panel = g_region_tab_panels[region][clicked];
        if (panel == g_region_active[region]) {
            g_move_panel = panel;
            return 1;
        }
        g_region_active[region] = panel;
    }
    if (g_region_active[region] >= 0) update_active_panel((editor_panel_t)g_region_active[region], &host);
    return 0;
}

static void draw_host_region(panel_region_t region, int window_w, int window_h) {
    workspace_rect_t host;
    workspace_host_rect(region, window_w, window_h, &host);
    ui_layout_draw_panel(host.x, host.y, host.w, host.h, TAB_BAR_HEIGHT);
    build_region_tabs(region);
    ui_tabs_draw(&g_region_tabs[region], host.x, host.y, host.w);
    if (g_region_active[region] >= 0) draw_active_panel((editor_panel_t)g_region_active[region], &host);
}

static void move_panel_to(editor_panel_t panel, panel_region_t target) {
    panel_region_t old = g_panel_region[panel];
    if (old == target) return;
    g_panel_region[panel] = target;
    g_region_active[target] = panel;
    if (g_region_active[old] == (int)panel) g_region_active[old] = first_panel_in_region(old);
}

static void panel_move_choice_rect(panel_region_t region, int dx, int dy, workspace_rect_t *out) {
    /* ست خلايا متساوية: الوسط العلوي والسفلي هما مساحة الـ Viewport. */
    const int cell_w = 112;
    const int cell_h = 56;
    const int gap = 8;
    const int grid_x = dx + 48;
    const int grid_y = dy + 82;
    int column = (region == PANEL_REGION_RIGHT_TOP || region == PANEL_REGION_RIGHT_BOTTOM) ? 2 : 0;
    int row = (region == PANEL_REGION_LEFT_BOTTOM || region == PANEL_REGION_RIGHT_BOTTOM) ? 1 : 0;
    *out = (workspace_rect_t){grid_x + column * (cell_w + gap),
                              grid_y + row * (cell_h + gap), cell_w, cell_h};
}

static void update_move_dialog(int window_w, int window_h) {
    int dx, dy;
    ui_center_rect(window_w, window_h, PANEL_MOVE_WIDTH, PANEL_MOVE_HEIGHT, &dx, &dy);
    int mx = window_mouse_x(), my = window_mouse_y();
    int close_x = dx + PANEL_MOVE_WIDTH - 16 - PANEL_MOVE_CLOSE_SIZE;
    int close_y = dy + (PANEL_MOVE_TITLEBAR - PANEL_MOVE_CLOSE_SIZE) / 2;
    if (window_mouse_left_just_pressed() && ui_dialog_close_button_hit(mx, my, close_x, close_y, PANEL_MOVE_CLOSE_SIZE)) {
        g_move_panel = -1;
        return;
    }
    if (!window_mouse_left_just_pressed()) return;
    for (int region = 0; region < PANEL_REGION_COUNT; region++) {
        workspace_rect_t choice;
        panel_move_choice_rect((panel_region_t)region, dx, dy, &choice);
        if (ui_point_in_rect(mx, my, choice.x, choice.y, choice.w, choice.h) &&
            g_panel_region[g_move_panel] != (panel_region_t)region) {
            move_panel_to((editor_panel_t)g_move_panel, (panel_region_t)region);
            g_move_panel = -1;
            return;
        }
    }
}

static void draw_move_dialog(int window_w, int window_h) {
    if (g_move_panel < 0) return;
    int dx, dy;
    ui_center_rect(window_w, window_h, PANEL_MOVE_WIDTH, PANEL_MOVE_HEIGHT, &dx, &dy);
    ui_dialog_draw_backdrop(window_w, window_h);
    ui_dialog_draw_frame(dx, dy, PANEL_MOVE_WIDTH, PANEL_MOVE_HEIGHT);
    ui_dialog_draw_title_left(g_move_title_tex, g_move_title_w, g_move_title_h,
                              dx, dy, PANEL_MOVE_TITLEBAR, 16);
    ui_dialog_draw_close_button(dx + PANEL_MOVE_WIDTH - 16 - PANEL_MOVE_CLOSE_SIZE,
                                dy + (PANEL_MOVE_TITLEBAR - PANEL_MOVE_CLOSE_SIZE) / 2,
                                PANEL_MOVE_CLOSE_SIZE);

    /* تمثيل الواجهة: ست خلايا متساوية (3×2). الوسط أسود لأنه Viewport. */
    const int cell_w = 112;
    const int cell_h = 56;
    const int gap = 8;
    const int grid_x = dx + 48;
    const int grid_y = dy + 82;
    for (int row = 0; row < 2; row++) {
        for (int column = 0; column < 3; column++) {
            int x = grid_x + column * (cell_w + gap);
            int y = grid_y + row * (cell_h + gap);
            if (column == 1) {
                window_fill_rect(x, y, cell_w, cell_h,
                                 UI_COLOR_BLACK_MUTED.r, UI_COLOR_BLACK_MUTED.g, UI_COLOR_BLACK_MUTED.b);
                continue;
            }
            panel_region_t region = (row == 0)
                ? (column == 0 ? PANEL_REGION_LEFT_TOP : PANEL_REGION_RIGHT_TOP)
                : (column == 0 ? PANEL_REGION_LEFT_BOTTOM : PANEL_REGION_RIGHT_BOTTOM);
            int selected = (g_panel_region[g_move_panel] == region);
            const ui_color_t *color = selected ? &UI_COLOR_BUTTON_BLUE : &UI_COLOR_GRAY_MUTED;
            window_fill_rect(x, y, cell_w, cell_h, color->r, color->g, color->b);
            if (g_move_label_tex[region] != NULL) {
                window_draw_texture(g_move_label_tex[region],
                                    x + (cell_w - g_move_label_w[region]) / 2,
                                    y + (cell_h - g_move_label_h[region]) / 2,
                                    g_move_label_w[region], g_move_label_h[region]);
            }
        }
    }
}

static void build_scene_tabs(void) {
    int count = scene_tabs_count();
    for (int i = 0; i < count && i < SCENE_TABS_MAX; i++) {
        snprintf(g_scene_tab_labels[i], sizeof(g_scene_tab_labels[i]), "%s%s",
                 scene_tabs_get_display_name(i), scene_tabs_get_dirty(i) ? " *" : "");
        g_scene_tab_items[i].label = g_scene_tab_labels[i];
        g_scene_tab_items[i].closable = 1;
    }
    ui_tabs_set_items(&g_scene_tabs, g_scene_tab_items, count);
    ui_tabs_set_selected(&g_scene_tabs, scene_tabs_active_index());
}

static void on_scene_close_save_picked(const char *picked_path, void *user_data) {
    int tab_index = *(int *)user_data;
    if (picked_path != NULL) {
        current_scene_save_as(picked_path);
        scene_tabs_close(tab_index);
    }
    free(user_data);
}

static void apply_close_scene_result(close_scene_result_t result, int tab_index) {
    if (result == CLOSE_SCENE_RESULT_DONT_SAVE) scene_tabs_close(tab_index);
    else if (result == CLOSE_SCENE_RESULT_SAVE) {
        if (current_scene_get_path() != NULL) {
            current_scene_save();
            scene_tabs_close(tab_index);
        } else {
            int *saved_index = malloc(sizeof(*saved_index));
            if (saved_index != NULL) {
                *saved_index = tab_index;
                char name[160];
                snprintf(name, sizeof(name), "%s.rscene", current_scene_get_display_name());
                file_manager_open_save(FILE_MANAGER_ROOT_ASSETS, name, on_scene_close_save_picked, saved_index);
            }
        }
    }
}

static void request_close_scene_tab(int index) {
    if (scene_tabs_get_dirty(index)) {
        scene_tabs_switch_to(index);
        close_scene_dialog_open(index);
    } else {
        scene_tabs_close(index);
    }
}

static void update_splitters(int window_w, int window_h, int mx, int my) {
    int col_top = TOOLBAR_HEIGHT, col_h = window_h - TOOLBAR_HEIGHT;
    int right_x = window_w - g_right_width;
    int center_x = g_left_width, center_w = window_w - g_left_width - g_right_width;
    int left_inner_y = col_top + g_left_split_y;
    int right_inner_y = col_top + g_right_split_y;
    int camera_bottom_y = window_h - g_bottom_center_height;
    if (g_dragging != SPLITTER_NONE) g_hovered_splitter = g_dragging;
    else {
        g_hovered_splitter = SPLITTER_NONE;
        if (ui_point_in_rect(mx, my, g_left_width - SPLITTER_THICKNESS / 2, col_top, SPLITTER_THICKNESS, col_h)) g_hovered_splitter = SPLITTER_LEFT_RIGHT_COL;
        else if (ui_point_in_rect(mx, my, right_x - SPLITTER_THICKNESS / 2, col_top, SPLITTER_THICKNESS, col_h)) g_hovered_splitter = SPLITTER_CENTER_RIGHT_COL;
        else if (ui_point_in_rect(mx, my, 0, left_inner_y - SPLITTER_THICKNESS / 2, g_left_width, SPLITTER_THICKNESS)) g_hovered_splitter = SPLITTER_LEFT_INNER;
        else if (ui_point_in_rect(mx, my, right_x, right_inner_y - SPLITTER_THICKNESS / 2, g_right_width, SPLITTER_THICKNESS)) g_hovered_splitter = SPLITTER_RIGHT_INNER;
        else if (ui_point_in_rect(mx, my, center_x, camera_bottom_y - SPLITTER_THICKNESS / 2, center_w, SPLITTER_THICKNESS)) g_hovered_splitter = SPLITTER_CAMERA_BOTTOM;
    }
    if (g_dragging == SPLITTER_NONE && window_mouse_left_just_pressed()) g_dragging = g_hovered_splitter;
    if (!window_mouse_left_down()) g_dragging = SPLITTER_NONE;
    switch (g_dragging) {
        case SPLITTER_LEFT_RIGHT_COL: {
            int max_w = window_w - g_right_width - CAMERA_MIN_W;
            g_left_width = mx; if (g_left_width < 0) g_left_width = 0; if (g_left_width > max_w) g_left_width = max_w > 0 ? max_w : 0; break;
        }
        case SPLITTER_CENTER_RIGHT_COL: {
            int max_w = window_w - g_left_width - CAMERA_MIN_W;
            g_right_width = window_w - mx; if (g_right_width < 0) g_right_width = 0; if (g_right_width > max_w) g_right_width = max_w > 0 ? max_w : 0; break;
        }
        case SPLITTER_LEFT_INNER: g_left_split_y = my - col_top; if (g_left_split_y < 0) g_left_split_y = 0; if (g_left_split_y > col_h) g_left_split_y = col_h; break;
        case SPLITTER_RIGHT_INNER: g_right_split_y = my - col_top; if (g_right_split_y < 0) g_right_split_y = 0; if (g_right_split_y > col_h) g_right_split_y = col_h; break;
        case SPLITTER_CAMERA_BOTTOM: {
            int max_h = col_h - CAMERA_MIN_H - SCENE_TABS_BAR_HEIGHT - CAMERA_TOP_BAR_HEIGHT;
            g_bottom_center_height = window_h - my; if (g_bottom_center_height < 0) g_bottom_center_height = 0;
            if (g_bottom_center_height > max_h) g_bottom_center_height = max_h > 0 ? max_h : 0;
            break;
        }
        default: break;
    }
}

void editor_workspace_update(int window_w, int window_h) {
    static int scene_system_initialized = 0;
    ensure_ui_ready();
    if (!scene_system_initialized) { scene_system_initialized = 1; scene_tabs_init(); }

    if (close_scene_dialog_is_open()) {
        close_scene_dialog_update(window_w, window_h);
        int index = close_scene_dialog_get_tab_index();
        apply_close_scene_result(close_scene_dialog_consume_result(), index);
        return;
    }
    if (export_dialog_is_open()) { export_dialog_update(window_w, window_h); return; }
    if (g_move_panel >= 0) { update_move_dialog(window_w, window_h); return; }
    if (add_node_dialog_is_open()) { add_node_dialog_update(window_w, window_h); return; }

    int mx = window_mouse_x(), my = window_mouse_y();
    int toolbar_x[TOOLBAR_MENU_ITEM_COUNT];
    toolbar_layout(window_w, toolbar_x);
    g_toolbar_menu_hovered = -1;
    for (int i = 0; i < TOOLBAR_MENU_ITEM_COUNT; i++) {
        int y = (TOOLBAR_HEIGHT - g_toolbar_menu_h[i]) / 2;
        if (ui_point_in_rect(mx, my, toolbar_x[i], y, g_toolbar_menu_w[i], g_toolbar_menu_h[i])) {
            g_toolbar_menu_hovered = i;
            if (i == 0 && window_mouse_left_just_pressed()) { export_dialog_open(); return; }
        }
    }

    int center_x = g_left_width, center_w = window_w - g_left_width - g_right_width;
    build_scene_tabs();
    ui_tabs_update(&g_scene_tabs, center_x, TOOLBAR_HEIGHT, center_w - SCENE_TAB_PLUS_SIZE - SCENE_TAB_PLUS_MARGIN * 2);
    int close_tab = ui_tabs_close_clicked_index(&g_scene_tabs);
    if (close_tab >= 0) { request_close_scene_tab(close_tab); return; }
    int clicked_tab = ui_tabs_clicked_index(&g_scene_tabs);
    if (clicked_tab >= 0 && clicked_tab != scene_tabs_active_index()) scene_tabs_switch_to(clicked_tab);
    int plus_x = center_x + center_w - SCENE_TAB_PLUS_MARGIN - SCENE_TAB_PLUS_SIZE;
    if (ui_point_in_rect(mx, my, plus_x - 3, TOOLBAR_HEIGHT + 3, SCENE_TAB_PLUS_SIZE + 6, SCENE_TAB_PLUS_SIZE + 6) &&
        window_mouse_left_just_pressed()) scene_tabs_new();

    if (g_camera_tab != CAMERA_TAB_SCRIPT) code_editor_set_active(0);
    if (update_host_region(PANEL_REGION_LEFT_TOP, window_w, window_h)) return;
    if (update_host_region(PANEL_REGION_RIGHT_TOP, window_w, window_h)) return;
    if (update_host_region(PANEL_REGION_LEFT_BOTTOM, window_w, window_h)) return;
    if (update_host_region(PANEL_REGION_RIGHT_BOTTOM, window_w, window_h)) return;

    int camera_bar_y = TOOLBAR_HEIGHT + SCENE_TABS_BAR_HEIGHT;
    ui_tabs_set_items(&g_camera_tabs, g_camera_tab_items, CAMERA_TAB_COUNT);
    ui_tabs_set_selected(&g_camera_tabs, g_camera_tab);
    ui_tabs_update(&g_camera_tabs, center_x, camera_bar_y, center_w);
    int camera_clicked = ui_tabs_clicked_index(&g_camera_tabs);
    if (camera_clicked >= 0) {
        g_camera_tab = (camera_tab_t)camera_clicked;
            if (g_camera_tab == CAMERA_TAB_3D) g_camera_mode = EDITOR_CAMERA_MODE_3D;
        else if (g_camera_tab == CAMERA_TAB_2D) g_camera_mode = EDITOR_CAMERA_MODE_2D;
        else code_editor_set_active(1);
    }

    int col_h = window_h - TOOLBAR_HEIGHT;
    int camera_h = col_h - g_bottom_center_height;
    int viewport_y = TOOLBAR_HEIGHT + SCENE_TABS_BAR_HEIGHT + CAMERA_TOP_BAR_HEIGHT;
    int viewport_h = camera_h - SCENE_TABS_BAR_HEIGHT - CAMERA_TOP_BAR_HEIGHT;
    if (g_camera_tab == CAMERA_TAB_SCRIPT) {
        code_editor_set_active(1);
        code_editor_update(center_x, viewport_y, center_w, viewport_h);
    } else {
        if (g_camera_mode == EDITOR_CAMERA_MODE_3D) viewport_3d_update(center_x, viewport_y, center_w, viewport_h);
        else viewport_2d_update(center_x, viewport_y, center_w, viewport_h);
    }
    update_splitters(window_w, window_h, mx, my);
}

static ui_splitter_visual_t splitter_visual(splitter_id_t id) {
    if (g_dragging == id) return UI_SPLITTER_ACTIVE;
    if (g_hovered_splitter == id) return UI_SPLITTER_HOVER;
    return UI_SPLITTER_IDLE;
}

void editor_workspace_draw(int window_w, int window_h) {
    ensure_ui_ready();
    int col_top = TOOLBAR_HEIGHT, col_h = window_h - TOOLBAR_HEIGHT;
    int right_x = window_w - g_right_width;
    int center_x = g_left_width, center_w = window_w - g_left_width - g_right_width;
    int camera_h = col_h - g_bottom_center_height;

    window_fill_rect(0, 0, window_w, TOOLBAR_HEIGHT, UI_COLOR_BLACK_MUTED.r, UI_COLOR_BLACK_MUTED.g, UI_COLOR_BLACK_MUTED.b);
    int toolbar_x[TOOLBAR_MENU_ITEM_COUNT];
    toolbar_layout(window_w, toolbar_x);
    for (int i = 0; i < TOOLBAR_MENU_ITEM_COUNT; i++) if (g_toolbar_menu_tex[i] != NULL) {
        unsigned char tint = (i == g_toolbar_menu_hovered) ? 0xF2 : 0x9A;
        window_draw_texture_tinted(g_toolbar_menu_tex[i], toolbar_x[i],
                                   (TOOLBAR_HEIGHT - g_toolbar_menu_h[i]) / 2,
                                   g_toolbar_menu_w[i], g_toolbar_menu_h[i], tint, tint, tint);
    }

    draw_host_region(PANEL_REGION_LEFT_TOP, window_w, window_h);
    draw_host_region(PANEL_REGION_RIGHT_TOP, window_w, window_h);
    draw_host_region(PANEL_REGION_LEFT_BOTTOM, window_w, window_h);
    draw_host_region(PANEL_REGION_RIGHT_BOTTOM, window_w, window_h);
    ui_layout_draw_panel(center_x, col_top + camera_h, center_w, g_bottom_center_height, TAB_BAR_HEIGHT);

    int scene_tabs_y = col_top;
    int camera_bar_y = scene_tabs_y + SCENE_TABS_BAR_HEIGHT;
    ui_tabs_draw(&g_scene_tabs, center_x, scene_tabs_y, center_w - SCENE_TAB_PLUS_SIZE - SCENE_TAB_PLUS_MARGIN * 2);
    int plus_x = center_x + center_w - SCENE_TAB_PLUS_MARGIN - SCENE_TAB_PLUS_SIZE;
    icon_atlas_draw(ICON_add, plus_x, scene_tabs_y + (SCENE_TABS_BAR_HEIGHT - SCENE_TAB_PLUS_SIZE) / 2, SCENE_TAB_PLUS_SIZE);
    window_fill_rect(center_x, camera_bar_y, center_w, CAMERA_TOP_BAR_HEIGHT,
                     UI_COLOR_BLACK_MUTED.r, UI_COLOR_BLACK_MUTED.g, UI_COLOR_BLACK_MUTED.b);
    ui_tabs_draw(&g_camera_tabs, center_x, camera_bar_y, center_w);

    int viewport_y = camera_bar_y + CAMERA_TOP_BAR_HEIGHT;
    int viewport_h = camera_h - SCENE_TABS_BAR_HEIGHT - CAMERA_TOP_BAR_HEIGHT;
    if (g_camera_tab == CAMERA_TAB_SCRIPT) code_editor_draw(center_x, viewport_y, center_w, viewport_h);
    else if (g_camera_mode == EDITOR_CAMERA_MODE_3D) viewport_3d_draw(center_x, viewport_y, center_w, viewport_h);
    else viewport_2d_draw(center_x, viewport_y, center_w, viewport_h);

    ui_layout_draw_splitter(g_left_width - SPLITTER_THICKNESS / 2, col_top, SPLITTER_THICKNESS, col_h, splitter_visual(SPLITTER_LEFT_RIGHT_COL));
    ui_layout_draw_splitter(right_x - SPLITTER_THICKNESS / 2, col_top, SPLITTER_THICKNESS, col_h, splitter_visual(SPLITTER_CENTER_RIGHT_COL));
    ui_layout_draw_splitter(0, col_top + g_left_split_y - SPLITTER_THICKNESS / 2, g_left_width, SPLITTER_THICKNESS, splitter_visual(SPLITTER_LEFT_INNER));
    ui_layout_draw_splitter(right_x, col_top + g_right_split_y - SPLITTER_THICKNESS / 2, g_right_width, SPLITTER_THICKNESS, splitter_visual(SPLITTER_RIGHT_INNER));
    ui_layout_draw_splitter(center_x, col_top + camera_h - SPLITTER_THICKNESS / 2, center_w, SPLITTER_THICKNESS, splitter_visual(SPLITTER_CAMERA_BOTTOM));

    add_node_dialog_draw(window_w, window_h);
    close_scene_dialog_draw(window_w, window_h);
    export_dialog_draw(window_w, window_h);
    draw_move_dialog(window_w, window_h);
}

void editor_workspace_shutdown(void) {
    for (int i = 0; i < TOOLBAR_MENU_ITEM_COUNT; i++) if (g_toolbar_menu_tex[i] != NULL) window_destroy_texture(g_toolbar_menu_tex[i]);
    if (g_move_title_tex != NULL) window_destroy_texture(g_move_title_tex);
    for (int i = 0; i < PANEL_REGION_COUNT; i++) if (g_move_label_tex[i] != NULL) window_destroy_texture(g_move_label_tex[i]);
    ui_tabs_shutdown(&g_scene_tabs);
    ui_tabs_shutdown(&g_camera_tabs);
    for (int i = 0; i < PANEL_REGION_COUNT; i++) ui_tabs_shutdown(&g_region_tabs[i]);
    scene_tabs_shutdown();
    close_scene_dialog_shutdown();
    export_dialog_shutdown();
    code_editor_shutdown();
    scene_tree_panel_shutdown();
    scene_data_shutdown();
    file_system_panel_shutdown();
    properties_panel_shutdown();
    add_node_dialog_shutdown();
    g_ui_ready = 0;
}
