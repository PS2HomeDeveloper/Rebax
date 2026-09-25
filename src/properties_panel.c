/*
 * ============================================================
 * properties_panel.c
 * ============================================================
 * القيم المعروضة والمعدَّلة هنا حية حقيقية - كل خانة تحرير مربوطة
 * مباشرة بذاكرة نسخة العقدة الفعلية بشجرة المشهد (عبر
 * scene_tree_panel_get_values)، بلا أي نسخة عرض وسيطة. التعديل هنا
 * ينعكس فوراً على نفس البيانات اللي الفيوبورت يرسم منها.
 *
 * خاصية STRING بعلامة is_asset_path=1 (زي "Image Path" بـSprite2D)
 * تُعرض كزر "تصفح" بدل خانة كتابة حرة - يفتح asset_browser محصوراً
 * بـAssets:// المشروع، ويكتب المسار المختار مباشرة على نفس ذاكرة
 * العقدة الحقيقية عند التأكيد.
 * ============================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>

#include "properties_panel.h"
#include "scene_tree_panel.h"
#include "node_registry.h"
#include "window.h"
#include "font.h"
#include "text_field.h"
#include "asset_browser.h"
#include "current_scene.h"
#include "ui_project_center.h" /* لأجل ألوان الثيم */

#define ROW_HEIGHT      22
#define HEADER_HEIGHT   28
#define ROW_PAD          8
#define FIELD_WIDTH     90 /* عرض خانة/زر القيمة، ملاصق للحافة اليمنى */
#define FIELD_VPAD       3 /* هامش رأسي بين الخانة وحدود صفها */

/* الامتدادات المقبولة لأي خاصية is_asset_path حالياً - نفس الصيغ
 * اللي src/nodes/sprite2d.c الحقيقية تقدر تحمّلها فعلاً (عبر
 * image_loader.h المشتركة - راجع تعليق ذاك الملف): PNG/JPEG/BMP/TGA/
 * TIFF عبر gsKit مباشرة، وRAW/TIM2/TIM عبر فك تشفيرنا الخاص - عمداً
 * بدون GIF لأنها غير مدعومة إطلاقاً (فخ تسمية، راجع النقاش السابق) */
static const char *g_image_extensions[] = { "png", "jpg", "jpeg", "bmp", "tga", "tif", "tiff", "raw", "tm2", "tim2", "tim" };
#define IMAGE_EXTENSION_COUNT 11

typedef struct {
    window_texture_t *name_tex;
    int name_w, name_h;
    text_field_t field;
    node_property_type_t type;
    int is_asset_path;
    int hovered; /* لزر التصفح بس - تغذية بصرية عند التحويم */

    /* نص زر التصفح (اسم الملف المختصر أو "Browse...") - يُبنى مرة
     * وحدة بـrebuild_rows، مو كل إطار رسم (بعكس لو أنشأناه بالرسم
     * مباشرة، اللي كان يعني نسيج جديد كل إطار بلا داعٍ) */
    window_texture_t *value_tex;
    int value_w, value_h;
} property_row_t;

/* سياق صغير يُمرَّر لـasset_browser_open وقت الضغط على زر تصفح -
 * يُخصَّص عند الضغط، ويُحرَّر داخل on_asset_picked مهما كانت النتيجة
 * (اختيار حقيقي أو إلغاء) */
typedef struct {
    int node_index;
    int property_index;
} asset_pick_context_t;

static int g_selected_node_index = -1;

static window_texture_t *g_header_tex = NULL;
static int g_header_w = 0, g_header_h = 0;

static property_row_t g_rows[32];
static int g_row_count = 0;

static void free_rows(void) {
    for (int i = 0; i < g_row_count; i++) {
        if (g_rows[i].name_tex) window_destroy_texture(g_rows[i].name_tex);
        if (g_rows[i].value_tex) window_destroy_texture(g_rows[i].value_tex);
        text_field_free(&g_rows[i].field);
    }
    g_row_count = 0;
}

/* يرجّع بس اسم الملف من مسار كامل (بعد آخر '/') - للعرض المختصر
 * بزر التصفح، بلا المسار الكامل الطويل */
static const char *basename_of(const char *path) {
    const char *slash = strrchr(path, '/');
    return (slash != NULL) ? (slash + 1) : path;
}

/* يبني كل صفوف الخصائص (تسمية + خانة تحرير/زر تصفح، مهيَّأة بالقيمة
 * الحقيقية الحالية) من جديد - يُستدعى عند تغيّر التحديد، أو بعد
 * اختيار ملف جديد بزر تصفح (لتحديث نص الزر فوراً) */
static void rebuild_rows(void) {
    free_rows();

    if (g_header_tex) {
        window_destroy_texture(g_header_tex);
        g_header_tex = NULL;
    }

    if (g_selected_node_index < 0) return;

    node_type_t type = scene_tree_panel_get_type(g_selected_node_index);
    const char *node_name = scene_tree_panel_get_name(g_selected_node_index);
    const node_registry_entry_t *entry = node_registry_get(type);
    node_property_value_t *values = scene_tree_panel_get_values(g_selected_node_index);

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

        /* القيمة الابتدائية = القيمة الحقيقية الحالية لهذي العقدة
         * بالضبط، مو الافتراضي بمخطط النوع. لخاصية is_asset_path
         * نخزّن بالحقل نفسه اسم الملف المختصر بس (للعرض بالزر) -
         * المسار الكامل الحقيقي يبقى بـvalues[i].s وحده، الحقل هنا
         * عرض بصري بس مو مصدر الحقيقة */
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
                             (path != NULL && path[0] != '\0') ? basename_of(path) : "Browse...");
                } else {
                    snprintf(row->field.text, TEXT_FIELD_MAX_LEN, "%s", values[i].s ? values[i].s : "");
                }
                break;
        }
        row->field.cursor_pos = (int)strlen(row->field.text);

        /* نسيج نص الزر - يُبنى مرة وحدة هنا بس لأزرار is_asset_path
         * (بعكس خانات الكتابة العادية اللي text_field تدير رسمها
         * بنفسها من نصها الحي كل إطار) */
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
    free_rows();
    if (g_header_tex) {
        window_destroy_texture(g_header_tex);
        g_header_tex = NULL;
    }
}

/* رد استدعاء اختيار ملف من asset_browser - تُستدعى مرة واحدة عند
 * إغلاق النافذة (اختيار حقيقي أو إلغاء) */
static void on_asset_picked(const char *picked_path, void *user_data) {
    asset_pick_context_t *ctx = (asset_pick_context_t *)user_data;

    if (picked_path != NULL && ctx->node_index == g_selected_node_index) {
        node_property_value_t *values = scene_tree_panel_get_values(ctx->node_index);
        if (values != NULL) {
            free(values[ctx->property_index].s);
            values[ctx->property_index].s = strdup(picked_path);
            current_scene_mark_dirty();
            rebuild_rows(); /* يحدّث نص الزر ليعرض اسم الملف الجديد فوراً */
        }
    }

    free(ctx);
}

void properties_panel_update(int x, int y, int w, int h) {
    (void)h;
    if (g_selected_node_index < 0) return;

    node_property_value_t *values = scene_tree_panel_get_values(g_selected_node_index);
    if (values == NULL) return;

    int mx = window_mouse_x();
    int my = window_mouse_y();

    int list_y = y + HEADER_HEIGHT;
    for (int i = 0; i < g_row_count; i++) {
        int row_y = list_y + i * ROW_HEIGHT;
        int field_w = FIELD_WIDTH;
        int field_h = ROW_HEIGHT - FIELD_VPAD * 2;
        int field_x = x + w - ROW_PAD - field_w;
        int field_y = row_y + FIELD_VPAD;

        if (g_rows[i].is_asset_path) {
            /* زر تصفح - بلا أي كتابة حرة، بس تحويم/ضغط */
            g_rows[i].hovered = (mx >= field_x && mx < field_x + field_w &&
                                 my >= field_y && my < field_y + field_h);

            if (g_rows[i].hovered && window_mouse_left_just_pressed() && !asset_browser_is_open()) {
                asset_pick_context_t *ctx = malloc(sizeof(asset_pick_context_t));
                ctx->node_index = g_selected_node_index;
                ctx->property_index = i;
                asset_browser_open(ASSET_BROWSER_ROOT_ASSETS, ASSET_BROWSER_MODE_PICK_FILE,
                                    g_image_extensions, IMAGE_EXTENSION_COUNT,
                                    on_asset_picked, ctx);
            }
            continue; /* بلا text_field_update ولا كتابة قيمة - القيمة تُكتب من on_asset_picked بس */
        }

        text_field_update(&g_rows[i].field, field_x, field_y, field_w, field_h, field_w);

        /* يكتب أي قيمة جديدة مباشرة على ذاكرة العقدة الحقيقية - نفس
         * المكان اللي الفيوبورت يقرأ منه وقت الرسم، بلا أي وسيط.
         * عام تماماً حسب نوع الخاصية (بلا أي معرفة باسمها أو معناها).
         * نعلّم المشهد "غير محفوظ" فقط لو القيمة تغيّرت فعلياً عن
         * الحالية - بعكس تعليمه كل إطار بلا داعٍ طول ما الخانة مفتوحة */
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
}

void properties_panel_draw(int x, int y, int w, int h) {
    (void)h;

    if (g_selected_node_index < 0) return;

    if (g_header_tex != NULL) {
        window_draw_texture(g_header_tex, x + ROW_PAD, y + (HEADER_HEIGHT - g_header_h) / 2, g_header_w, g_header_h);
    }

    int list_y = y + HEADER_HEIGHT;
    for (int i = 0; i < g_row_count; i++) {
        int row_y = list_y + i * ROW_HEIGHT;

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
}

void properties_panel_shutdown(void) {
    properties_panel_clear_selection();
}
