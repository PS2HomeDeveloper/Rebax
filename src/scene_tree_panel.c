/*
 * ============================================================
 * scene_tree_panel.c
 * ============================================================
 * شجرة عقد حقيقية بعلاقة أب/ابن، بنفس نظام file_system_panel.c
 * بالضبط (سهم دوّار، إزاحة 14 بكسل، فراغ 2 بكسل بين السهم
 * والأيقونة، خطوط بيضاء رفيعة تربط الأب بأبنائه).
 * ============================================================
 */

#include <string.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>

#include "scene_tree_panel.h"
#include "window.h"
#include "shape_provider.h"
#include "icon_atlas.h"
#include "node_registry.h"
#include "properties_panel.h"
#include "current_scene.h"
#include "ui_project_center.h" /* لأجل ألوان الثيم */
#include "add_node_dialog.h"
#include "font.h"

#define MAX_NODES     128
#define ROW_HEIGHT     22
#define HEADER_HEIGHT  28
#define ICON_SIZE      16
#define ROW_PAD         6
#define INDENT_STEP    14  /* نفس file_system_panel.c بالضبط */
#define ARROW_ICON_GAP  2  /* نفس file_system_panel.c بالضبط */

typedef struct {
    node_type_t type;
    char name[32];
    int parent_index; /* -1 = جذر */
    int expanded;      /* هل مفتوحة (تعرض أبناءها)؟ */
    window_texture_t *name_tex;
    int name_w, name_h;

    /* القيم الفعلية الحية لكل خاصية بهذي العقدة - مصفوفة مخصَّصة
     * ديناميكياً بطول property_count بجدول node_registry لنفس
     * النوع، مُهيَّأة من نفس القيم الافتراضية وقت الإضافة، وتتحدّث
     * فعلياً من properties_panel. عامة تماماً (بلا أي حقل باسم خاصية
     * معينة) - عقدة بخصائص أكثر أو أقل تشتغل بنفس الكود، بلا أي
     * تعديل هنا */
    node_property_value_t *values;
    int value_count;
} scene_node_t;

static scene_node_t g_nodes[MAX_NODES];
static int g_node_count = 0;
static int g_selected_index = -1;
static int g_hovered_index = -1;

static int g_ready = 0;

/* صفوف مرئية حالياً (بترتيب العرض، بعد تصفية الأبناء المطويين) -
 * تُحسب من جديد كل إطار (رخيصة: مصفوفة بالذاكرة، مو فحص قرص) */
static int g_visible[MAX_NODES];
static int g_depth[MAX_NODES];
static int g_visible_count = 0;

/* تخزين مؤقت لشريط التحديد (hover/selected) - يُعاد توليده فقط لو
 * العرض تغيّر، بدل كل إطار */
static window_texture_t *g_hover_tex = NULL;
static window_texture_t *g_selected_tex = NULL;
static int g_highlight_w = 0;

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
    icon_atlas_init();
}

static int node_has_children(int index) {
    for (int i = 0; i < g_node_count; i++) {
        if (g_nodes[i].parent_index == index) return 1;
    }
    return 0;
}

/* يبني قائمة الصفوف المرئية (جذور دائماً ظاهرة، أبناء بس لو أبوهم
 * مفتوح، بعمق أي مستوى) */
static void rebuild_visible(void) {
    g_visible_count = 0;

    for (int i = 0; i < g_node_count; i++) {
        if (g_nodes[i].parent_index != -1) continue;
        if (g_visible_count >= MAX_NODES) break;
        g_visible[g_visible_count] = i;
        g_depth[g_visible_count] = 0;
        g_visible_count++;
    }

    for (int pass = 0; pass < MAX_NODES; pass++) {
        int inserted = 0;
        for (int vi = 0; vi < g_visible_count; vi++) {
            int idx = g_visible[vi];
            if (!g_nodes[idx].expanded) continue;

            int already = (vi + 1 < g_visible_count)
                          && (g_nodes[g_visible[vi + 1]].parent_index == idx);
            if (already) continue;

            int children[MAX_NODES];
            int child_count = 0;
            for (int j = 0; j < g_node_count; j++) {
                if (g_nodes[j].parent_index == idx && child_count < MAX_NODES) {
                    children[child_count++] = j;
                }
            }
            if (child_count == 0) continue;

            for (int k = g_visible_count - 1; k > vi; k--) {
                if (k + child_count < MAX_NODES) {
                    g_visible[k + child_count] = g_visible[k];
                    g_depth[k + child_count] = g_depth[k];
                }
            }
            for (int c = 0; c < child_count && (vi + 1 + c) < MAX_NODES; c++) {
                g_visible[vi + 1 + c] = children[c];
                g_depth[vi + 1 + c] = g_depth[vi] + 1;
            }
            g_visible_count += child_count;
            if (g_visible_count > MAX_NODES) g_visible_count = MAX_NODES;
            inserted = 1;
            break;
        }
        if (!inserted) break;
    }
}

/* ينشئ عقدة خام بنوع/اسم/أب محدَّدين صراحة - القيم تُهيَّأ من
 * افتراضيات نوعها بالمخطط. مشتركة بين scene_tree_panel_add_node
 * (يحسب الأب من التحديد الحالي) وscene_tree_panel_deserialize
 * (الأب مذكور صراحة بالملف) - نفس منطق الإنشاء الحقيقي بالاثنين،
 * بلا تكرار. يرجّع NULL لو الشجرة ممتلئة (MAX_NODES) */
static scene_node_t *create_node_raw(node_type_t type, const char *name, int parent_index) {
    if (g_node_count >= MAX_NODES) return NULL;

    scene_node_t *n = &g_nodes[g_node_count];
    n->type = type;
    strncpy(n->name, name, sizeof(n->name) - 1);
    n->name[sizeof(n->name) - 1] = '\0';
    n->parent_index = parent_index;
    n->expanded = 0;

    font_text_image_t txt = font_render_text(name, FONT_WEIGHT_REGULAR, 15);
    n->name_tex = (txt.pixels != NULL) ? window_create_texture(txt.pixels, txt.width, txt.height) : NULL;
    n->name_w = txt.width;
    n->name_h = txt.height;
    font_free_text_image(&txt);

    /* مصفوفة قيم حقيقية بطول property_count بالضبط لنفس النوع -
     * تُهيَّأ من نفس القيم الافتراضية بالمخطط، بلا أي معرفة بأسماء
     * أو معاني الخصائص هنا (لف عام بس حسب النوع المصرَّح بكل خاصية) */
    const node_registry_entry_t *entry = node_registry_get(type);
    n->value_count = (entry != NULL) ? entry->property_count : 0;
    n->values = NULL;
    if (n->value_count > 0) {
        n->values = malloc(sizeof(node_property_value_t) * (size_t)n->value_count);
        for (int i = 0; i < n->value_count; i++) {
            const node_property_t *prop = &entry->properties[i];
            switch (prop->type) {
                case NODE_PROPERTY_TYPE_FLOAT:
                    n->values[i].f = prop->default_value.f;
                    break;
                case NODE_PROPERTY_TYPE_INT:
                    n->values[i].i = prop->default_value.i;
                    break;
                case NODE_PROPERTY_TYPE_STRING:
                    n->values[i].s = prop->default_value.s ? strdup(prop->default_value.s) : strdup("");
                    break;
            }
        }
    }

    if (parent_index >= 0 && parent_index < g_node_count) {
        g_nodes[parent_index].expanded = 1;
    }

    g_node_count++;
    return n;
}

void scene_tree_panel_add_node(node_type_t type, const char *name) {
    int parent = -1;
    if (g_node_count == 0) {
        parent = -1;
    } else if (g_selected_index >= 0 && g_selected_index < g_node_count) {
        parent = g_selected_index;
    } else {
        parent = 0;
    }

    create_node_raw(type, name, parent);
    current_scene_mark_dirty(); /* إضافة تفاعلية حقيقية - بعكس create_node_raw
                                  * المستخدمة أيضاً داخلياً بـdeserialize (تحميل
                                  * مشهد محفوظ ما يفترض يعلّمه "غير محفوظ" فوراً) */
    rebuild_visible();
}

void scene_tree_panel_clear(void) {
    for (int i = 0; i < g_node_count; i++) {
        if (g_nodes[i].name_tex) window_destroy_texture(g_nodes[i].name_tex);

        if (g_nodes[i].values != NULL) {
            const node_registry_entry_t *entry = node_registry_get(g_nodes[i].type);
            for (int p = 0; p < g_nodes[i].value_count; p++) {
                if (entry != NULL && entry->properties[p].type == NODE_PROPERTY_TYPE_STRING) {
                    free(g_nodes[i].values[p].s);
                }
            }
            free(g_nodes[i].values);
            g_nodes[i].values = NULL;
        }
    }
    g_node_count = 0;
    g_visible_count = 0;
    g_selected_index = -1;
    g_hovered_index = -1;
    properties_panel_clear_selection();
    rebuild_visible();
}

/* يبحث عن مدخل السجل بالاسم (مو بقيمة enum الرقمية) - الرقمية غير
 * موثوقة للتخزين طويل الأمد لأنها تعتمد على ترتيب معالجة ملفات
 * src/nodes (ملفات .c) وقت التوليد، ممكن تتغيّر لو أُضيف/حُذف/أُعيد
 * تسمية ملف عقدة. الاسم ("Element2D"...) ثابت ومستقر عبر أي بناء */
static const node_registry_entry_t *find_registry_entry_by_name(const char *name) {
    int count = node_registry_count();
    for (int i = 0; i < count; i++) {
        const node_registry_entry_t *e = node_registry_get_by_index(i);
        if (e != NULL && strcmp(e->name, name) == 0) return e;
    }
    return NULL;
}

int scene_tree_panel_serialize(char *buffer, int buffer_size) {
    int pos = 0;
    int written;

#define SCENE_APPEND(...) \
    do { \
        written = snprintf(buffer + pos, (pos < buffer_size) ? (size_t)(buffer_size - pos) : 0, __VA_ARGS__); \
        if (written < 0) return -1; \
        pos += written; \
    } while (0)

    SCENE_APPEND("scene_version=1\n");
    SCENE_APPEND("node_count=%d\n\n", g_node_count);

    for (int i = 0; i < g_node_count; i++) {
        const node_registry_entry_t *entry = node_registry_get(g_nodes[i].type);

        SCENE_APPEND("[node %d]\n", i);
        /* الاسم بالنوع (entry->name)، مو قيمة enum الرقمية - راجع
         * تعليق find_registry_entry_by_name فوق */
        SCENE_APPEND("type=%s\n", entry != NULL ? entry->name : "");
        SCENE_APPEND("name=%s\n", g_nodes[i].name);
        SCENE_APPEND("parent=%d\n", g_nodes[i].parent_index);

        if (entry != NULL) {
            for (int p = 0; p < g_nodes[i].value_count; p++) {
                const node_property_t *prop = &entry->properties[p];
                int is_default = 0;

                switch (prop->type) {
                    case NODE_PROPERTY_TYPE_FLOAT:
                        is_default = (g_nodes[i].values[p].f == prop->default_value.f);
                        break;
                    case NODE_PROPERTY_TYPE_INT:
                        is_default = (g_nodes[i].values[p].i == prop->default_value.i);
                        break;
                    case NODE_PROPERTY_TYPE_STRING: {
                        const char *cur = g_nodes[i].values[p].s ? g_nodes[i].values[p].s : "";
                        const char *def = prop->default_value.s ? prop->default_value.s : "";
                        is_default = (strcmp(cur, def) == 0);
                        break;
                    }
                }
                /* بس اللي يختلف عن الافتراضي يُكتب - نفس فلسفة
                 * "نسجّل بس اللي تغيّر" المتفق عليها */
                if (is_default) continue;

                switch (prop->type) {
                    case NODE_PROPERTY_TYPE_FLOAT:
                        SCENE_APPEND("prop:%s=%.6f\n", prop->name, g_nodes[i].values[p].f);
                        break;
                    case NODE_PROPERTY_TYPE_INT:
                        SCENE_APPEND("prop:%s=%d\n", prop->name, g_nodes[i].values[p].i);
                        break;
                    case NODE_PROPERTY_TYPE_STRING:
                        SCENE_APPEND("prop:%s=%s\n", prop->name, g_nodes[i].values[p].s ? g_nodes[i].values[p].s : "");
                        break;
                }
            }
        }
        SCENE_APPEND("\n");
    }

#undef SCENE_APPEND

    if (pos >= buffer_size) return -1; /* المساحة ما كفت - المحتوى المكتوب جزئي وغير موثوق */
    return pos;
}

int scene_tree_panel_deserialize(const char *buffer) {
    scene_tree_panel_clear();

    const char *p = buffer;
    int cur_node_index = -1;
    char line[1024];

    while (*p != '\0') {
        int len = 0;
        while (p[len] != '\0' && p[len] != '\n' && len < (int)sizeof(line) - 1) len++;
        memcpy(line, p, (size_t)len);
        line[len] = '\0';
        p += len;
        if (*p == '\n') p++;

        if (line[0] == '\0') continue;                 /* سطر فاضي بين العقد */
        if (strncmp(line, "scene_version=", 14) == 0) continue;
        if (strncmp(line, "node_count=", 11) == 0) continue; /* معلوماتية بس حالياً */
        if (line[0] == '[') {                            /* "[node N]" - بداية عقدة جديدة */
            cur_node_index++;
            continue;
        }

        char *eq = strchr(line, '=');
        if (eq == NULL) continue;
        *eq = '\0';
        const char *key = line;
        const char *value = eq + 1;

        if (strcmp(key, "type") == 0) {
            const node_registry_entry_t *found = find_registry_entry_by_name(value);
            if (found == NULL) return 0; /* نوع عقدة غير معروف - ملف غير متوافق، فشل صريح */
            if (create_node_raw(found->type, "", -1) == NULL) return 0; /* الشجرة ممتلئة */
        } else if (cur_node_index < 0 || cur_node_index >= g_node_count) {
            return 0; /* سطر prop:/name=/parent= قبل أي سطر type= بنفس العقدة - ملف تالف */
        } else if (strcmp(key, "name") == 0) {
            scene_node_t *n = &g_nodes[cur_node_index];
            strncpy(n->name, value, sizeof(n->name) - 1);
            n->name[sizeof(n->name) - 1] = '\0';

            if (n->name_tex) window_destroy_texture(n->name_tex);
            font_text_image_t txt = font_render_text(n->name, FONT_WEIGHT_REGULAR, 15);
            n->name_tex = (txt.pixels != NULL) ? window_create_texture(txt.pixels, txt.width, txt.height) : NULL;
            n->name_w = txt.width;
            n->name_h = txt.height;
            font_free_text_image(&txt);
        } else if (strcmp(key, "parent") == 0) {
            g_nodes[cur_node_index].parent_index = atoi(value);
            int parent_index = g_nodes[cur_node_index].parent_index;
            if (parent_index >= 0 && parent_index < g_node_count) {
                g_nodes[parent_index].expanded = 1;
            }
        } else if (strncmp(key, "prop:", 5) == 0) {
            const char *prop_name = key + 5;
            scene_node_t *n = &g_nodes[cur_node_index];
            const node_registry_entry_t *entry = node_registry_get(n->type);
            if (entry != NULL) {
                for (int pi = 0; pi < n->value_count; pi++) {
                    if (strcmp(entry->properties[pi].name, prop_name) != 0) continue;
                    switch (entry->properties[pi].type) {
                        case NODE_PROPERTY_TYPE_FLOAT:
                            n->values[pi].f = (float)atof(value);
                            break;
                        case NODE_PROPERTY_TYPE_INT:
                            n->values[pi].i = atoi(value);
                            break;
                        case NODE_PROPERTY_TYPE_STRING:
                            free(n->values[pi].s);
                            n->values[pi].s = strdup(value);
                            break;
                    }
                    break;
                }
                /* خاصية مذكورة بالملف بس مو موجودة بنوع العقدة الحالي
                 * (اتحذفت لاحقاً من ملف src/nodes/<type>.c) - تُتجاهل
                 * بصمت، نفس فلسفة "قوي أمام تغيّر الخصائص مستقبلاً" */
            }
        }
    }

    g_selected_index = -1;
    rebuild_visible();
    return 1;
}

int scene_tree_panel_get_selected_index(void) {
    return g_selected_index;
}

int scene_tree_panel_get_node_count(void) {
    return g_node_count;
}

node_type_t scene_tree_panel_get_type(int index) {
    if (index < 0 || index >= g_node_count) return NODE_TYPE_ELEMENT;
    return g_nodes[index].type;
}

const char *scene_tree_panel_get_name(int index) {
    if (index < 0 || index >= g_node_count) return NULL;
    return g_nodes[index].name;
}

node_property_value_t *scene_tree_panel_get_values(int index) {
    if (index < 0 || index >= g_node_count) return NULL;
    return g_nodes[index].values;
}

void scene_tree_panel_select_index(int index) {
    if (index < 0 || index >= g_node_count) return;
    g_selected_index = index;
    properties_panel_set_selected(index);
}

void scene_tree_panel_set_float_property(int index, int property_index, float value) {
    if (index < 0 || index >= g_node_count || property_index < 0) return;
    if (property_index >= g_nodes[index].value_count) return;
    const node_registry_entry_t *entry = node_registry_get(g_nodes[index].type);
    if (entry == NULL || entry->properties[property_index].type != NODE_PROPERTY_TYPE_FLOAT) return;
    if (g_nodes[index].values[property_index].f == value) return;
    g_nodes[index].values[property_index].f = value;
    current_scene_mark_dirty();
}

void scene_tree_panel_update(int x, int y, int w, int h) {
    (void)h;
    ensure_ready();
    rebuild_visible();

    if (g_highlight_w != w && w > 0) {
        if (g_hover_tex) window_destroy_texture(g_hover_tex);
        if (g_selected_tex) window_destroy_texture(g_selected_tex);

        shape_image_t hov = shape_provider_render_rect(w, ROW_HEIGHT, 8,
            UI_COLOR_BUTTON_BLUE.r, UI_COLOR_BUTTON_BLUE.g, UI_COLOR_BUTTON_BLUE.b);
        dim_alpha(&hov, 0.25);
        g_hover_tex = window_create_texture(hov.pixels, hov.width, hov.height);
        shape_provider_free_image(&hov);

        shape_image_t sel = shape_provider_render_rect(w, ROW_HEIGHT, 8,
            UI_COLOR_BUTTON_BLUE.r, UI_COLOR_BUTTON_BLUE.g, UI_COLOR_BUTTON_BLUE.b);
        dim_alpha(&sel, 0.55);
        g_selected_tex = window_create_texture(sel.pixels, sel.width, sel.height);
        shape_provider_free_image(&sel);

        g_highlight_w = w;
    }

    int add_x = x + w - ICON_SIZE - ROW_PAD;
    int add_y = y + (HEADER_HEIGHT - ICON_SIZE) / 2;

    int mx = window_mouse_x(), my = window_mouse_y();

    if (window_mouse_left_just_pressed()) {
        if (mx >= add_x && mx < add_x + ICON_SIZE && my >= add_y && my < add_y + ICON_SIZE) {
            add_node_dialog_open();
        }
    }

    int list_y = y + HEADER_HEIGHT;
    g_hovered_index = -1;

    for (int vi = 0; vi < g_visible_count; vi++) {
        int idx = g_visible[vi];
        int depth = g_depth[vi];
        int row_y = list_y + vi * ROW_HEIGHT;
        int base_x = x + ROW_PAD + depth * INDENT_STEP;

        if (node_has_children(idx) && window_mouse_left_just_pressed()
            && mx >= base_x && mx < base_x + ICON_SIZE
            && my >= row_y && my < row_y + ROW_HEIGHT) {
            g_nodes[idx].expanded = !g_nodes[idx].expanded;
            break;
        }

        if (mx >= base_x && mx < x + w && my >= row_y && my < row_y + ROW_HEIGHT) {
            g_hovered_index = idx;
            if (window_mouse_left_just_pressed()) {
                g_selected_index = idx;
                properties_panel_set_selected(idx);
            }
        }
    }

    rebuild_visible();
}

void scene_tree_panel_draw(int x, int y, int w, int h) {
    (void)h;

    icon_atlas_draw(ICON_add, x + w - ICON_SIZE - ROW_PAD, y + (HEADER_HEIGHT - ICON_SIZE) / 2, ICON_SIZE);

    int list_y = y + HEADER_HEIGHT;

    for (int vi = 0; vi < g_visible_count; vi++) {
        int idx = g_visible[vi];
        int depth = g_depth[vi];
        int row_y = list_y + vi * ROW_HEIGHT;
        int base_x = x + ROW_PAD + depth * INDENT_STEP;

        if (idx == g_selected_index && g_selected_tex != NULL) {
            window_draw_texture(g_selected_tex, base_x, row_y, x + w - base_x, ROW_HEIGHT);
        } else if (idx == g_hovered_index && g_hover_tex != NULL) {
            window_draw_texture(g_hover_tex, base_x, row_y, x + w - base_x, ROW_HEIGHT);
        }

        int has_children = node_has_children(idx);

        if (has_children && g_nodes[idx].expanded) {
            int line_x = base_x + ICON_SIZE / 2;
            int line_top = row_y + ROW_HEIGHT;
            int line_bottom = line_top;
            for (int j = vi + 1; j < g_visible_count && g_depth[j] > depth; j++) {
                if (g_depth[j] == depth + 1) {
                    line_bottom = list_y + j * ROW_HEIGHT + ROW_HEIGHT / 2;
                }
            }
            window_fill_rect(line_x, line_top, 1, line_bottom - line_top, 255, 255, 255);
        }
        if (depth > 0) {
            int parent_line_x = base_x - INDENT_STEP + ICON_SIZE / 2;
            int mid_y = row_y + ROW_HEIGHT / 2;
            window_fill_rect(parent_line_x, mid_y, base_x - parent_line_x, 1, 255, 255, 255);
        }

        int icon_x = base_x + ICON_SIZE + ARROW_ICON_GAP;

        if (has_children) {
            icon_atlas_draw_rotated(ICON_tree_expand_collapse, base_x,
                                     row_y + (ROW_HEIGHT - ICON_SIZE) / 2, ICON_SIZE,
                                     g_nodes[idx].expanded ? 90.0 : 0.0);
        }

        const node_registry_entry_t *entry = node_registry_get(g_nodes[idx].type);
        if (entry != NULL) {
            icon_atlas_draw(entry->icon_id, icon_x, row_y + (ROW_HEIGHT - ICON_SIZE) / 2, ICON_SIZE);
        }

        int name_x = icon_x + ICON_SIZE + ROW_PAD;
        if (g_nodes[idx].name_tex != NULL) {
            window_draw_texture(g_nodes[idx].name_tex, name_x,
                                 row_y + (ROW_HEIGHT - g_nodes[idx].name_h) / 2,
                                 g_nodes[idx].name_w, g_nodes[idx].name_h);
        }
    }
}

void scene_tree_panel_shutdown(void) {
    for (int i = 0; i < g_node_count; i++) {
        if (g_nodes[i].name_tex) window_destroy_texture(g_nodes[i].name_tex);

        if (g_nodes[i].values != NULL) {
            const node_registry_entry_t *entry = node_registry_get(g_nodes[i].type);
            for (int p = 0; p < g_nodes[i].value_count; p++) {
                if (entry != NULL && entry->properties[p].type == NODE_PROPERTY_TYPE_STRING) {
                    free(g_nodes[i].values[p].s);
                }
            }
            free(g_nodes[i].values);
            g_nodes[i].values = NULL;
        }
    }
    g_node_count = 0;
    g_visible_count = 0;
    g_selected_index = -1;
    g_hovered_index = -1;
    if (g_hover_tex) window_destroy_texture(g_hover_tex);
    if (g_selected_tex) window_destroy_texture(g_selected_tex);
    g_highlight_w = 0;
    g_ready = 0;
}
