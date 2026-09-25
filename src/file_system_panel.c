/*
 * ============================================================
 * file_system_panel.c
 * ============================================================
 * إعادة الفحص: تصير فقط لما المستخدم يفعل شيء فعلي (فتح/إغلاق
 * مجلد، أول ظهور للبانل) - مو كل إطار ولا كل ثانية بمؤقّت. هذا
 * أرخص وأدق من أي فحص دوري: أي تغيير بمحتوى مجلد مفتوح يظهر فوراً
 * أول ما تقفل وتفتح نفس المجلد مرة ثانية (إعادة فحص عند الطلب،
 * مو مراقبة مستمرة بالخلفية).
 * ============================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "file_system_panel.h"
#include "window.h"
#include "shape_provider.h"
#include "icon_atlas.h"
#include "text_field.h"
#include "font.h"
#include "current_project.h"
#include "ui_project_center.h" /* لأجل ألوان الثيم */

#define MAX_ROWS        256
#define MAX_EXPANDED     64
#define ROW_HEIGHT       22
#define ICON_SIZE        16
#define ROW_PAD           6
/* إزاحة كل مستوى بالشجرة - رقم ثابت محدد (14 بكسل)، مو مشتق من
 * حجم الأيقونة + فراغ، عشان يطابق نفس الإحساس البصري المطلوب
 * (الابن قريب من أيقونة أبيه بمسافة محسوبة بدقة، مو فراغ كبير) */
#define INDENT_STEP      14
#define ARROW_ICON_GAP    2  /* المسافة بين سهم الفتح/الإغلاق والأيقونة الرئيسية */
#define TOOLBAR_HEIGHT   26
#define SEARCH_HEIGHT    20

typedef struct {
    char path[600];
    char display_name[128];
    int depth;
    int is_dir;
    int has_children;
    window_texture_t *name_tex;
    int name_w, name_h;
} fs_row_t;

static fs_row_t g_rows[MAX_ROWS];
static int g_row_count = 0;

static char g_expanded[MAX_EXPANDED][600];
static int g_expanded_count = 0;

static text_field_t g_search_field;
static int g_ready = 0;
static int g_needs_rebuild = 1;
static char g_last_project_path[600] = {0};

static int is_expanded(const char *path) {
    for (int i = 0; i < g_expanded_count; i++) {
        if (strcmp(g_expanded[i], path) == 0) return 1;
    }
    return 0;
}

static void toggle_expanded(const char *path) {
    for (int i = 0; i < g_expanded_count; i++) {
        if (strcmp(g_expanded[i], path) == 0) {
            for (int j = i; j < g_expanded_count - 1; j++) {
                strcpy(g_expanded[j], g_expanded[j + 1]);
            }
            g_expanded_count--;
            return;
        }
    }
    if (g_expanded_count < MAX_EXPANDED) {
        strncpy(g_expanded[g_expanded_count], path, sizeof(g_expanded[0]) - 1);
        g_expanded[g_expanded_count][sizeof(g_expanded[0]) - 1] = '\0';
        g_expanded_count++;
    }
}

static int dir_has_children(const char *path) {
    DIR *d = opendir(path);
    if (d == NULL) return 0;
    struct dirent *entry;
    int found = 0;
    while ((entry = readdir(d)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
        found = 1;
        break;
    }
    closedir(d);
    return found;
}

static int compare_names(const void *a, const void *b) {
    return strcmp(*(const char **)a, *(const char **)b);
}

static void add_directory_contents(const char *dir_path, int depth) {
    DIR *d = opendir(dir_path);
    if (d == NULL) return;

    char *names[256];
    int count = 0;
    struct dirent *entry;
    while ((entry = readdir(d)) != NULL && count < 256) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
        names[count] = strdup(entry->d_name);
        count++;
    }
    closedir(d);

    qsort(names, count, sizeof(char *), compare_names);

    for (int i = 0; i < count && g_row_count < MAX_ROWS; i++) {
        char full_path[600];
        snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, names[i]);

        struct stat st;
        if (stat(full_path, &st) != 0) { free(names[i]); continue; }

        fs_row_t *row = &g_rows[g_row_count++];
        strncpy(row->path, full_path, sizeof(row->path) - 1);
        row->path[sizeof(row->path) - 1] = '\0';
        strncpy(row->display_name, names[i], sizeof(row->display_name) - 1);
        row->display_name[sizeof(row->display_name) - 1] = '\0';
        row->depth = depth;
        row->is_dir = S_ISDIR(st.st_mode);
        /* يُفحص دائماً بلحظة إعادة البناء (يعني بلحظة الفتح/الإغلاق
         * الفعلية) - فحص "حي" وقت الحاجة، مو تخزين قديم */
        row->has_children = row->is_dir ? dir_has_children(full_path) : 0;

        font_text_image_t txt = font_render_text(row->display_name, FONT_WEIGHT_REGULAR, 14);
        row->name_tex = (txt.pixels != NULL) ? window_create_texture(txt.pixels, txt.width, txt.height) : NULL;
        row->name_w = txt.width;
        row->name_h = txt.height;
        font_free_text_image(&txt);

        free(names[i]);

        if (row->is_dir && is_expanded(full_path)) {
            add_directory_contents(full_path, depth + 1);
        }
    }
}

static void free_rows(void) {
    for (int i = 0; i < g_row_count; i++) {
        if (g_rows[i].name_tex) window_destroy_texture(g_rows[i].name_tex);
    }
    g_row_count = 0;
}

static void rebuild_rows(void) {
    free_rows();

    const char *project_path = current_project_get_path();
    if (project_path == NULL) return;

    char assets_path[600];
    snprintf(assets_path, sizeof(assets_path), "%s/Assets", project_path);

    struct stat st;
    if (stat(assets_path, &st) != 0) {
        mkdir(assets_path, 0755);
    }

    fs_row_t *root = &g_rows[g_row_count++];
    strncpy(root->path, assets_path, sizeof(root->path) - 1);
    root->path[sizeof(root->path) - 1] = '\0';
    strncpy(root->display_name, "Assets://", sizeof(root->display_name) - 1);
    root->depth = 0;
    root->is_dir = 1;
    root->has_children = dir_has_children(assets_path);

    font_text_image_t txt = font_render_text("Assets://", FONT_WEIGHT_REGULAR, 14);
    root->name_tex = (txt.pixels != NULL) ? window_create_texture(txt.pixels, txt.width, txt.height) : NULL;
    root->name_w = txt.width;
    root->name_h = txt.height;
    font_free_text_image(&txt);

    if (is_expanded(assets_path)) {
        add_directory_contents(assets_path, 1);
    }

    g_needs_rebuild = 0;
}

static int icon_for_file(const char *name) {
    const char *ext = strrchr(name, '.');
    if (ext == NULL) return ICON_file;
    ext++;

    if (strcmp(ext, "c") == 0)   return ICON_c_file;
    if (strcmp(ext, "h") == 0)   return ICON_h_file;
    if (strcmp(ext, "cpp") == 0) return ICON_cpp_file;
    if (strcmp(ext, "hpp") == 0) return ICON_hpp_file;
    if (strcmp(ext, "png") == 0 || strcmp(ext, "jpg") == 0 || strcmp(ext, "jpeg") == 0)
        return ICON_image_file;
    if (strcmp(ext, "wav") == 0 || strcmp(ext, "mp3") == 0 || strcmp(ext, "ogg") == 0)
        return ICON_Audio_file;
    return ICON_file;
}

static void ensure_ready(void) {
    if (g_ready) return;
    g_ready = 1;
    text_field_init(&g_search_field);
    icon_atlas_init();
}

void file_system_panel_update(int x, int y, int w, int h) {
    (void)h;
    ensure_ready();

    const char *project_path = current_project_get_path();
    if (project_path != NULL && strcmp(project_path, g_last_project_path) != 0) {
        strncpy(g_last_project_path, project_path, sizeof(g_last_project_path) - 1);
        g_last_project_path[sizeof(g_last_project_path) - 1] = '\0';
        g_needs_rebuild = 1;
    }

    if (g_needs_rebuild) {
        rebuild_rows();
    }

    /* شريط الأدوات كله بصف واحد: أيقونة بحث + حقل بحث يملأ الفراغ
     * + أيقونتا استيراد/مزيد أقصى اليمين - كلهم بنفس ارتفاع الصف */
    int toolbar_icon_y = y + (TOOLBAR_HEIGHT - ICON_SIZE) / 2;

    int more_x   = x + w - ICON_SIZE - ROW_PAD;
    int import_x = more_x - ICON_SIZE - ROW_PAD;

    int search_icon_x = x + ROW_PAD;
    int search_field_x = search_icon_x + ICON_SIZE + ROW_PAD;
    int search_field_w = import_x - ROW_PAD - search_field_x;
    int search_field_y = y + (TOOLBAR_HEIGHT - SEARCH_HEIGHT) / 2;

    text_field_update(&g_search_field, search_field_x, search_field_y, search_field_w, SEARCH_HEIGHT, search_field_w);

    int mx = window_mouse_x(), my = window_mouse_y();

    if (window_mouse_left_just_pressed()) {
        if (mx >= more_x && mx < more_x + ICON_SIZE && my >= toolbar_icon_y && my < toolbar_icon_y + ICON_SIZE) {
            /* مستقبلاً: قائمة منبثقة (New File, New Folder...) */
        } else if (mx >= import_x && mx < import_x + ICON_SIZE && my >= toolbar_icon_y && my < toolbar_icon_y + ICON_SIZE) {
            /* مستقبلاً: فتح مستعرض ملفات النظام لاستيراد مورد */
        }
    }

    /* النقر على سهم الفتح/الإغلاق - متاح لأي مجلد (حتى لو فارغ حالياً)،
     * عشان لو المستخدم فتحه ثم أضاف ملفاً من خارج المحرك، يشوفه أول
     * ما يقفل ويفتح المجلد مرة ثانية (إعادة فحص عند الطلب فقط) */
    int list_y = y + TOOLBAR_HEIGHT + 4;
    if (window_mouse_left_just_pressed()) {
        for (int i = 0; i < g_row_count; i++) {
            if (!g_rows[i].is_dir) continue;
            int row_y = list_y + i * ROW_HEIGHT;
            int arrow_x = x + ROW_PAD + g_rows[i].depth * INDENT_STEP;
            if (mx >= arrow_x && mx < arrow_x + ICON_SIZE
                && my >= row_y && my < row_y + ROW_HEIGHT) {
                toggle_expanded(g_rows[i].path);
                g_needs_rebuild = 1;
                break;
            }
        }
    }
}

void file_system_panel_draw(int x, int y, int w, int h) {
    (void)h;

    int toolbar_icon_y = y + (TOOLBAR_HEIGHT - ICON_SIZE) / 2;

    int more_x   = x + w - ICON_SIZE - ROW_PAD;
    int import_x = more_x - ICON_SIZE - ROW_PAD;

    int search_icon_x = x + ROW_PAD;
    int search_field_x = search_icon_x + ICON_SIZE + ROW_PAD;
    int search_field_w = import_x - ROW_PAD - search_field_x;
    int search_field_y = y + (TOOLBAR_HEIGHT - SEARCH_HEIGHT) / 2;

    icon_atlas_draw(ICON_search, search_icon_x, toolbar_icon_y, ICON_SIZE);
    text_field_draw(&g_search_field, search_field_x, search_field_y, search_field_w, SEARCH_HEIGHT, 13);

    icon_atlas_draw(ICON_more, more_x, toolbar_icon_y, ICON_SIZE);
    icon_atlas_draw(ICON_import_file, import_x, toolbar_icon_y, ICON_SIZE);

    int list_y = y + TOOLBAR_HEIGHT + 4;

    for (int i = 0; i < g_row_count; i++) {
        fs_row_t *row = &g_rows[i];
        int row_y = list_y + i * ROW_HEIGHT;
        int base_x = x + ROW_PAD + row->depth * INDENT_STEP;

        if (row->is_dir && row->has_children && is_expanded(row->path)) {
            int line_x = base_x + ICON_SIZE / 2;
            int line_top = row_y + ROW_HEIGHT;
            int line_bottom = line_top;
            for (int j = i + 1; j < g_row_count && g_rows[j].depth > row->depth; j++) {
                if (g_rows[j].depth == row->depth + 1) {
                    line_bottom = list_y + j * ROW_HEIGHT + ROW_HEIGHT / 2;
                }
            }
            window_fill_rect(line_x, line_top, 1, line_bottom - line_top, 255, 255, 255);
        }
        if (row->depth > 0) {
            int parent_line_x = base_x - INDENT_STEP + ICON_SIZE / 2;
            int mid_y = row_y + ROW_HEIGHT / 2;
            window_fill_rect(parent_line_x, mid_y, base_x - parent_line_x, 1, 255, 255, 255);
        }

        /* الأيقونة الرئيسية (ملف أو مجلد) دائماً بعد سهم الفتح/الإغلاق
         * + فراغ ثابت 2 بكسل - سواء رُسم سهم فعلي أو لأ، عشان أي صف
         * بنفس العمق يصطف بنفس المحاذاة بالضبط، ملف كان أو مجلد */
        int icon_x = base_x + ICON_SIZE + ARROW_ICON_GAP;

        if (row->is_dir) {
            int expanded = is_expanded(row->path);
            /* سهم يشير يميناً مغلق (0°)، يدور 90° مع عقارب الساعة
             * فيصير يشير لتحت مفتوح - بلا أي تغيير لو المجلد فارغ
             * (يدور، بس ما فيه محتوى يُرسم تحته، وهذا متوقع وصحيح) */
            icon_atlas_draw_rotated(ICON_tree_expand_collapse, base_x,
                                     row_y + (ROW_HEIGHT - ICON_SIZE) / 2, ICON_SIZE,
                                     expanded ? 90.0 : 0.0);
            icon_atlas_draw_tinted(ICON_Folder, icon_x, row_y + (ROW_HEIGHT - ICON_SIZE) / 2, ICON_SIZE,
                                    230, 200, 60); /* أصفر */
        } else {
            icon_atlas_draw(icon_for_file(row->display_name), icon_x,
                             row_y + (ROW_HEIGHT - ICON_SIZE) / 2, ICON_SIZE);
        }

        int name_x = icon_x + ICON_SIZE + ROW_PAD;
        if (row->name_tex != NULL) {
            window_draw_texture(row->name_tex, name_x, row_y + (ROW_HEIGHT - row->name_h) / 2,
                                 row->name_w, row->name_h);
        }
    }
}

void file_system_panel_shutdown(void) {
    free_rows();
    text_field_free(&g_search_field);
    g_ready = 0;
    g_needs_rebuild = 1;
    g_last_project_path[0] = '\0';
    g_expanded_count = 0;
}
