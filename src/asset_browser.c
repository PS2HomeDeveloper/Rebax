/*
 * ============================================================
 * asset_browser.c
 * ============================================================
 * تصفح مجلد واحد بكل مرة (مو بحث متكرر بكل المجلدات الفرعية دفعة
 * وحدة) - بالضبط زي أي مدير ملفات عادي: تدخل مجلداً، تشوف محتواه
 * فقط، وتتنقل بالدخول لمجلد فرعي أو الرجوع عبر شريط المسار. هذا
 * مقصود: يطابق سلوك شريط المسار المتحرك المطلوب، وأرخص بكثير من
 * فحص شجرة كاملة كل مرة تُفتح فيها النافذة.
 *
 * الصور المصغّرة (Thumbnails): تُفك فعلياً عبر stb_image لكل ملف
 * صورة موجود بالمجلد الحالي وقت الدخول إليه (مو كل الأيقونات دفعة
 * وحدة، فقط محتوى المجلد المعروض الآن) - أي ملف يفشل فك ضغطه
 * (تالف رغم امتداده الصحيح) يبقى بأيقونة الصورة الافتراضية بدلاً
 * من الصورة نفسها، بالضبط كما هو مطلوب. ملفات أكبر من THUMB_MAX_FILE_SIZE
 * تُستثنى من محاولة التصغير أصلاً (تجنّباً لتجميد الواجهة على ملف ضخم)
 * وتُعرض بأيقونتها الافتراضية مباشرة.
 *
 * ملاحظة: STB_IMAGE_IMPLEMENTATION لا يُعرَّف هنا - icon_atlas.c هو
 * المُصرَّح الوحيد له بكل المشروع (تعليق بذاك الملف). هنا نكتفي
 * بتضمين الهيدر واستخدام الدوال الجاهزة من التطبيق المُترجَم هناك.
 * ============================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h> /* strcasecmp */
#include <stddef.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "nodes_editor/image_loader.h" /* يدعم كل صيغ الصور اللي محركنا يدعمها - stb_image داخلياً لـPNG/JPEG/BMP/TGA/TIFF، وفك تشفيرنا الخاص لـRAW/TIM2/TIM */

#include "asset_browser.h"
#include "window.h"
#include "shape_provider.h"
#include "icon_atlas.h"
#include "labeled_button.h"
#include "text_field.h"
#include "font.h"
#include "current_project.h"
#include "ui_project_center.h" /* لأجل ألوان الثيم UI_COLOR_* */

/* ------------------------------------------------------------
 * ثوابت التخطيط
 * ------------------------------------------------------------ */
#define DIALOG_WIDTH        640
#define DIALOG_HEIGHT        460
#define DIALOG_PADDING         16
#define TITLEBAR_HEIGHT        32
#define CLOSE_BTN_SIZE          22
#define CRUMB_BAR_HEIGHT        24
#define TOOLBAR_HEIGHT          30
#define GRID_TOP_GAP             8
#define FOOTER_HEIGHT           44
#define FILENAME_ROW_HEIGHT     32 /* صف إضافي بس بوضع SAVE_FILE - خانة اسم الملف + تسميتها */

#define CELL_W                  84
#define CELL_H                  96
#define GRID_ICON_SIZE          48
#define CELL_CORNER_RADIUS       8
#define LABEL_FONT_SIZE         14 /* يطابق حجم أسماء العناصر بـfile_system_panel.c بالضبط - كان 12 بالغلط، غير متسق مع باقي مديري الملفات بالمحرك */
#define BUTTON_RADIUS             3

#define SCROLL_STEP_PX           40

#define AB_MAX_ENTRIES          512
#define AB_MAX_PATH              900
#define AB_MAX_EXTS               16
#define AB_MAX_CRUMBS             24

#define THUMB_MAX_FILE_SIZE (8 * 1024 * 1024) /* 8 ميجابايت - أكبر من كذا يُستثنى من التصغير */

/* ------------------------------------------------------------
 * أنواع البيانات الداخلية
 * ------------------------------------------------------------ */
typedef struct {
    char full_path[AB_MAX_PATH];
    char display_name[160];
    int  is_dir;

    window_texture_t *thumb_tex;   /* NULL لو بلا صورة مصغّرة حقيقية (يُستخدم أيقونة الامتداد بدلاً) */
    int  thumb_w, thumb_h;         /* أبعاد المصغّرة الفعلية (النسبة الأصلية محفوظة، بلا تمديد) */

    window_texture_t *label_tex;
    int  label_w, label_h;
} ab_entry_t;

/* ------------------------------------------------------------
 * حالة عامة (نسخة واحدة فقط بكل البرنامج - نفس نمط بقية النوافذ
 * المنبثقة بالمشروع: add_node_dialog، project_dialog)
 * ------------------------------------------------------------ */
static int g_ready = 0;
static int g_is_open = 0;

static asset_browser_root_t g_root_mode;
static asset_browser_mode_t g_pick_mode;
static asset_browser_callback_t g_callback = NULL;
static void *g_user_data = NULL;

static char g_extensions[AB_MAX_EXTS][16];
static int  g_extension_count = 0;

static char g_root_path[AB_MAX_PATH];
static char g_root_label[64];
static char g_current_path[AB_MAX_PATH];

static ab_entry_t g_entries[AB_MAX_ENTRIES];
static int g_entry_count = 0;

static char g_crumb_label[AB_MAX_CRUMBS][64];
static char g_crumb_path[AB_MAX_CRUMBS][AB_MAX_PATH];
static window_texture_t *g_crumb_tex[AB_MAX_CRUMBS];
static int  g_crumb_w[AB_MAX_CRUMBS], g_crumb_h[AB_MAX_CRUMBS];
static int  g_crumb_x[AB_MAX_CRUMBS], g_crumb_w_click[AB_MAX_CRUMBS]; /* مستطيلات الضغط المحسوبة كل إطار */
static int  g_crumb_count = 0;
static window_texture_t *g_crumb_sep_tex = NULL;
static int  g_crumb_sep_w = 0, g_crumb_sep_h = 0;

static text_field_t g_search_field;

/* وضع SAVE_FILE بس - خانة اسم الملف وتسميتها الثابتة "File name:"
 * (تُبنى مرة وحدة بـensure_ready، ما تتغيّر بين فتحات النافذة) */
static text_field_t g_filename_field;
static window_texture_t *g_filename_label_tex = NULL;
static int g_filename_label_w = 0, g_filename_label_h = 0;

static int g_selected_index = -1; /* بوضع اختيار ملف فقط - فهرس داخل g_entries */
static int g_scroll_offset = 0;

static window_texture_t *g_title_tex = NULL;
static int g_title_w = 0, g_title_h = 0;

static window_texture_t *g_filter_label_tex = NULL; /* "*.png, *.jpg" بجانب البحث - NULL لو بلا فلتر */
static int g_filter_label_w = 0, g_filter_label_h = 0;

static labeled_button_t  g_cancel_btn, g_select_btn;
static window_texture_t *g_cancel_btn_tex = NULL, *g_cancel_txt_tex = NULL;
static window_texture_t *g_select_btn_tex = NULL, *g_select_txt_tex = NULL;

static window_texture_t *g_close_btn_tex = NULL; /* خلفية زر الإغلاق X (مربع صغير بزوايا مدورة) */
static window_texture_t *g_close_x_tex = NULL;   /* حرف X نفسه */
static int g_close_x_w = 0, g_close_x_h = 0;

static window_texture_t *g_hover_tex = NULL;   /* تظليل مربع الشبكة عند التحويم */
static window_texture_t *g_selected_tex = NULL; /* تظليل مربع الشبكة المحدد حالياً */

static window_texture_t *g_dim_overlay_tex = NULL; /* خلفية شبه شفافة خلف كامل النافذة - راجع تعليق إنشائها بـensure_ready */

/* ------------------------------------------------------------
 * أدوات مساعدة صغيرة
 * ------------------------------------------------------------ */

/* كل صيغ الصور اللي محركنا يدعمها فعلياً (عبر image_loader.h
 * الموحَّدة - راجع تعليقها) - نفس قائمة g_image_extensions بملف
 * properties_panel.c بالضبط، عمداً بلا GIF (غير مدعومة إطلاقاً) */
static int is_raster_image_ext(const char *ext) {
    static const char *exts[] = { "png", "jpg", "jpeg", "bmp", "tga", "tif", "tiff", "raw", "tm2", "tim2", "tim" };
    for (size_t i = 0; i < sizeof(exts) / sizeof(exts[0]); i++) {
        if (strcasecmp(ext, exts[i]) == 0) return 1;
    }
    return 0;
}

static int extension_matches_filter(const char *name) {
    if (g_extension_count <= 0) return 1; /* بلا فلتر = كل الملفات مقبولة */
    const char *dot = strrchr(name, '.');
    if (dot == NULL) return 0;
    const char *ext = dot + 1;
    for (int i = 0; i < g_extension_count; i++) {
        if (strcasecmp(ext, g_extensions[i]) == 0) return 1;
    }
    return 0;
}

static int icon_for_extension(const char *name) {
    const char *dot = strrchr(name, '.');
    if (dot == NULL) return ICON_file;
    const char *ext = dot + 1;
    if (strcasecmp(ext, "c") == 0)   return ICON_c_file;
    if (strcasecmp(ext, "h") == 0)   return ICON_h_file;
    if (strcasecmp(ext, "cpp") == 0) return ICON_cpp_file;
    if (strcasecmp(ext, "hpp") == 0) return ICON_hpp_file;
    if (is_raster_image_ext(ext)) return ICON_image_file;
    if (strcasecmp(ext, "wav") == 0 || strcasecmp(ext, "mp3") == 0 || strcasecmp(ext, "ogg") == 0)
        return ICON_Audio_file;
    return ICON_file;
}

static int matches_search(const char *name) {
    if (g_search_field.text[0] == '\0') return 1;
    /* بحث فرعي بسيط بلا حساسية لحالة الأحرف - كافٍ تماماً لأسماء ملفات */
    size_t needle_len = strlen(g_search_field.text);
    size_t hay_len = strlen(name);
    if (needle_len > hay_len) return 0;
    for (size_t i = 0; i + needle_len <= hay_len; i++) {
        if (strncasecmp(name + i, g_search_field.text, needle_len) == 0) return 1;
    }
    return 0;
}

static window_texture_t *make_text_texture(const char *text, font_weight_t weight,
                                            int pixel_size, int *out_w, int *out_h) {
    font_text_image_t img = font_render_text(text, weight, pixel_size);
    window_texture_t *tex = NULL;
    if (img.pixels != NULL) {
        tex = window_create_texture(img.pixels, img.width, img.height);
        *out_w = img.width;
        *out_h = img.height;
        font_free_text_image(&img);
    }
    return tex;
}

static void make_button(const char *text, labeled_button_t *out_btn,
                         window_texture_t **out_btn_tex, window_texture_t **out_txt_tex) {
    *out_btn = labeled_button_create(text, FONT_WEIGHT_REGULAR, 15, 14, 6, BUTTON_RADIUS,
                                      UI_COLOR_BUTTON_BLUE.r, UI_COLOR_BUTTON_BLUE.g, UI_COLOR_BUTTON_BLUE.b);
    if (out_btn->width > 0) {
        *out_btn_tex = window_create_texture(out_btn->shape_img.pixels,
                                              out_btn->shape_img.width, out_btn->shape_img.height);
        *out_txt_tex = window_create_texture(out_btn->text_img.pixels,
                                              out_btn->text_img.width, out_btn->text_img.height);
    }
}

/* يصغّر بيانات صورة خام (RGBA) لتلائم مربع dst_size×dst_size بلا
 * تشويه (Letterbox - يحافظ على النسبة الأصلية)، بعيّنة أقرب جار
 * (Nearest Neighbor) - كافية تماماً لحجم مصغّرة صغير كهذا */
static window_texture_t *make_thumbnail_texture(const unsigned char *src, int sw, int sh,
                                                  int dst_size, int *out_w, int *out_h) {
    if (sw <= 0 || sh <= 0) return NULL;

    int dw, dh;
    if (sw >= sh) {
        dw = dst_size;
        dh = (int)((long)sh * dst_size / sw);
        if (dh < 1) dh = 1;
    } else {
        dh = dst_size;
        dw = (int)((long)sw * dst_size / sh);
        if (dw < 1) dw = 1;
    }

    unsigned char *dst = (unsigned char *)malloc((size_t)dw * (size_t)dh * 4);
    if (dst == NULL) return NULL;

    for (int y = 0; y < dh; y++) {
        int sy = (int)((long)y * sh / dh);
        for (int x = 0; x < dw; x++) {
            int sx = (int)((long)x * sw / dw);
            const unsigned char *sp = &src[(size_t)(sy * sw + sx) * 4];
            unsigned char *dp = &dst[(size_t)(y * dw + x) * 4];
            dp[0] = sp[0]; dp[1] = sp[1]; dp[2] = sp[2]; dp[3] = sp[3];
        }
    }

    window_texture_t *tex = window_create_texture(dst, dw, dh);
    free(dst);
    *out_w = dw;
    *out_h = dh;
    return tex;
}

/* ------------------------------------------------------------
 * تحميل محتوى مجلد
 * ------------------------------------------------------------ */

static void free_entries(void) {
    for (int i = 0; i < g_entry_count; i++) {
        if (g_entries[i].thumb_tex != NULL) window_destroy_texture(g_entries[i].thumb_tex);
        if (g_entries[i].label_tex != NULL) window_destroy_texture(g_entries[i].label_tex);
    }
    g_entry_count = 0;
}

static void add_entry(const char *full_path, const char *display_name, int is_dir) {
    if (g_entry_count >= AB_MAX_ENTRIES) return;

    ab_entry_t *e = &g_entries[g_entry_count++];
    memset(e, 0, sizeof(*e));
    strncpy(e->full_path, full_path, sizeof(e->full_path) - 1);
    strncpy(e->display_name, display_name, sizeof(e->display_name) - 1);
    e->is_dir = is_dir;

    font_text_image_t txt = font_render_text(display_name, FONT_WEIGHT_REGULAR, LABEL_FONT_SIZE);
    if (txt.pixels != NULL) {
        e->label_tex = window_create_texture(txt.pixels, txt.width, txt.height);
        e->label_w = txt.width;
        e->label_h = txt.height;
        font_free_text_image(&txt);
    }

    if (!is_dir) {
        const char *dot = strrchr(display_name, '.');
        if (dot != NULL && is_raster_image_ext(dot + 1)) {
            struct stat st;
            if (stat(full_path, &st) == 0 && st.st_size > 0 && st.st_size <= THUMB_MAX_FILE_SIZE) {
                int w, h;
                /* raw_width/height=0 عمداً هنا - المتصفح يستعرض ملفات
                 * عامة بلا معرفة أبعادها المقصودة (بعكس عقدة Sprite2D
                 * اللي عندها خصائص Raw Width/Height محدَّدة) - يرجع
                 * NULL بأمان لملفات .raw (تُعرض أيقونة افتراضية بدلها،
                 * السلوك الوحيد الممكن بلا معلومات خارجية عن أبعادها) */
                unsigned char *pixels = image_loader_editor_load(full_path, &w, &h, 0, 0, 0);
                if (pixels != NULL) {
                    e->thumb_tex = make_thumbnail_texture(pixels, w, h, GRID_ICON_SIZE, &e->thumb_w, &e->thumb_h);
                    free(pixels);
                }
                /* pixels == NULL يعني ملف صورة تالف رغم امتداده الصحيح -
                 * thumb_tex يبقى NULL، فتُرسم أيقونة الصورة الافتراضية
                 * بدلاً منها وقت الرسم (بالضبط السلوك المطلوب) */
            }
        }
    }
}

static int compare_strs_ci(const void *a, const void *b) {
    return strcasecmp(*(const char * const *)a, *(const char * const *)b);
}

static void free_crumb_textures(void) {
    for (int i = 0; i < g_crumb_count; i++) {
        if (g_crumb_tex[i] != NULL) { window_destroy_texture(g_crumb_tex[i]); g_crumb_tex[i] = NULL; }
    }
}

static void rebuild_breadcrumb(void) {
    free_crumb_textures();
    g_crumb_count = 0;

    strncpy(g_crumb_label[0], g_root_label, sizeof(g_crumb_label[0]) - 1);
    g_crumb_label[0][sizeof(g_crumb_label[0]) - 1] = '\0';
    strncpy(g_crumb_path[0], g_root_path, sizeof(g_crumb_path[0]) - 1);
    g_crumb_path[0][sizeof(g_crumb_path[0]) - 1] = '\0';
    g_crumb_count = 1;

    size_t root_len = strlen(g_root_path);
    if (strncmp(g_current_path, g_root_path, root_len) == 0
        && strlen(g_current_path) > root_len) {
        char rel_copy[AB_MAX_PATH];
        strncpy(rel_copy, g_current_path + root_len, sizeof(rel_copy) - 1);
        rel_copy[sizeof(rel_copy) - 1] = '\0';

        char accum[AB_MAX_PATH];
        strncpy(accum, g_root_path, sizeof(accum) - 1);
        accum[sizeof(accum) - 1] = '\0';

        char *save = NULL;
        char *tok = strtok_r(rel_copy, "/", &save);
        while (tok != NULL && g_crumb_count < AB_MAX_CRUMBS) {
            size_t len = strlen(accum);
            snprintf(accum + len, sizeof(accum) - len, "/%s", tok);

            strncpy(g_crumb_label[g_crumb_count], tok, sizeof(g_crumb_label[0]) - 1);
            g_crumb_label[g_crumb_count][sizeof(g_crumb_label[0]) - 1] = '\0';
            strncpy(g_crumb_path[g_crumb_count], accum, sizeof(g_crumb_path[0]) - 1);
            g_crumb_path[g_crumb_count][sizeof(g_crumb_path[0]) - 1] = '\0';
            g_crumb_count++;

            tok = strtok_r(NULL, "/", &save);
        }
    }

    for (int i = 0; i < g_crumb_count; i++) {
        g_crumb_tex[i] = make_text_texture(g_crumb_label[i], FONT_WEIGHT_REGULAR, 13,
                                            &g_crumb_w[i], &g_crumb_h[i]);
    }
}

static void load_directory(const char *path) {
    free_entries();
    g_selected_index = -1;
    g_scroll_offset = 0;

    DIR *d = opendir(path);
    if (d == NULL) {
        strncpy(g_current_path, path, sizeof(g_current_path) - 1);
        g_current_path[sizeof(g_current_path) - 1] = '\0';
        rebuild_breadcrumb();
        return;
    }

    char *dir_names[AB_MAX_ENTRIES];
    char *file_names[AB_MAX_ENTRIES];
    int dir_count = 0, file_count = 0;

    struct dirent *entry;
    while ((entry = readdir(d)) != NULL) {
        if (entry->d_name[0] == '.') continue; /* يخفي الملفات المخفية و . و .. */

        char full[AB_MAX_PATH];
        snprintf(full, sizeof(full), "%s/%s", path, entry->d_name);

        struct stat st;
        if (stat(full, &st) != 0) continue;

        if (S_ISDIR(st.st_mode)) {
            if (dir_count < AB_MAX_ENTRIES) dir_names[dir_count++] = strdup(entry->d_name);
        } else {
            if (g_pick_mode == ASSET_BROWSER_MODE_PICK_FILE
                && !extension_matches_filter(entry->d_name)) {
                continue;
            }
            if (file_count < AB_MAX_ENTRIES) file_names[file_count++] = strdup(entry->d_name);
        }
    }
    closedir(d);

    qsort(dir_names, dir_count, sizeof(char *), compare_strs_ci);
    qsort(file_names, file_count, sizeof(char *), compare_strs_ci);

    /* المجلدات أولاً دائماً، ثم الملفات - كل مجموعة أبجدياً بلا حساسية لحالة الأحرف */
    for (int i = 0; i < dir_count; i++) {
        char full[AB_MAX_PATH];
        snprintf(full, sizeof(full), "%s/%s", path, dir_names[i]);
        add_entry(full, dir_names[i], 1);
        free(dir_names[i]);
    }
    for (int i = 0; i < file_count; i++) {
        char full[AB_MAX_PATH];
        snprintf(full, sizeof(full), "%s/%s", path, file_names[i]);
        add_entry(full, file_names[i], 0);
        free(file_names[i]);
    }

    strncpy(g_current_path, path, sizeof(g_current_path) - 1);
    g_current_path[sizeof(g_current_path) - 1] = '\0';
    rebuild_breadcrumb();
}

/* ------------------------------------------------------------
 * تهيئة كسولة (مرة واحدة فقط، أول استدعاء فعلي)
 * ------------------------------------------------------------ */
static void ensure_ready(void) {
    if (g_ready) return;
    g_ready = 1;

    font_init();
    icon_atlas_init();
    text_field_init(&g_search_field);
    text_field_init(&g_filename_field);
    g_filename_label_tex = make_text_texture("File name:", FONT_WEIGHT_REGULAR, 13,
                                              &g_filename_label_w, &g_filename_label_h);

    make_button("Cancel", &g_cancel_btn, &g_cancel_btn_tex, &g_cancel_txt_tex);
    make_button("Select", &g_select_btn, &g_select_btn_tex, &g_select_txt_tex);

    g_crumb_sep_tex = make_text_texture("/", FONT_WEIGHT_REGULAR, 13, &g_crumb_sep_w, &g_crumb_sep_h);

    /* خلفية زر الإغلاق X - مربع صغير بلون أحمر خافت (يبين واضح إنه
     * "إغلاق" بدون أي أيقونة مخصصة - لا توجد أيقونة X جاهزة بأطلس
     * أيقونات المحرك حالياً، فالحل النظيف هو حرف X نصي بدل صورة) */
    shape_image_t close_shape = shape_provider_render_rect(CLOSE_BTN_SIZE, CLOSE_BTN_SIZE, 5, 90, 40, 40);
    g_close_btn_tex = window_create_texture(close_shape.pixels, close_shape.width, close_shape.height);
    shape_provider_free_image(&close_shape);
    g_close_x_tex = make_text_texture("X", FONT_WEIGHT_BOLD, 14, &g_close_x_w, &g_close_x_h);

    shape_image_t hov = shape_provider_render_rect(CELL_W - 4, CELL_H - 4, CELL_CORNER_RADIUS,
        UI_COLOR_BUTTON_BLUE.r, UI_COLOR_BUTTON_BLUE.g, UI_COLOR_BUTTON_BLUE.b);
    /* تخفيف الشفافية يدوياً (بدون دالة عامة بالمشروع لهذا - نفس
     * أسلوب add_node_dialog.c بالضبط لتظليل الصف المحوَّم عليه) */
    for (int i = 0; i < hov.width * hov.height; i++) hov.pixels[i * 4 + 3] = (unsigned char)(hov.pixels[i * 4 + 3] * 0.25);
    g_hover_tex = window_create_texture(hov.pixels, hov.width, hov.height);
    shape_provider_free_image(&hov);

    shape_image_t sel = shape_provider_render_rect(CELL_W - 4, CELL_H - 4, CELL_CORNER_RADIUS,
        UI_COLOR_BUTTON_BLUE.r, UI_COLOR_BUTTON_BLUE.g, UI_COLOR_BUTTON_BLUE.b);
    for (int i = 0; i < sel.width * sel.height; i++) sel.pixels[i * 4 + 3] = (unsigned char)(sel.pixels[i * 4 + 3] * 0.6);
    g_selected_tex = window_create_texture(sel.pixels, sel.width, sel.height);
    shape_provider_free_image(&sel);

    /* خلفية معتمة شبه شفافة خلف النافذة المنبثقة - تُنشأ مرة واحدة
     * بنسيج صغير 4×4 فقط (كل بكسل أسود بشفافية ~55%)، ثم تُمدَّد وقت
     * الرسم لتغطي كامل مقاس النافذة الحالي مهما كان (window_draw_texture
     * يمدّد أي نسيج لأي حجم مطلوب) - أرخص بكثير من إعادة إنشاء نسيج
     * بحجم الشاشة الفعلي كل إطار، ويعطي شفافية حقيقية (بعكس
     * window_fill_rect اللي دائماً معتمة 100% بلا أي دعم شفافية -
     * السبب: window_create_texture يفعّل SDL_BLENDMODE_BLEND تلقائياً
     * على أي نسيج، بينما window_fill_rect يرسم مباشرة بألفا 255 ثابتة) */
    unsigned char dim_pixels[4 * 4 * 4];
    for (int i = 0; i < 4 * 4; i++) {
        dim_pixels[i * 4 + 0] = 8;
        dim_pixels[i * 4 + 1] = 8;
        dim_pixels[i * 4 + 2] = 8;
        dim_pixels[i * 4 + 3] = 140;
    }
    g_dim_overlay_tex = window_create_texture(dim_pixels, 4, 4);
}

/* ------------------------------------------------------------
 * فتح النافذة (الواجهة العامة)
 * ------------------------------------------------------------ */
/* الجزء المشترك بين asset_browser_open وasset_browser_open_save -
 * إعداد الجذر (Assets:// أو الجهاز) وتحميل أول مجلد وتصفير شريط
 * البحث. يرجع 0 لو فشل الإعداد (Assets بلا مشروع مفتوح - callback
 * استُدعيت بـNULL بالفعل داخلها، المستدعي يوقف فوراً وقتها بلا أي
 * شيء إضافي)، و1 لو نجح (النافذة صارت مفتوحة فعلياً) */
static int open_common(asset_browser_root_t root, asset_browser_callback_t callback, void *user_data) {
    ensure_ready();

    g_root_mode = root;
    g_callback = callback;
    g_user_data = user_data;

    if (root == ASSET_BROWSER_ROOT_ASSETS) {
        const char *project_path = current_project_get_path();
        if (project_path == NULL) {
            /* ما فيه مشروع مفتوح - ما فيه Assets:// نتصفحها إطلاقاً،
             * نُبلغ المستدعي فوراً بدل ما نفتح نافذة على مجلد وهمي */
            if (callback != NULL) callback(NULL, user_data);
            return 0;
        }
        snprintf(g_root_path, sizeof(g_root_path), "%s/Assets", project_path);
        struct stat st;
        if (stat(g_root_path, &st) != 0) mkdir(g_root_path, 0755);
        strncpy(g_root_label, "Assets://", sizeof(g_root_label) - 1);
        g_root_label[sizeof(g_root_label) - 1] = '\0';
    } else {
        /* الجذر الحقيقي (حد التصفح الأعلى) دائماً "/" بلا أي قيد -
         * لكن نقطة البداية الفعلية تُختار بذكاء حسب الجهاز، بدل ما
         * نفتح المستخدم مباشرة على جذر النظام الفارغ عملياً: لو
         * الجهاز أندرويد (تشغيل داخل Termux:X11 كما هو حال المحرك
         * حالياً) فمساحة التخزين المتاحة للمستخدم فعلياً تكون بمسار
         * /storage/emulated/0 وليس "/" - نتحقق من وجوده فعلاً ونبدأ
         * منه لو موجوداً، وإلا $HOME، وإلا نكتفي بالجذر نفسه. المستخدم
         * يقدر يتصفح لأي مكان ثانٍ بالجهاز بعدها عادي عبر شريط المسار
         * (زر بداية المسار "/" دائماً متاح، بلا أي قفل فعلي) */
        strncpy(g_root_path, "/", sizeof(g_root_path) - 1);
        g_root_path[sizeof(g_root_path) - 1] = '\0';
        strncpy(g_root_label, "/", sizeof(g_root_label) - 1);
        g_root_label[sizeof(g_root_label) - 1] = '\0';
    }

    g_search_field.text[0] = '\0';
    g_search_field.cursor_pos = 0;
    g_search_field.scroll_offset = 0;

    if (root == ASSET_BROWSER_ROOT_DEVICE) {
        struct stat st;
        const char *home = getenv("HOME");
        if (stat("/storage/emulated/0", &st) == 0 && S_ISDIR(st.st_mode)) {
            load_directory("/storage/emulated/0");
        } else if (home != NULL && stat(home, &st) == 0 && S_ISDIR(st.st_mode)) {
            load_directory(home);
        } else {
            load_directory(g_root_path);
        }
    } else {
        load_directory(g_root_path);
    }

    g_is_open = 1;
    return 1;
}

void asset_browser_open(asset_browser_root_t root, asset_browser_mode_t mode,
                         const char **extensions, int extension_count,
                         asset_browser_callback_t callback, void *user_data) {
    g_pick_mode = mode;

    g_extension_count = 0;
    if (mode == ASSET_BROWSER_MODE_PICK_FILE) {
        for (int i = 0; i < extension_count && i < AB_MAX_EXTS; i++) {
            strncpy(g_extensions[g_extension_count], extensions[i], sizeof(g_extensions[0]) - 1);
            g_extensions[g_extension_count][sizeof(g_extensions[0]) - 1] = '\0';
            g_extension_count++;
        }
    }

    if (!open_common(root, callback, user_data)) return;

    if (g_title_tex != NULL) { window_destroy_texture(g_title_tex); g_title_tex = NULL; }
    g_title_tex = make_text_texture(mode == ASSET_BROWSER_MODE_PICK_FOLDER ? "Select Folder" : "Select File",
                                     FONT_WEIGHT_BOLD, 17, &g_title_w, &g_title_h);

    if (g_select_btn_tex != NULL) { window_destroy_texture(g_select_btn_tex); g_select_btn_tex = NULL; }
    if (g_select_txt_tex != NULL) { window_destroy_texture(g_select_txt_tex); g_select_txt_tex = NULL; }
    make_button("Select", &g_select_btn, &g_select_btn_tex, &g_select_txt_tex);

    if (g_filter_label_tex != NULL) { window_destroy_texture(g_filter_label_tex); g_filter_label_tex = NULL; }
    if (g_extension_count > 0) {
        char joined[160] = "";
        for (int i = 0; i < g_extension_count; i++) {
            char part[32];
            snprintf(part, sizeof(part), "%s*.%s", (i > 0) ? "  " : "", g_extensions[i]);
            strncat(joined, part, sizeof(joined) - strlen(joined) - 1);
        }
        g_filter_label_tex = make_text_texture(joined, FONT_WEIGHT_REGULAR, 12,
                                                &g_filter_label_w, &g_filter_label_h);
    }
}

void asset_browser_open_save(asset_browser_root_t root, const char *default_filename,
                              asset_browser_callback_t callback, void *user_data) {
    g_pick_mode = ASSET_BROWSER_MODE_SAVE_FILE;
    g_extension_count = 0; /* بلا فلترة - كل الملفات تُعرض للسياق بس، بنفس سلوك PICK_FOLDER بالضبط */

    if (!open_common(root, callback, user_data)) return;

    if (g_title_tex != NULL) { window_destroy_texture(g_title_tex); g_title_tex = NULL; }
    g_title_tex = make_text_texture("Save As", FONT_WEIGHT_BOLD, 17, &g_title_w, &g_title_h);

    if (g_select_btn_tex != NULL) { window_destroy_texture(g_select_btn_tex); g_select_btn_tex = NULL; }
    if (g_select_txt_tex != NULL) { window_destroy_texture(g_select_txt_tex); g_select_txt_tex = NULL; }
    make_button("Save", &g_select_btn, &g_select_btn_tex, &g_select_txt_tex);

    if (g_filter_label_tex != NULL) { window_destroy_texture(g_filter_label_tex); g_filter_label_tex = NULL; }

    g_filename_field.text[0] = '\0';
    if (default_filename != NULL) {
        strncpy(g_filename_field.text, default_filename, TEXT_FIELD_MAX_LEN - 1);
        g_filename_field.text[TEXT_FIELD_MAX_LEN - 1] = '\0';
    }
    g_filename_field.cursor_pos = (int)strlen(g_filename_field.text);
    g_filename_field.scroll_offset = 0;
}

int asset_browser_is_open(void) { return g_is_open; }

/* ------------------------------------------------------------
 * تحديث (منطق + تفاعل)
 * ------------------------------------------------------------ */
static void finish(const char *picked_path) {
    asset_browser_callback_t cb = g_callback;
    void *ud = g_user_data;
    g_is_open = 0;
    g_callback = NULL;
    g_user_data = NULL;
    window_stop_text_input();
    if (cb != NULL) cb(picked_path, ud);
}

void asset_browser_update(int window_w, int window_h) {
    if (!g_is_open) return;

    int dx = (window_w - DIALOG_WIDTH) / 2;
    int dy = (window_h - DIALOG_HEIGHT) / 2;

    int mx = window_mouse_x(), my = window_mouse_y();

    /* --- شريط العنوان + زر الإغلاق --- */
    int close_x = dx + DIALOG_WIDTH - DIALOG_PADDING - CLOSE_BTN_SIZE;
    int close_y = dy + (TITLEBAR_HEIGHT - CLOSE_BTN_SIZE) / 2;
    if (window_mouse_left_just_pressed()
        && mx >= close_x && mx < close_x + CLOSE_BTN_SIZE
        && my >= close_y && my < close_y + CLOSE_BTN_SIZE) {
        finish(NULL);
        return;
    }

    /* --- شريط المسار (Breadcrumb) - محسوب هنا، يُعاد استخدامه بالرسم --- */
    int crumb_y = dy + TITLEBAR_HEIGHT;
    int crumb_x = dx + DIALOG_PADDING;
    for (int i = 0; i < g_crumb_count; i++) {
        g_crumb_x[i] = crumb_x;
        g_crumb_w_click[i] = g_crumb_w[i];
        crumb_x += g_crumb_w[i];
        if (i < g_crumb_count - 1) crumb_x += g_crumb_sep_w + 10;
    }
    if (window_mouse_left_just_pressed() && my >= crumb_y && my < crumb_y + CRUMB_BAR_HEIGHT) {
        for (int i = 0; i < g_crumb_count - 1; i++) { /* آخر عنصر هو المجلد الحالي نفسه - لا داعي لضغطه */
            if (mx >= g_crumb_x[i] && mx < g_crumb_x[i] + g_crumb_w_click[i]) {
                load_directory(g_crumb_path[i]);
                break;
            }
        }
    }

    /* --- شريط البحث --- */
    int toolbar_y = crumb_y + CRUMB_BAR_HEIGHT;
    int search_x = dx + DIALOG_PADDING;
    int search_w = (DIALOG_WIDTH - DIALOG_PADDING * 2) / 2;
    text_field_update(&g_search_field, search_x + 20, toolbar_y, search_w - 20, TOOLBAR_HEIGHT - 6, search_w - 20);

    if (g_search_field.focused) window_start_text_input(); else window_stop_text_input();

    /* --- منطقة الشبكة --- */
    int grid_x = dx + DIALOG_PADDING;
    int grid_y = toolbar_y + TOOLBAR_HEIGHT + GRID_TOP_GAP;
    int grid_w = DIALOG_WIDTH - DIALOG_PADDING * 2;
    int filename_row_reserve = (g_pick_mode == ASSET_BROWSER_MODE_SAVE_FILE) ? FILENAME_ROW_HEIGHT : 0;
    int grid_h = DIALOG_HEIGHT - (grid_y - dy) - FOOTER_HEIGHT - DIALOG_PADDING - filename_row_reserve;

    int columns = grid_w / CELL_W;
    if (columns < 1) columns = 1;

    int visible_count = 0;
    for (int i = 0; i < g_entry_count; i++) {
        if (matches_search(g_entries[i].display_name)) visible_count++;
    }
    int total_rows = (visible_count + columns - 1) / columns;
    int content_h = total_rows * CELL_H;
    int max_scroll = content_h - grid_h;
    if (max_scroll < 0) max_scroll = 0;

    if (mx >= grid_x && mx < grid_x + grid_w && my >= grid_y && my < grid_y + grid_h) {
        g_scroll_offset -= window_mouse_wheel_delta() * SCROLL_STEP_PX;
    }
    if (g_scroll_offset < 0) g_scroll_offset = 0;
    if (g_scroll_offset > max_scroll) g_scroll_offset = max_scroll;

    int visible_index = 0;
    int clicked_grid = 0;
    for (int i = 0; i < g_entry_count && !clicked_grid; i++) {
        ab_entry_t *e = &g_entries[i];
        if (!matches_search(e->display_name)) continue;

        int col = visible_index % columns;
        int row = visible_index / columns;
        int cell_x = grid_x + col * CELL_W;
        int cell_y = grid_y + row * CELL_H - g_scroll_offset;
        visible_index++;

        if (cell_y + CELL_H < grid_y || cell_y > grid_y + grid_h) continue; /* خارج منطقة الرؤية - لا داعي لفحص الضغط */

        int inside = (mx >= cell_x && mx < cell_x + CELL_W && my >= cell_y && my < cell_y + CELL_H
                      && my >= grid_y && my < grid_y + grid_h);
        if (inside && window_mouse_left_just_pressed()) {
            if (e->is_dir) {
                g_search_field.text[0] = '\0';
                g_search_field.cursor_pos = 0;
                g_search_field.scroll_offset = 0;
                load_directory(e->full_path);
                clicked_grid = 1;
            } else if (g_pick_mode == ASSET_BROWSER_MODE_PICK_FILE) {
                g_selected_index = i;
                clicked_grid = 1;
            }
            /* بوضع PICK_FOLDER: الضغط على ملف لا يفعل شيئاً - الملفات
             * تُعرض للسياق فقط، غير قابلة للاختيار بهذا الوضع */
        }
    }

    /* --- تحديد الاختيار النهائي الحالي (لتفعيل/تعطيل زر Select/Save) --- */
    int can_select = 0;
    char final_path[AB_MAX_PATH];
    final_path[0] = '\0';
    if (g_pick_mode == ASSET_BROWSER_MODE_SAVE_FILE) {
        /* صف اسم الملف - أسفل الشبكة مباشرة، فوق صف الأزرار */
        int buttons_y_early = dy + DIALOG_HEIGHT - DIALOG_PADDING - g_cancel_btn.height;
        int filename_row_y = buttons_y_early - FILENAME_ROW_HEIGHT;
        int field_label_w = 70; /* عرض تقريبي كافٍ لتسمية "File name:" بخط 13 */
        int field_x = grid_x + field_label_w;
        int field_y = filename_row_y + (FILENAME_ROW_HEIGHT - 22) / 2;
        int field_w = grid_w - field_label_w;

        int was_focused = g_filename_field.focused;
        text_field_update(&g_filename_field, field_x, field_y, field_w, 22, field_w);
        if (g_filename_field.focused != was_focused) {
            if (g_filename_field.focused) window_start_text_input(); else window_stop_text_input();
        }

        if (g_filename_field.text[0] != '\0') {
            can_select = 1;
            int written = snprintf(final_path, sizeof(final_path), "%s/%s", g_current_path, g_filename_field.text);
            if (written < 0 || written >= (int)sizeof(final_path)) {
                final_path[sizeof(final_path) - 1] = '\0'; /* قص آمن - snprintf يضمنه أصلاً، هذا يوثّق الوعي بالحالة لتسكيت تحذير GCC الوقائي */
            }
        }
    } else if (g_pick_mode == ASSET_BROWSER_MODE_PICK_FOLDER) {
        can_select = 1;
        strncpy(final_path, g_current_path, sizeof(final_path) - 1);
    } else if (g_selected_index >= 0 && g_selected_index < g_entry_count
               && !g_entries[g_selected_index].is_dir) {
        can_select = 1;
        strncpy(final_path, g_entries[g_selected_index].full_path, sizeof(final_path) - 1);
    }

    /* --- أزرار Cancel / Select --- */
    int buttons_y = dy + DIALOG_HEIGHT - DIALOG_PADDING - g_cancel_btn.height;
    int select_x = dx + DIALOG_WIDTH - DIALOG_PADDING - g_select_btn.width;
    int cancel_x = select_x - 8 - g_cancel_btn.width;

    if (window_mouse_left_just_pressed()) {
        if (mx >= cancel_x && mx < cancel_x + g_cancel_btn.width
            && my >= buttons_y && my < buttons_y + g_cancel_btn.height) {
            finish(NULL);
            return;
        }
        if (can_select && mx >= select_x && mx < select_x + g_select_btn.width
            && my >= buttons_y && my < buttons_y + g_select_btn.height) {
            finish(final_path);
            return;
        }
    }
}

/* ------------------------------------------------------------
 * رسم
 * ------------------------------------------------------------ */
void asset_browser_draw(int window_w, int window_h) {
    if (!g_is_open) return;

    int dx = (window_w - DIALOG_WIDTH) / 2;
    int dy = (window_h - DIALOG_HEIGHT) / 2;

    /* خلفية شبه شفافة خلف كامل نافذة البرنامج - تُبرز النافذة المنبثقة
     * كطبقة فوقية واضحة (لمسة احترافية غير موجودة بنوافذ المشروع
     * المنبثقة الأخرى حالياً - راجع تعليق إنشاء g_dim_overlay_tex
     * بـensure_ready لتفصيل السبب التقني) */
    if (g_dim_overlay_tex != NULL) {
        window_draw_texture(g_dim_overlay_tex, 0, 0, window_w, window_h);
    }

    window_fill_rect(dx, dy, DIALOG_WIDTH, DIALOG_HEIGHT,
                      UI_COLOR_BLACK_MUTED.r, UI_COLOR_BLACK_MUTED.g, UI_COLOR_BLACK_MUTED.b);

    /* --- شريط العنوان --- */
    if (g_title_tex != NULL) {
        window_draw_texture(g_title_tex, dx + DIALOG_PADDING, dy + (TITLEBAR_HEIGHT - g_title_h) / 2,
                             g_title_w, g_title_h);
    }
    int close_x = dx + DIALOG_WIDTH - DIALOG_PADDING - CLOSE_BTN_SIZE;
    int close_y = dy + (TITLEBAR_HEIGHT - CLOSE_BTN_SIZE) / 2;
    if (g_close_btn_tex != NULL) window_draw_texture(g_close_btn_tex, close_x, close_y, CLOSE_BTN_SIZE, CLOSE_BTN_SIZE);
    if (g_close_x_tex != NULL) {
        window_draw_texture(g_close_x_tex,
                             close_x + (CLOSE_BTN_SIZE - g_close_x_w) / 2,
                             close_y + (CLOSE_BTN_SIZE - g_close_x_h) / 2,
                             g_close_x_w, g_close_x_h);
    }

    window_fill_rect(dx + DIALOG_PADDING, dy + TITLEBAR_HEIGHT, DIALOG_WIDTH - DIALOG_PADDING * 2, 1, 60, 60, 60);

    /* --- شريط المسار --- */
    int crumb_y = dy + TITLEBAR_HEIGHT;
    for (int i = 0; i < g_crumb_count; i++) {
        int is_last = (i == g_crumb_count - 1);
        unsigned char cr = is_last ? 235 : 150, cg = is_last ? 235 : 150, cb = is_last ? 235 : 150;
        if (g_crumb_tex[i] != NULL) {
            window_draw_texture_tinted(g_crumb_tex[i], g_crumb_x[i], crumb_y + (CRUMB_BAR_HEIGHT - g_crumb_h[i]) / 2,
                                        g_crumb_w[i], g_crumb_h[i], cr, cg, cb);
        }
        if (!is_last && g_crumb_sep_tex != NULL) {
            window_draw_texture(g_crumb_sep_tex, g_crumb_x[i] + g_crumb_w[i] + 4,
                                 crumb_y + (CRUMB_BAR_HEIGHT - g_crumb_sep_h) / 2, g_crumb_sep_w, g_crumb_sep_h);
        }
    }

    /* --- شريط البحث + عداد النتائج/الفلتر --- */
    int toolbar_y = crumb_y + CRUMB_BAR_HEIGHT;
    int search_x = dx + DIALOG_PADDING;
    int search_w = (DIALOG_WIDTH - DIALOG_PADDING * 2) / 2;
    icon_atlas_draw(ICON_search, search_x, toolbar_y + (TOOLBAR_HEIGHT - 6 - 16) / 2, 16);
    text_field_draw(&g_search_field, search_x + 20, toolbar_y, search_w - 20, TOOLBAR_HEIGHT - 6, 13);

    if (g_filter_label_tex != NULL) {
        int fx = dx + DIALOG_WIDTH - DIALOG_PADDING - g_filter_label_w;
        window_draw_texture_tinted(g_filter_label_tex, fx, toolbar_y + (TOOLBAR_HEIGHT - g_filter_label_h) / 2,
                                    g_filter_label_w, g_filter_label_h, 150, 150, 150);
    }

    /* --- منطقة الشبكة (مقصوصة عند حوافها فلا يطفح أي محتوى خارجها) --- */
    int grid_x = dx + DIALOG_PADDING;
    int grid_y = toolbar_y + TOOLBAR_HEIGHT + GRID_TOP_GAP;
    int grid_w = DIALOG_WIDTH - DIALOG_PADDING * 2;
    int filename_row_reserve = (g_pick_mode == ASSET_BROWSER_MODE_SAVE_FILE) ? FILENAME_ROW_HEIGHT : 0;
    int grid_h = DIALOG_HEIGHT - (grid_y - dy) - FOOTER_HEIGHT - DIALOG_PADDING - filename_row_reserve;

    int columns = grid_w / CELL_W;
    if (columns < 1) columns = 1;

    window_set_clip_rect(grid_x, grid_y, grid_w, grid_h);

    int mx = window_mouse_x(), my = window_mouse_y();
    int visible_index = 0;
    for (int i = 0; i < g_entry_count; i++) {
        ab_entry_t *e = &g_entries[i];
        if (!matches_search(e->display_name)) continue;

        int col = visible_index % columns;
        int row = visible_index / columns;
        int cell_x = grid_x + col * CELL_W;
        int cell_y = grid_y + row * CELL_H - g_scroll_offset;
        visible_index++;

        if (cell_y + CELL_H < grid_y || cell_y > grid_y + grid_h) continue;

        int is_dim = (!e->is_dir && g_pick_mode == ASSET_BROWSER_MODE_PICK_FOLDER);
        int is_hover = (mx >= cell_x && mx < cell_x + CELL_W && my >= cell_y && my < cell_y + CELL_H
                         && my >= grid_y && my < grid_y + grid_h);
        int is_selected = (!e->is_dir && i == g_selected_index);

        if (is_selected && g_selected_tex != NULL) {
            window_draw_texture(g_selected_tex, cell_x + 2, cell_y + 2, CELL_W - 4, CELL_H - 4);
        } else if (is_hover && !is_dim && g_hover_tex != NULL) {
            window_draw_texture(g_hover_tex, cell_x + 2, cell_y + 2, CELL_W - 4, CELL_H - 4);
        }

        int icon_area_x = cell_x + (CELL_W - GRID_ICON_SIZE) / 2;
        int icon_area_y = cell_y + 8;
        unsigned char tint = is_dim ? 100 : 255;

        if (e->is_dir) {
            icon_atlas_draw_tinted(ICON_Folder, icon_area_x, icon_area_y, GRID_ICON_SIZE, 230, 200, 60);
        } else if (e->thumb_tex != NULL) {
            /* صورة مصغّرة حقيقية - تُرسم بمنتصف مربع الأيقونة محافظة على نسبتها الأصلية */
            int tw = e->thumb_w, th = e->thumb_h;
            int tx = icon_area_x + (GRID_ICON_SIZE - tw) / 2;
            int ty = icon_area_y + (GRID_ICON_SIZE - th) / 2;
            if (is_dim) {
                window_draw_texture_tinted(e->thumb_tex, tx, ty, tw, th, tint, tint, tint);
            } else {
                window_draw_texture(e->thumb_tex, tx, ty, tw, th);
            }
        } else {
            int icon_id = icon_for_extension(e->display_name);
            if (is_dim) {
                icon_atlas_draw_tinted(icon_id, icon_area_x, icon_area_y, GRID_ICON_SIZE, tint, tint, tint);
            } else {
                icon_atlas_draw(icon_id, icon_area_x, icon_area_y, GRID_ICON_SIZE);
            }
        }

        if (e->label_tex != NULL) {
            int label_x = cell_x + (CELL_W - e->label_w) / 2;
            int label_y = icon_area_y + GRID_ICON_SIZE + 6;
            window_set_clip_rect(cell_x + 2, label_y, CELL_W - 4, LABEL_FONT_SIZE + 4);
            if (is_dim) {
                window_draw_texture_tinted(e->label_tex, label_x, label_y, e->label_w, e->label_h, tint, tint, tint);
            } else {
                window_draw_texture(e->label_tex, label_x, label_y, e->label_w, e->label_h);
            }
            window_set_clip_rect(grid_x, grid_y, grid_w, grid_h); /* استرجاع قص منطقة الشبكة الكاملة بعد قص الاسم الفرعي */
        }
    }

    window_clear_clip_rect();

    /* --- تذييل: أزرار Cancel / Select/Save --- */
    int can_select = 0;
    if (g_pick_mode == ASSET_BROWSER_MODE_SAVE_FILE) {
        can_select = (g_filename_field.text[0] != '\0');
    } else if (g_pick_mode == ASSET_BROWSER_MODE_PICK_FOLDER) {
        can_select = 1;
    } else if (g_selected_index >= 0 && g_selected_index < g_entry_count
               && !g_entries[g_selected_index].is_dir) {
        can_select = 1;
    }

    int buttons_y = dy + DIALOG_HEIGHT - DIALOG_PADDING - g_cancel_btn.height;

    if (g_pick_mode == ASSET_BROWSER_MODE_SAVE_FILE) {
        int filename_row_y = buttons_y - FILENAME_ROW_HEIGHT;
        int field_label_w = 70;
        int label_x = grid_x;
        int label_y = filename_row_y + (FILENAME_ROW_HEIGHT - g_filename_label_h) / 2;
        if (g_filename_label_tex != NULL) {
            window_draw_texture(g_filename_label_tex, label_x, label_y, g_filename_label_w, g_filename_label_h);
        }
        int field_x = grid_x + field_label_w;
        int field_y = filename_row_y + (FILENAME_ROW_HEIGHT - 22) / 2;
        int field_w = grid_w - field_label_w;
        text_field_draw(&g_filename_field, field_x, field_y, field_w, 22, 13);
    }

    int select_x = dx + DIALOG_WIDTH - DIALOG_PADDING - g_select_btn.width;
    int cancel_x = select_x - 8 - g_cancel_btn.width;

    window_draw_texture(g_cancel_btn_tex, cancel_x, buttons_y, g_cancel_btn.width, g_cancel_btn.height);
    window_draw_texture(g_cancel_txt_tex, cancel_x + g_cancel_btn.text_offset_x, buttons_y + g_cancel_btn.text_offset_y,
                         g_cancel_btn.text_img.width, g_cancel_btn.text_img.height);

    if (can_select) {
        window_draw_texture(g_select_btn_tex, select_x, buttons_y, g_select_btn.width, g_select_btn.height);
        window_draw_texture(g_select_txt_tex, select_x + g_select_btn.text_offset_x, buttons_y + g_select_btn.text_offset_y,
                             g_select_btn.text_img.width, g_select_btn.text_img.height);
    } else {
        /* معطَّل حالياً - نفس الزر لكن بتلوين رمادي خافت بدل توليد
         * نسيج ثانٍ مخصص لحالة التعطيل (window_draw_texture_tinted
         * يكفي تماماً لهذا الغرض) */
        window_draw_texture_tinted(g_select_btn_tex, select_x, buttons_y, g_select_btn.width, g_select_btn.height,
                                    90, 90, 90);
        window_draw_texture_tinted(g_select_txt_tex, select_x + g_select_btn.text_offset_x, buttons_y + g_select_btn.text_offset_y,
                                    g_select_btn.text_img.width, g_select_btn.text_img.height, 150, 150, 150);
    }
}

void asset_browser_shutdown(void) {
    free_entries();
    free_crumb_textures();

    if (g_title_tex != NULL) window_destroy_texture(g_title_tex);
    if (g_filter_label_tex != NULL) window_destroy_texture(g_filter_label_tex);
    if (g_crumb_sep_tex != NULL) window_destroy_texture(g_crumb_sep_tex);
    if (g_close_btn_tex != NULL) window_destroy_texture(g_close_btn_tex);
    if (g_close_x_tex != NULL) window_destroy_texture(g_close_x_tex);
    if (g_hover_tex != NULL) window_destroy_texture(g_hover_tex);
    if (g_selected_tex != NULL) window_destroy_texture(g_selected_tex);
    if (g_dim_overlay_tex != NULL) window_destroy_texture(g_dim_overlay_tex);
    if (g_cancel_btn_tex != NULL) window_destroy_texture(g_cancel_btn_tex);
    if (g_cancel_txt_tex != NULL) window_destroy_texture(g_cancel_txt_tex);
    if (g_select_btn_tex != NULL) window_destroy_texture(g_select_btn_tex);
    if (g_select_txt_tex != NULL) window_destroy_texture(g_select_txt_tex);
    if (g_filename_label_tex != NULL) window_destroy_texture(g_filename_label_tex);

    text_field_free(&g_search_field);
    text_field_free(&g_filename_field);
    labeled_button_free(&g_cancel_btn);
    labeled_button_free(&g_select_btn);

    g_title_tex = NULL;
    g_filter_label_tex = NULL;
    g_crumb_sep_tex = NULL;
    g_close_btn_tex = NULL;
    g_close_x_tex = NULL;
    g_hover_tex = NULL;
    g_selected_tex = NULL;
    g_dim_overlay_tex = NULL;

    g_is_open = 0;
    g_ready = 0;
}
