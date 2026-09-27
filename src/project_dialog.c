/*
 * ============================================================
 * project_dialog.c
 * ============================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#include "project_dialog.h"
#include "window.h"
#include "labeled_button.h"
#include "font.h"
#include "text_field.h"
#include "current_project.h"
#include "project_create.h"
#include "loading_screen.h"
#include "asset_browser.h" /* زر Browse يفتح متصفح الملفات المدمج بدل الاعتماد على أداة نظام خارجية */
#include "ui_project_center.h" /* لأجل ألوان الثيم UI_COLOR_* */

#define DIALOG_WIDTH   420
#define DIALOG_HEIGHT  240
#define DIALOG_PADDING 24
#define FIELD_HEIGHT   28
#define LABEL_FONT_SIZE 15
#define FIELD_FONT_SIZE 16
#define TITLE_FONT_SIZE 18
#define BUTTON_RADIUS 3 /* نفس نصف قطر زر الشاشة الرئيسية بالضبط */

/* ألوان رسالة التحقق من المسار (أخضر = صالح، أحمر = غير صالح) */
static const unsigned char COLOR_VALID_R = 0x5C, COLOR_VALID_G = 0xB8, COLOR_VALID_B = 0x5C;
static const unsigned char COLOR_INVALID_R = 0xD9, COLOR_INVALID_G = 0x5C, COLOR_INVALID_B = 0x5C;

static int g_is_open = 0;
static int g_ready = 0;

static text_field_t g_name_field;
static text_field_t g_path_field;

/* حالة التحقق من المسار المكتوب - تُحدَّث كل إطار أثناء الكتابة */
static int g_path_valid = 0;
static const char *g_path_message = "";

/* تخزين مؤقت لصورة رسالة التحقق - تُعاد فقط لو الرسالة أو صلاحيتها
 * تغيّرت فعلاً عن آخر إطار، مو كل إطار */
static window_texture_t *g_path_msg_tex = NULL;
static const char *g_path_msg_cached_text = NULL;
static int g_path_msg_cached_valid = -1;
static int g_path_msg_w = 0, g_path_msg_h = 0;

/* أزرار جاهزة الحجم تلقائياً (browse / cancel / create) - تُبنى
 * مرة واحدة فقط عند أول فتح للنافذة */
static labeled_button_t  g_browse_btn;
static labeled_button_t  g_cancel_btn;
static labeled_button_t  g_create_btn;
static window_texture_t *g_browse_btn_tex = NULL;
static window_texture_t *g_browse_txt_tex = NULL;
static window_texture_t *g_cancel_btn_tex = NULL;
static window_texture_t *g_cancel_txt_tex = NULL;
static window_texture_t *g_create_btn_tex = NULL;
static window_texture_t *g_create_txt_tex = NULL;

/* عناوين النصوص الثابتة (العنوان + تسميات الحقول) */
static window_texture_t *g_title_tex = NULL;
static int g_title_w = 0, g_title_h = 0;
static window_texture_t *g_label_name_tex = NULL;
static int g_label_name_w = 0, g_label_name_h = 0;
static window_texture_t *g_label_path_tex = NULL;
static int g_label_path_w = 0, g_label_path_h = 0;

/* ينشئ texture من نص جاهز، ويرجع أبعاده - أداة مساعدة داخلية
 * لتبسيط تكرار نفس الخطوات (رسم نص -> رفعه كصورة -> تحرير المؤقت) */
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

/* ينشئ زراً جاهزاً (خلفية + نص) ويرفع صورتيه كـ textures */
static void make_button(const char *text, labeled_button_t *out_btn,
                         window_texture_t **out_btn_tex, window_texture_t **out_txt_tex) {
    *out_btn = labeled_button_create(text, FONT_WEIGHT_REGULAR, LABEL_FONT_SIZE, 14, 6,
                                      BUTTON_RADIUS,
                                      UI_COLOR_BUTTON_BLUE.r, UI_COLOR_BUTTON_BLUE.g, UI_COLOR_BUTTON_BLUE.b);
    if (out_btn->width > 0) {
        *out_btn_tex = window_create_texture(out_btn->shape_img.pixels,
                                              out_btn->shape_img.width, out_btn->shape_img.height);
        *out_txt_tex = window_create_texture(out_btn->text_img.pixels,
                                              out_btn->text_img.width, out_btn->text_img.height);
    }
}

static void ensure_ready(void) {
    if (g_ready) {
        return;
    }
    g_ready = 1;

    if (!font_init()) {
        return;
    }

    text_field_init(&g_name_field);
    text_field_init(&g_path_field);

    g_title_tex = make_text_texture("New Project", FONT_WEIGHT_BOLD, TITLE_FONT_SIZE,
                                     &g_title_w, &g_title_h);
    g_label_name_tex = make_text_texture("Project name", FONT_WEIGHT_REGULAR, LABEL_FONT_SIZE,
                                          &g_label_name_w, &g_label_name_h);
    g_label_path_tex = make_text_texture("Project path", FONT_WEIGHT_REGULAR, LABEL_FONT_SIZE,
                                          &g_label_path_w, &g_label_path_h);

    make_button("Browse", &g_browse_btn, &g_browse_btn_tex, &g_browse_txt_tex);
    make_button("Cancel", &g_cancel_btn, &g_cancel_btn_tex, &g_cancel_txt_tex);
    make_button("Create", &g_create_btn, &g_create_btn_tex, &g_create_txt_tex);
}

void project_dialog_open(void) {
    ensure_ready();
    g_is_open = 1;
}

int project_dialog_is_open(void) {
    return g_is_open;
}

/* يتحقق هل المسار المكتوب صالح لإنشاء مشروع فيه: مسار مطلق (يبدأ
 * بـ /)، ونتراجع للخلف حتى نلقى أول جزء موجود فعلياً على القرص،
 * ونتأكد إن عندنا صلاحية كتابة فيه (المجلدات الناقصة بعده تُنشأ
 * تلقائياً وقت الإنشاء الفعلي، فما يشترط وجودها الآن) */
static int path_is_writable_ancestor(const char *path) {
    if (path == NULL || path[0] != '/') {
        return 0;
    }

    char buf[1024];
    strncpy(buf, path, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    while (1) {
        struct stat st;
        if (stat(buf, &st) == 0 && S_ISDIR(st.st_mode)) {
            return access(buf, W_OK) == 0;
        }
        char *slash = strrchr(buf, '/');
        if (slash == NULL || slash == buf) {
            break;
        }
        *slash = '\0';
    }
    return access("/", W_OK) == 0;
}

/* يحدّث g_path_valid وg_path_message بناءً على محتوى حقل المسار
 * الحالي - يُستدعى كل إطار أثناء فتح النافذة */
static void update_path_validation(void) {
    if (g_path_field.text[0] == '\0') {
        g_path_valid = 0;
        g_path_message = "Enter a project path";
    } else if (g_path_field.text[0] != '/') {
        g_path_valid = 0;
        g_path_message = "Path must be absolute (start with /)";
    } else if (!path_is_writable_ancestor(g_path_field.text)) {
        g_path_valid = 0;
        g_path_message = "Cannot create project here (no permission)";
    } else {
        g_path_valid = 1;
        g_path_message = "Ready - missing folders will be created automatically";
    }
}

/* تُستدعى مرة واحدة من asset_browser عند إغلاق نافذة اختيار المجلد -
 * إما بالمسار المختار (ضغط Select)، أو NULL لو أُلغيت العملية
 * (Cancel أو زر الإغلاق X) - بهذي الحالة لا نغيّر حقل المسار
 * إطلاقاً، يبقى كما كان قبل فتح المتصفح */
static void on_browse_result(const char *path, void *user_data) {
    (void)user_data;
    if (path == NULL) {
        return;
    }
    strncpy(g_path_field.text, path, TEXT_FIELD_MAX_LEN - 1);
    g_path_field.text[TEXT_FIELD_MAX_LEN - 1] = '\0';
    g_path_field.cursor_pos = (int)strlen(g_path_field.text);
    g_path_field.scroll_offset = 0;
    update_path_validation();
}

/* يحسب مستطيل النافذة الحالي (تتوسط نافذة البرنامج دائماً) */
static void get_dialog_rect(int window_w, int window_h, int *x, int *y) {
    *x = (window_w - DIALOG_WIDTH) / 2;
    *y = (window_h - DIALOG_HEIGHT) / 2;
}

void project_dialog_update(int window_w, int window_h) {
    if (!g_is_open) {
        return;
    }

    int dx, dy;
    get_dialog_rect(window_w, window_h, &dx, &dy);

    int field_x = dx + DIALOG_PADDING;
    int field_w = DIALOG_WIDTH - DIALOG_PADDING * 2;

    int name_field_y = dy + 64;
    int path_field_y = dy + 128;

    /* حقل المسار أضيق شوي عشان يترك مكان لزر Browse بجانبه */
    int path_field_w = field_w - g_browse_btn.width - 8;

    text_field_update(&g_name_field, field_x, name_field_y, field_w, FIELD_HEIGHT, field_w);
    text_field_update(&g_path_field, field_x, path_field_y, path_field_w, FIELD_HEIGHT, path_field_w);

    /* تنسيق مركزي لتفعيل/تعطيل نظام الكتابة - مرة واحدة بس، بعد
     * ما الخانتين حدّثوا حالة focused الخاصة بهم. نادي هنا بس،
     * مو داخل كل خانة على حدة، عشان نضمن ترتيب صحيح (ما فيه تفعيل
     * وتعطيل متضاربين بنفس اللحظة عند الانتقال من خانة لثانية) */
    if (g_name_field.focused || g_path_field.focused) {
        window_start_text_input();
    } else {
        window_stop_text_input();
    }

    /* التحقق من صلاحية المسار يُحدَّث كل إطار (يعكس أي كتابة جديدة فوراً) */
    update_path_validation();

    /* زر Browse */
    int browse_x = field_x + path_field_w + 8;
    int browse_y = path_field_y;
    if (window_mouse_left_just_pressed()) {
        int mx = window_mouse_x(), my = window_mouse_y();
        if (mx >= browse_x && mx < browse_x + g_browse_btn.width
            && my >= browse_y && my < browse_y + g_browse_btn.height) {
            /* اختيار مجلد بكامل الجهاز - بلا فلترة امتدادات (لا معنى
             * لها باختيار مجلد أصلاً)، النتيجة تصل عبر on_browse_result */
            asset_browser_open(ASSET_BROWSER_ROOT_DEVICE, ASSET_BROWSER_MODE_PICK_FOLDER,
                                NULL, 0, on_browse_result, NULL);
        }
    }

    int create_x = dx + DIALOG_WIDTH - DIALOG_PADDING - g_create_btn.width;
    int cancel_x = create_x - 8 - g_cancel_btn.width;
    int buttons_y = dy + DIALOG_HEIGHT - DIALOG_PADDING - g_cancel_btn.height;

    /* زر Cancel */
    if (window_mouse_left_just_pressed()) {
        int mx = window_mouse_x(), my = window_mouse_y();
        if (mx >= cancel_x && mx < cancel_x + g_cancel_btn.width
            && my >= buttons_y && my < buttons_y + g_cancel_btn.height) {
            g_is_open = 0;
            g_name_field.focused = 0;
            g_path_field.focused = 0;
            window_stop_text_input();
        }
    }

    /* زر Create - يشتغل فقط لو الاسم مو فارغ والمسار صالح */
    if (window_mouse_left_just_pressed()) {
        int mx = window_mouse_x(), my = window_mouse_y();
        if (mx >= create_x && mx < create_x + g_create_btn.width
            && my >= buttons_y && my < buttons_y + g_create_btn.height) {
            if (g_name_field.text[0] != '\0' && g_path_valid) {
                project_create_result_t result =
                    project_create(g_name_field.text, g_path_field.text);
                if (result == PROJECT_CREATE_OK) {
                    char full_path[600];
                    snprintf(full_path, sizeof(full_path), "%s/%s",
                             g_path_field.text, g_name_field.text);
                    current_project_set_path(full_path);

                    g_is_open = 0;
                    g_name_field.focused = 0;
                    g_path_field.focused = 0;
                    window_stop_text_input();
                    loading_screen_show();
                }
            }
        }
    }
}

void project_dialog_draw(int window_w, int window_h) {
    if (!g_is_open) {
        return;
    }

    int dx, dy;
    get_dialog_rect(window_w, window_h, &dx, &dy);

    /* خلفية النافذة بالكامل - الأسود الباهت (نفس ثيم المحرك) */
    window_fill_rect(dx, dy, DIALOG_WIDTH, DIALOG_HEIGHT,
                      UI_COLOR_BLACK_MUTED.r, UI_COLOR_BLACK_MUTED.g, UI_COLOR_BLACK_MUTED.b);

    /* العنوان في الأعلى، بمنتصف النافذة أفقياً */
    if (g_title_tex != NULL) {
        int tx = dx + (DIALOG_WIDTH - g_title_w) / 2;
        int ty = dy + 16;
        window_draw_texture(g_title_tex, tx, ty, g_title_w, g_title_h);
    }

    int field_x = dx + DIALOG_PADDING;
    int field_w = DIALOG_WIDTH - DIALOG_PADDING * 2;
    int name_field_y = dy + 64;
    int path_field_y = dy + 128;
    int path_field_w = field_w - g_browse_btn.width - 8;

    /* تسمية "Project name" فوق حقلها مباشرة */
    if (g_label_name_tex != NULL) {
        window_draw_texture(g_label_name_tex, field_x, name_field_y - g_label_name_h - 4,
                             g_label_name_w, g_label_name_h);
    }
    text_field_draw(&g_name_field, field_x, name_field_y, field_w, FIELD_HEIGHT, FIELD_FONT_SIZE);

    /* تسمية "Project path" فوق حقلها مباشرة */
    if (g_label_path_tex != NULL) {
        window_draw_texture(g_label_path_tex, field_x, path_field_y - g_label_path_h - 4,
                             g_label_path_w, g_label_path_h);
    }
    text_field_draw(&g_path_field, field_x, path_field_y, path_field_w, FIELD_HEIGHT, FIELD_FONT_SIZE);

    /* زر Browse بجانب حقل المسار */
    int browse_x = field_x + path_field_w + 8;
    int browse_y = path_field_y;
    if (g_browse_btn_tex != NULL) {
        window_draw_texture(g_browse_btn_tex, browse_x, browse_y,
                             g_browse_btn.width, g_browse_btn.height);
        window_draw_texture(g_browse_txt_tex,
                             browse_x + g_browse_btn.text_offset_x,
                             browse_y + g_browse_btn.text_offset_y,
                             g_browse_btn.text_img.width, g_browse_btn.text_img.height);
    }

    /* رسالة التحقق من المسار - تُعاد رسمها فقط لو تغيّرت فعلاً */
    if (g_path_message != g_path_msg_cached_text || g_path_valid != g_path_msg_cached_valid) {
        if (g_path_msg_tex != NULL) {
            window_destroy_texture(g_path_msg_tex);
            g_path_msg_tex = NULL;
        }
        font_text_image_t msg = font_render_text(g_path_message, FONT_WEIGHT_REGULAR, 13);
        if (msg.pixels != NULL) {
            g_path_msg_tex = window_create_texture(msg.pixels, msg.width, msg.height);
            g_path_msg_w = msg.width;
            g_path_msg_h = msg.height;
            font_free_text_image(&msg);
        }
        g_path_msg_cached_text = g_path_message;
        g_path_msg_cached_valid = g_path_valid;
    }
    if (g_path_msg_tex != NULL) {
        unsigned char cr = g_path_valid ? COLOR_VALID_R : COLOR_INVALID_R;
        unsigned char cg = g_path_valid ? COLOR_VALID_G : COLOR_INVALID_G;
        unsigned char cb = g_path_valid ? COLOR_VALID_B : COLOR_INVALID_B;
        window_draw_texture_tinted(g_path_msg_tex, field_x, path_field_y + FIELD_HEIGHT + 4,
                                    g_path_msg_w, g_path_msg_h, cr, cg, cb);
    }

    /* زرا Cancel وCreate أسفل النافذة، بمحاذاة اليمين */
    int buttons_y = dy + DIALOG_HEIGHT - DIALOG_PADDING - g_cancel_btn.height;
    int create_x = dx + DIALOG_WIDTH - DIALOG_PADDING - g_create_btn.width;
    int cancel_x = create_x - 8 - g_cancel_btn.width;

    if (g_cancel_btn_tex != NULL) {
        window_draw_texture(g_cancel_btn_tex, cancel_x, buttons_y,
                             g_cancel_btn.width, g_cancel_btn.height);
        window_draw_texture(g_cancel_txt_tex,
                             cancel_x + g_cancel_btn.text_offset_x,
                             buttons_y + g_cancel_btn.text_offset_y,
                             g_cancel_btn.text_img.width, g_cancel_btn.text_img.height);
    }
    if (g_create_btn_tex != NULL) {
        window_draw_texture(g_create_btn_tex, create_x, buttons_y,
                             g_create_btn.width, g_create_btn.height);
        window_draw_texture(g_create_txt_tex,
                             create_x + g_create_btn.text_offset_x,
                             buttons_y + g_create_btn.text_offset_y,
                             g_create_btn.text_img.width, g_create_btn.text_img.height);
    }
}

void project_dialog_shutdown(void) {
    if (g_title_tex) window_destroy_texture(g_title_tex);
    if (g_label_name_tex) window_destroy_texture(g_label_name_tex);
    if (g_label_path_tex) window_destroy_texture(g_label_path_tex);
    if (g_browse_btn_tex) window_destroy_texture(g_browse_btn_tex);
    if (g_browse_txt_tex) window_destroy_texture(g_browse_txt_tex);
    if (g_cancel_btn_tex) window_destroy_texture(g_cancel_btn_tex);
    if (g_cancel_txt_tex) window_destroy_texture(g_cancel_txt_tex);
    if (g_create_btn_tex) window_destroy_texture(g_create_btn_tex);
    if (g_create_txt_tex) window_destroy_texture(g_create_txt_tex);
    if (g_path_msg_tex) window_destroy_texture(g_path_msg_tex);

    labeled_button_free(&g_browse_btn);
    labeled_button_free(&g_cancel_btn);
    labeled_button_free(&g_create_btn);
    text_field_free(&g_name_field);
    text_field_free(&g_path_field);

    g_is_open = 0;
    g_ready = 0;
}
