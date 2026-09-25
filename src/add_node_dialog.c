/*
 * ============================================================
 * add_node_dialog.c
 * ============================================================
 */

#include <string.h>
#include <stddef.h>

#include "add_node_dialog.h"
#include "scene_tree_panel.h"
#include "window.h"
#include "labeled_button.h"
#include "shape_provider.h"
#include "icon_atlas.h"
#include "node_registry.h"
#include "text_field.h"
#include "font.h"
#include "ui_project_center.h"

#define DIALOG_WIDTH     320
#define DIALOG_HEIGHT    280
#define DIALOG_PADDING    16
#define TITLE_HEIGHT       28
#define SEARCH_HEIGHT      22
#define SEARCH_ICON_GAP     6
#define GAP                8
#define ROW_HEIGHT         24
#define ICON_SIZE          16
#define MAX_NODE_TYPES     32

static int g_is_open = 0;
static int g_hovered_index = -1;
static int g_selected_index = 0;

static text_field_t g_search_field;

static labeled_button_t  g_cancel_btn, g_add_btn;
static window_texture_t *g_cancel_btn_tex, *g_cancel_txt_tex;
static window_texture_t *g_add_btn_tex, *g_add_txt_tex;
static window_texture_t *g_title_tex = NULL;
static int g_title_w = 0, g_title_h = 0;
static window_texture_t *g_hover_tex = NULL;
static window_texture_t *g_selected_tex = NULL;

/* أسماء أنواع العقد مرسومة مسبقاً مرة واحدة بس - تُقرأ من
 * node_registry تلقائياً، بدون قائمة يدوية ثابتة */
static window_texture_t *g_name_tex[MAX_NODE_TYPES];
static int g_name_w[MAX_NODE_TYPES], g_name_h[MAX_NODE_TYPES];

static int g_ready = 0;

static void dim_alpha(shape_image_t *img, double factor) {
    if (img->pixels == NULL) return;
    int count = img->width * img->height;
    for (int i = 0; i < count; i++) {
        unsigned char *a = &img->pixels[i * 4 + 3];
        *a = (unsigned char)((double)(*a) * factor);
    }
}

static void ensure_ready(void) {
    if (g_ready) return;
    g_ready = 1;

    text_field_init(&g_search_field);

    font_text_image_t title = font_render_text("Add Node", FONT_WEIGHT_BOLD, 17);
    if (title.pixels != NULL) {
        g_title_tex = window_create_texture(title.pixels, title.width, title.height);
        g_title_w = title.width;
        g_title_h = title.height;
        font_free_text_image(&title);
    }

    g_cancel_btn = labeled_button_create("Cancel", FONT_WEIGHT_REGULAR, 15, 14, 6, 3,
                                          UI_COLOR_BUTTON_BLUE.r, UI_COLOR_BUTTON_BLUE.g, UI_COLOR_BUTTON_BLUE.b);
    g_cancel_btn_tex = window_create_texture(g_cancel_btn.shape_img.pixels, g_cancel_btn.shape_img.width, g_cancel_btn.shape_img.height);
    g_cancel_txt_tex = window_create_texture(g_cancel_btn.text_img.pixels, g_cancel_btn.text_img.width, g_cancel_btn.text_img.height);

    g_add_btn = labeled_button_create("Add", FONT_WEIGHT_REGULAR, 15, 14, 6, 3,
                                       UI_COLOR_BUTTON_BLUE.r, UI_COLOR_BUTTON_BLUE.g, UI_COLOR_BUTTON_BLUE.b);
    g_add_btn_tex = window_create_texture(g_add_btn.shape_img.pixels, g_add_btn.shape_img.width, g_add_btn.shape_img.height);
    g_add_txt_tex = window_create_texture(g_add_btn.text_img.pixels, g_add_btn.text_img.width, g_add_btn.text_img.height);

    icon_atlas_init();

    int count = node_registry_count();
    for (int i = 0; i < count && i < MAX_NODE_TYPES; i++) {
        const node_registry_entry_t *entry = node_registry_get_by_index(i);
        font_text_image_t txt = font_render_text(entry->name, FONT_WEIGHT_REGULAR, 15);
        g_name_tex[i] = (txt.pixels != NULL) ? window_create_texture(txt.pixels, txt.width, txt.height) : NULL;
        g_name_w[i] = txt.width;
        g_name_h[i] = txt.height;
        font_free_text_image(&txt);
    }

    int list_w = DIALOG_WIDTH - DIALOG_PADDING * 2;
    shape_image_t hov = shape_provider_render_rect(list_w, ROW_HEIGHT, 8,
        UI_COLOR_BUTTON_BLUE.r, UI_COLOR_BUTTON_BLUE.g, UI_COLOR_BUTTON_BLUE.b);
    dim_alpha(&hov, 0.25);
    g_hover_tex = window_create_texture(hov.pixels, hov.width, hov.height);
    shape_provider_free_image(&hov);

    shape_image_t sel = shape_provider_render_rect(list_w, ROW_HEIGHT, 8,
        UI_COLOR_BUTTON_BLUE.r, UI_COLOR_BUTTON_BLUE.g, UI_COLOR_BUTTON_BLUE.b);
    dim_alpha(&sel, 0.55);
    g_selected_tex = window_create_texture(sel.pixels, sel.width, sel.height);
    shape_provider_free_image(&sel);
}

static int matches_search(const char *name) {
    if (g_search_field.text[0] == '\0') {
        return 1;
    }
    return strstr(name, g_search_field.text) != NULL;
}

void add_node_dialog_open(void) {
    ensure_ready();
    g_is_open = 1;
    g_selected_index = 0;
    g_search_field.text[0] = '\0';
    g_search_field.cursor_pos = 0;
    g_search_field.scroll_offset = 0;
}

int add_node_dialog_is_open(void) { return g_is_open; }

void add_node_dialog_update(int window_w, int window_h) {
    if (!g_is_open) return;

    int dx = (window_w - DIALOG_WIDTH) / 2;
    int dy = (window_h - DIALOG_HEIGHT) / 2;

    int search_y = dy + DIALOG_PADDING + TITLE_HEIGHT;
    int list_x   = dx + DIALOG_PADDING;
    int list_y   = search_y + SEARCH_HEIGHT + GAP;
    int list_w   = DIALOG_WIDTH - DIALOG_PADDING * 2;

    int search_field_x = list_x + ICON_SIZE + SEARCH_ICON_GAP;
    int search_field_w = list_w - ICON_SIZE - SEARCH_ICON_GAP;
    text_field_update(&g_search_field, search_field_x, search_y, search_field_w, SEARCH_HEIGHT, search_field_w);

    int mx = window_mouse_x(), my = window_mouse_y();
    int count = node_registry_count();

    g_hovered_index = -1;
    int visible_row = 0;
    for (int i = 0; i < count; i++) {
        const node_registry_entry_t *entry = node_registry_get_by_index(i);
        if (!matches_search(entry->name)) continue;
        int row_y = list_y + visible_row * ROW_HEIGHT;
        if (mx >= list_x && mx < list_x + list_w && my >= row_y && my < row_y + ROW_HEIGHT) {
            g_hovered_index = i;
            if (window_mouse_left_just_pressed()) {
                g_selected_index = i;
            }
        }
        visible_row++;
    }

    int buttons_y = dy + DIALOG_HEIGHT - DIALOG_PADDING - g_cancel_btn.height;
    int add_x    = dx + DIALOG_WIDTH - DIALOG_PADDING - g_add_btn.width;
    int cancel_x = add_x - 8 - g_cancel_btn.width;

    if (window_mouse_left_just_pressed()) {
        int inside_dialog = (mx >= dx && mx < dx + DIALOG_WIDTH && my >= dy && my < dy + DIALOG_HEIGHT);
        if (mx >= cancel_x && mx < cancel_x + g_cancel_btn.width
            && my >= buttons_y && my < buttons_y + g_cancel_btn.height) {
            g_is_open = 0;
            window_stop_text_input();
        } else if (mx >= add_x && mx < add_x + g_add_btn.width
                   && my >= buttons_y && my < buttons_y + g_add_btn.height) {
            const node_registry_entry_t *entry = node_registry_get_by_index(g_selected_index);
            if (entry != NULL) {
                scene_tree_panel_add_node(entry->type, entry->name);
            }
            g_is_open = 0;
            window_stop_text_input();
        } else if (!inside_dialog && !g_search_field.focused) {
            /* لا شيء - نترك النافذة مفتوحة */
        }
    }

    if (!g_search_field.focused) {
        window_stop_text_input();
    } else {
        window_start_text_input();
    }
}

void add_node_dialog_draw(int window_w, int window_h) {
    if (!g_is_open) return;

    int dx = (window_w - DIALOG_WIDTH) / 2;
    int dy = (window_h - DIALOG_HEIGHT) / 2;

    window_fill_rect(dx, dy, DIALOG_WIDTH, DIALOG_HEIGHT,
                      UI_COLOR_BLACK_MUTED.r, UI_COLOR_BLACK_MUTED.g, UI_COLOR_BLACK_MUTED.b);

    if (g_title_tex != NULL) {
        int title_x = dx + (DIALOG_WIDTH - g_title_w) / 2;
        int title_y = dy + DIALOG_PADDING;
        window_draw_texture(g_title_tex, title_x, title_y, g_title_w, g_title_h);
    }

    int search_y = dy + DIALOG_PADDING + TITLE_HEIGHT;
    int list_x   = dx + DIALOG_PADDING;
    int list_y   = search_y + SEARCH_HEIGHT + GAP;
    int list_w   = DIALOG_WIDTH - DIALOG_PADDING * 2;

    int search_field_x = list_x + ICON_SIZE + SEARCH_ICON_GAP;
    int search_field_w = list_w - ICON_SIZE - SEARCH_ICON_GAP;
    icon_atlas_draw(ICON_search, list_x, search_y + (SEARCH_HEIGHT - ICON_SIZE) / 2, ICON_SIZE);
    text_field_draw(&g_search_field, search_field_x, search_y, search_field_w, SEARCH_HEIGHT, 13);

    int count = node_registry_count();
    int visible_row = 0;
    for (int i = 0; i < count; i++) {
        const node_registry_entry_t *entry = node_registry_get_by_index(i);
        if (!matches_search(entry->name)) continue;
        int row_y = list_y + visible_row * ROW_HEIGHT;

        if (i == g_selected_index && g_selected_tex != NULL) {
            window_draw_texture(g_selected_tex, list_x, row_y, list_w, ROW_HEIGHT);
        } else if (i == g_hovered_index && g_hover_tex != NULL) {
            window_draw_texture(g_hover_tex, list_x, row_y, list_w, ROW_HEIGHT);
        }

        int icon_x = list_x + 6;
        int icon_y = row_y + (ROW_HEIGHT - ICON_SIZE) / 2;
        icon_atlas_draw(entry->icon_id, icon_x, icon_y, ICON_SIZE);

        if (g_name_tex[i] != NULL) {
            int name_x = icon_x + ICON_SIZE + 6;
            window_draw_texture(g_name_tex[i], name_x, row_y + (ROW_HEIGHT - g_name_h[i]) / 2,
                                 g_name_w[i], g_name_h[i]);
        }

        visible_row++;
    }

    int buttons_y = dy + DIALOG_HEIGHT - DIALOG_PADDING - g_cancel_btn.height;
    int add_x    = dx + DIALOG_WIDTH - DIALOG_PADDING - g_add_btn.width;
    int cancel_x = add_x - 8 - g_cancel_btn.width;

    window_draw_texture(g_cancel_btn_tex, cancel_x, buttons_y, g_cancel_btn.width, g_cancel_btn.height);
    window_draw_texture(g_cancel_txt_tex, cancel_x + g_cancel_btn.text_offset_x, buttons_y + g_cancel_btn.text_offset_y,
                         g_cancel_btn.text_img.width, g_cancel_btn.text_img.height);

    window_draw_texture(g_add_btn_tex, add_x, buttons_y, g_add_btn.width, g_add_btn.height);
    window_draw_texture(g_add_txt_tex, add_x + g_add_btn.text_offset_x, buttons_y + g_add_btn.text_offset_y,
                         g_add_btn.text_img.width, g_add_btn.text_img.height);
}

void add_node_dialog_shutdown(void) {
    if (g_title_tex) window_destroy_texture(g_title_tex);
    if (g_cancel_btn_tex) window_destroy_texture(g_cancel_btn_tex);
    if (g_cancel_txt_tex) window_destroy_texture(g_cancel_txt_tex);
    if (g_add_btn_tex) window_destroy_texture(g_add_btn_tex);
    if (g_add_txt_tex) window_destroy_texture(g_add_txt_tex);
    if (g_hover_tex) window_destroy_texture(g_hover_tex);
    if (g_selected_tex) window_destroy_texture(g_selected_tex);
    for (int i = 0; i < MAX_NODE_TYPES; i++) {
        if (g_name_tex[i]) window_destroy_texture(g_name_tex[i]);
    }
    text_field_free(&g_search_field);
    labeled_button_free(&g_cancel_btn);
    labeled_button_free(&g_add_btn);
    g_is_open = 0;
    g_ready = 0;
}
