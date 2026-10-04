/*
 * ============================================================
 * properties_panel.c
 * ============================================================
 * Values displayed and edited here are live - each edit field is directly
 * bound to the actual node instance memory in the scene tree (via
 * scene_data_get_values), with no intermediate display copy. Edits here
 * immediately affect the same data the viewport renders from.
 *
 * A STRING property marked is_asset_path=1 (like "Image Path" in Sprite2D)
 * is shown as a "Browse" button instead of a free text field - it opens
 * the file_manager scoped to the project's Assets:// and writes the
 * selected path directly into the real node memory on confirmation.
 * ============================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>

#include "properties_panel.h"
#include "scene_data.h"
#include "node_registry.h"
#include "window.h"
#include "font.h"
#include "text_field.h"
#include "file_manager.h"
#include "current_scene.h"
#include "ui_theme.h"
#include "ui_scrollbar.h"
#include "path_utils.h"

#define ROW_HEIGHT      22
#define HEADER_HEIGHT   28
#define ROW_PAD          8
#define FIELD_WIDTH     90 /* Display the value field/button, aligned to the right edge */
#define FIELD_VPAD       3 /* Vertical margin between the field and its row bounds */

/* Accepted extensions for any is_asset_path property currently - the same formats that the real src/nodes/sprite2d.c can actually load (via the shared image_loader.h - see that file's comment): PNG/JPEG/BMP/TGA/TIFF via gsKit/stb_image directly, and RAW/TIM2/TIM via our own decoder - intentionally excluding GIF because it's not supported at all (naming trap, see previous discussion) */
static const char *g_image_extensions[] = { "png", "jpg", "jpeg", "bmp", "tga", "tif", "tiff", "raw", "tm2", "tim2", "tim" };
#define IMAGE_EXTENSION_COUNT 11

typedef struct {
    window_texture_t *name_tex;
    int name_w, name_h;
    text_field_t field;
    node_property_type_t type;
    int is_asset_path;
    int hovered; /* For the Browse button only - hover visual feedback */

    /* Text for the Browse button (short file name or "Browse...") - built once in rebuild_rows, not every draw frame (unlike creating it during rendering, which would create a new texture every frame unnecessarily) */
    window_texture_t *value_tex;
    int value_w, value_h;
} property_row_t;

/* Small context passed to file_manager_open when the Browse button is pressed - allocated on press and freed inside on_asset_picked regardless of the result (real selection or cancel) */
typedef struct {
    int node_index;
    int property_index;
} asset_pick_context_t;

static int g_selected_node_index = -1;

static window_texture_t *g_header_tex = NULL;
static int g_header_w = 0, g_header_h = 0;

static property_row_t g_rows[32];
static int g_row_count = 0;
static int g_scroll_offset = 0;
static int g_scroll_dragging = 0;

static void free_rows(void) {
    for (int i = 0; i < g_row_count; i++) {
        if (g_rows[i].name_tex) window_destroy_texture(g_rows[i].name_tex);
        if (g_rows[i].value_tex) window_destroy_texture(g_rows[i].value_tex);
        text_field_free(&g_rows[i].field);
    }
    g_row_count = 0;
    g_scroll_offset = 0;
}

/* Rebuilds all property rows (label + edit field/browse button, initialized with the actual
 * current value) - called when the selection changes, or after choosing a new file via the
 * browse button (to update the button text immediately) */
static void rebuild_rows(void) {
    free_rows();

    if (g_header_tex) {
        window_destroy_texture(g_header_tex);
        g_header_tex = NULL;
    }

    if (g_selected_node_index < 0) return;

    node_type_t type = scene_data_get_type(g_selected_node_index);
    const char *node_name = scene_data_get_name(g_selected_node_index);
    const node_registry_entry_t *entry = node_registry_get(type);
    node_property_value_t *values = scene_data_get_values(g_selected_node_index);

    char header_text[96];
    snprintf(header_text, sizeof(header_text), "%s (%s)",
             node_name ? node_name : "?", entry ? entry->name : "?");

    font_text_image_t header = font_render_text(header_text, FONT_WEIGHT_BOLD, 15);
    if (header.pixels != NULL) {
        g_header_tex = window_create_texture(header.pixels, header.width, header.height);
        g_header_w = header.width;
        g_header_h = header.height;
        font_free_text_image(&header);
    }

    if (entry == NULL || entry->property_count <= 0 || entry->properties == NULL || values == NULL) return;

    for (int i = 0; i < entry->property_count && i < 32; i++) {
        const node_property_t *prop = &entry->properties[i];
        property_row_t *row = &g_rows[g_row_count];

        font_text_image_t name_txt = font_render_text(prop->name, FONT_WEIGHT_REGULAR, 13);
        row->name_tex = (name_txt.pixels != NULL) ? window_create_texture(name_txt.pixels, name_txt.width, name_txt.height) : NULL;
        row->name_w = name_txt.width;
        row->name_h = name_txt.height;
        font_free_text_image(&name_txt);

        row->type = prop->type;
        row->is_asset_path = prop->is_asset_path;
        row->hovered = 0;
        row->value_tex = NULL;
        row->value_w = row->value_h = 0;

        text_field_init(&row->field);

        /* The initial value = the node's exact current real value, not the type-scheme default. For an
         * is_asset_path property we store only the short filename in the same field (for display on the button) -
         * the real full path remains in values[i].s alone; this field is just a visual display, not the source of truth */
        switch (prop->type) {
            case NODE_PROPERTY_TYPE_FLOAT:
                snprintf(row->field.text, TEXT_FIELD_MAX_LEN, "%.3f", values[i].f);
                break;
            case NODE_PROPERTY_TYPE_INT:
                snprintf(row->field.text, TEXT_FIELD_MAX_LEN, "%d", values[i].i);
                break;
            case NODE_PROPERTY_TYPE_STRING:
                if (row->is_asset_path) {
                    const char *path = values[i].s;
                    snprintf(row->field.text, TEXT_FIELD_MAX_LEN, "%s",
                             (path != NULL && path[0] != '\0') ? path_utils_basename(path) : "Browse...");
                } else {
                    snprintf(row->field.text, TEXT_FIELD_MAX_LEN, "%s", values[i].s ? values[i].s : "");
                }
                break;
        }
        row->field.cursor_pos = (int)strlen(row->field.text);

        /* Button text texture - built only once here for is_asset_path buttons
         * (unlike regular text fields where the text_field draws itself from its live text each frame) */
        if (row->is_asset_path) {
            font_text_image_t value_txt = font_render_text(row->field.text, FONT_WEIGHT_REGULAR, 12);
            if (value_txt.pixels != NULL) {
                row->value_tex = window_create_texture(value_txt.pixels, value_txt.width, value_txt.height);
                row->value_w = value_txt.width;
                row->value_h = value_txt.height;
                font_free_text_image(&value_txt);
            }
        }

        g_row_count++;
    }
}

void properties_panel_set_selected(int node_index) {
    g_selected_node_index = node_index;
    rebuild_rows();
}

void properties_panel_clear_selection(void) {
    g_selected_node_index = -1;
    g_scroll_offset = 0;
    g_scroll_dragging = 0;
    free_rows();
    if (g_header_tex) {
        window_destroy_texture(g_header_tex);
        g_header_tex = NULL;
    }
}

/* File selection callback from file_manager - called once when the window is closed (either with an actual selection or cancel) */
static void on_asset_picked(const char *picked_path, void *user_data) {
    asset_pick_context_t *ctx = (asset_pick_context_t *)user_data;

    if (picked_path != NULL && ctx->node_index == g_selected_node_index) {
        node_property_value_t *values = scene_data_get_values(ctx->node_index);
        if (values != NULL) {
            free(values[ctx->property_index].s);
            values[ctx->property_index].s = strdup(picked_path);
            current_scene_mark_dirty();
            rebuild_rows(); /* Updates the button text to immediately show the new filename */
        }
    }

    free(ctx);
}

void properties_panel_update(int x, int y, int w, int h) {
    (void)h;
    if (g_selected_node_index < 0) return;

    node_property_value_t *values = scene_data_get_values(g_selected_node_index);
    if (values == NULL) return;

    int mx = window_mouse_x();
    int my = window_mouse_y();
    int viewport_y = y + HEADER_HEIGHT;
    int viewport_h = h - HEADER_HEIGHT;
    if (viewport_h < ROW_HEIGHT) viewport_h = ROW_HEIGHT;
    int content_h = g_row_count * ROW_HEIGHT;
    ui_scrollbar_update(&g_scroll_offset, &g_scroll_dragging, x, w, viewport_y, viewport_h, content_h, ROW_HEIGHT);

    int any_field_focused = 0;
    int list_y = y + HEADER_HEIGHT;
    for (int i = 0; i < g_row_count; i++) {
        int row_y = list_y + i * ROW_HEIGHT - g_scroll_offset;
        if (row_y + ROW_HEIGHT < list_y || row_y >= y + h) continue;
        int field_w = FIELD_WIDTH;
        int field_h = ROW_HEIGHT - FIELD_VPAD * 2;
        int field_x = x + w - ROW_PAD - field_w;
        int field_y = row_y + FIELD_VPAD;

        if (g_rows[i].is_asset_path) {
            /* Browse button - no free text entry, just hover/click */
            g_rows[i].hovered = (mx >= field_x && mx < field_x + field_w &&
                                 my >= field_y && my < field_y + field_h);

            if (g_rows[i].hovered && window_mouse_left_just_pressed() && !file_manager_is_open()) {
                asset_pick_context_t *ctx = malloc(sizeof(asset_pick_context_t));
                ctx->node_index = g_selected_node_index;
                ctx->property_index = i;
                file_manager_open(FILE_MANAGER_ROOT_ASSETS, FILE_MANAGER_MODE_PICK_FILE,
                                    g_image_extensions, IMAGE_EXTENSION_COUNT,
                                    on_asset_picked, ctx);
            }
            continue; /* No text_field_update or manual value entry - the value is written only from on_asset_picked */
        }

        char previous_text[TEXT_FIELD_MAX_LEN];
        strncpy(previous_text, g_rows[i].field.text, sizeof(previous_text) - 1);
        previous_text[sizeof(previous_text) - 1] = '\0';
        text_field_update(&g_rows[i].field, field_x, field_y, field_w, field_h, field_w);
        int text_changed = strcmp(previous_text, g_rows[i].field.text) != 0;
        if (g_rows[i].field.focused) any_field_focused = 1;

        if (!g_rows[i].field.focused && !text_changed) {
            switch (g_rows[i].type) {
                case NODE_PROPERTY_TYPE_FLOAT:
                    snprintf(g_rows[i].field.text, TEXT_FIELD_MAX_LEN, "%.3f", values[i].f);
                    break;
                case NODE_PROPERTY_TYPE_INT:
                    snprintf(g_rows[i].field.text, TEXT_FIELD_MAX_LEN, "%d", values[i].i);
                    break;
                case NODE_PROPERTY_TYPE_STRING:
                    snprintf(g_rows[i].field.text, TEXT_FIELD_MAX_LEN, "%s", values[i].s ? values[i].s : "");
                    break;
            }
            g_rows[i].field.cursor_pos = (int)strlen(g_rows[i].field.text);
            g_rows[i].field.scroll_offset = 0;
        }

        if (!text_changed) continue;

        switch (g_rows[i].type) {
            case NODE_PROPERTY_TYPE_FLOAT: {
                float new_val = (float)atof(g_rows[i].field.text);
                if (new_val != values[i].f) {
                    values[i].f = new_val;
                    current_scene_mark_dirty();
                }
                break;
            }
            case NODE_PROPERTY_TYPE_INT: {
                int new_val = atoi(g_rows[i].field.text);
                if (new_val != values[i].i) {
                    values[i].i = new_val;
                    current_scene_mark_dirty();
                }
                break;
            }
            case NODE_PROPERTY_TYPE_STRING:
                if (strcmp(g_rows[i].field.text, values[i].s ? values[i].s : "") != 0) {
                    free(values[i].s);
                    values[i].s = strdup(g_rows[i].field.text);
                    current_scene_mark_dirty();
                }
                break;
        }
    }
    if (any_field_focused) window_start_text_input();
    else window_stop_text_input();
}

void properties_panel_draw(int x, int y, int w, int h) {
    (void)h;

    if (g_selected_node_index < 0) return;

    if (g_header_tex != NULL) {
        window_draw_texture(g_header_tex, x + ROW_PAD, y + (HEADER_HEIGHT - g_header_h) / 2, g_header_w, g_header_h);
    }

    int list_y = y + HEADER_HEIGHT;
    for (int i = 0; i < g_row_count; i++) {
        int row_y = list_y + i * ROW_HEIGHT - g_scroll_offset;
        if (row_y + ROW_HEIGHT < list_y || row_y >= y + h) continue;

        if (g_rows[i].name_tex != NULL) {
            window_draw_texture(g_rows[i].name_tex, x + ROW_PAD, row_y + (ROW_HEIGHT - g_rows[i].name_h) / 2,
                                 g_rows[i].name_w, g_rows[i].name_h);
        }

        int field_w = FIELD_WIDTH;
        int field_h = ROW_HEIGHT - FIELD_VPAD * 2;
        int field_x = x + w - ROW_PAD - field_w;
        int field_y = row_y + FIELD_VPAD;

        if (g_rows[i].is_asset_path) {
            unsigned char r = g_rows[i].hovered ? UI_COLOR_BUTTON_BLUE.r : UI_COLOR_GRAY_MUTED.r;
            unsigned char g = g_rows[i].hovered ? UI_COLOR_BUTTON_BLUE.g : UI_COLOR_GRAY_MUTED.g;
            unsigned char b = g_rows[i].hovered ? UI_COLOR_BUTTON_BLUE.b : UI_COLOR_GRAY_MUTED.b;
            window_fill_rect(field_x, field_y, field_w, field_h, r, g, b);
            if (g_rows[i].value_tex != NULL) {
                window_draw_texture(g_rows[i].value_tex,
                                     field_x + (field_w - g_rows[i].value_w) / 2,
                                     field_y + (field_h - g_rows[i].value_h) / 2,
                                     g_rows[i].value_w, g_rows[i].value_h);
            }
        } else {
            text_field_draw(&g_rows[i].field, field_x, field_y, field_w, field_h, 13);
        }
    }
    int viewport_y = y + HEADER_HEIGHT;
    int viewport_h = h - HEADER_HEIGHT; if (viewport_h < ROW_HEIGHT) viewport_h = ROW_HEIGHT;
    int content_h = g_row_count * ROW_HEIGHT;
    ui_scrollbar_draw(g_scroll_offset, x, w, viewport_y, viewport_h, content_h);
}

void properties_panel_shutdown(void) {
    properties_panel_clear_selection();
}
