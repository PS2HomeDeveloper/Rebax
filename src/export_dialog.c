/*
 * ============================================================
 * export_dialog.c
 * ============================================================
 * راجع export_dialog.h للتوثيق العام. نفس أسلوب project_dialog.c
 * بالضبط (حقول، أزرار، متصفح مجلدات مدمج، زر إغلاق X من asset_browser) -
 * بلا إعادة اختراع أي نمط جديد.
 * ============================================================
 */

#include <stdio.h>
#include <string.h>

#include "export_dialog.h"
#include "window.h"
#include "labeled_button.h"
#include "font.h"
#include "text_field.h"
#include "asset_browser.h"
#include "shape_provider.h"
#include "ui_project_center.h"
#include "ps2_exporter.h"
#include "rebax_paths.h"

#define DIALOG_WIDTH    620
#define DIALOG_HEIGHT   460
#define DIALOG_PADDING  24
#define FIELD_HEIGHT    28
#define LABEL_FONT_SIZE 15
#define FIELD_FONT_SIZE 16
#define TITLE_FONT_SIZE 18
#define BUTTON_RADIUS   3
#define TITLEBAR_HEIGHT 40
#define CLOSE_BTN_SIZE  24
#define RADIO_SIZE      16
#define LOG_VISIBLE_LINES 22
#define LOG_LINE_FONT_SIZE 12

static int g_is_open = 0;
static int g_ready = 0;
static int g_template_prompt = 0;
static int g_template_error = 0;

static text_field_t g_name_field;
static text_field_t g_path_field;

static int g_is_release = 0; /* 0 = Debug (افتراضي)، 1 = Release */

static labeled_button_t  g_browse_btn;
static labeled_button_t  g_cancel_btn;
static labeled_button_t  g_export_btn;
static labeled_button_t  g_template_ok_btn;
static labeled_button_t  g_template_download_btn;
static labeled_button_t  g_template_install_btn;
static window_texture_t *g_browse_btn_tex = NULL;
static window_texture_t *g_browse_txt_tex = NULL;
static window_texture_t *g_cancel_btn_tex = NULL;
static window_texture_t *g_cancel_txt_tex = NULL;
static window_texture_t *g_export_btn_tex = NULL;
static window_texture_t *g_export_txt_tex = NULL;
static window_texture_t *g_template_ok_tex = NULL;
static window_texture_t *g_template_ok_txt_tex = NULL;
static window_texture_t *g_template_download_tex = NULL;
static window_texture_t *g_template_download_txt_tex = NULL;
static window_texture_t *g_template_install_tex = NULL;
static window_texture_t *g_template_install_txt_tex = NULL;

static window_texture_t *g_title_tex = NULL;
static int g_title_w = 0, g_title_h = 0;
static window_texture_t *g_label_name_tex = NULL;
static int g_label_name_w = 0, g_label_name_h = 0;
static window_texture_t *g_label_path_tex = NULL;
static int g_label_path_w = 0, g_label_path_h = 0;
static window_texture_t *g_label_debug_tex = NULL;
static int g_label_debug_w = 0, g_label_debug_h = 0;
static window_texture_t *g_label_release_tex = NULL;
static int g_label_release_w = 0, g_label_release_h = 0;
static window_texture_t *g_template_title_tex = NULL;
static int g_template_title_w = 0, g_template_title_h = 0;
static window_texture_t *g_template_line1_tex = NULL;
static int g_template_line1_w = 0, g_template_line1_h = 0;
static window_texture_t *g_template_line2_tex = NULL;
static int g_template_line2_w = 0, g_template_line2_h = 0;
static window_texture_t *g_template_error_tex = NULL;
static int g_template_error_w = 0, g_template_error_h = 0;

/* زر الإغلاق X - نفس مقاسات/رسم asset_browser بالضبط */
static window_texture_t *g_close_btn_tex = NULL;
static window_texture_t *g_close_x_tex = NULL;
static int g_close_x_w = 0, g_close_x_h = 0;

/* دائرتا الاختيار Debug/Release - غير مملوءة (حلقة) وممتلئة، بلونين
 * (عادي/مُختار) - تُبنى مرة واحدة، الرسم يختار الجاهزة المناسبة */
static window_texture_t *g_radio_ring_tex = NULL;
static window_texture_t *g_radio_filled_tex = NULL;

/* سطور مخرجات التصدير المعروضة حالياً - نسخة محلية من آخر
 * LOG_VISIBLE_LINES سطر وصلت من ps2_exporter (القارئ الحقيقي هناك،
 * هذي بس ذاكرة عرض) */
static char g_log_display[LOG_VISIBLE_LINES][200];
static int g_log_display_count = 0;
static window_texture_t *g_log_line_tex[LOG_VISIBLE_LINES];
static int g_log_line_w[LOG_VISIBLE_LINES];
static int g_log_line_h[LOG_VISIBLE_LINES];
static char g_log_line_cached[LOG_VISIBLE_LINES][200];

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
                         window_texture_t **out_btn_tex, window_texture_t **out_txt_tex,
                         unsigned char r, unsigned char g, unsigned char b) {
    *out_btn = labeled_button_create(text, FONT_WEIGHT_REGULAR, LABEL_FONT_SIZE, 14, 6,
                                      BUTTON_RADIUS, r, g, b);
    if (out_btn->width > 0) {
        *out_btn_tex = window_create_texture(out_btn->shape_img.pixels,
                                              out_btn->shape_img.width, out_btn->shape_img.height);
        *out_txt_tex = window_create_texture(out_btn->text_img.pixels,
                                              out_btn->text_img.width, out_btn->text_img.height);
    }
}

static void ensure_ready(void) {
    if (g_ready) return;
    g_ready = 1;

    if (!font_init()) return;

    text_field_init(&g_name_field);
    text_field_init(&g_path_field);
    strncpy(g_name_field.text, "MyGame", TEXT_FIELD_MAX_LEN - 1);
    g_name_field.cursor_pos = (int)strlen(g_name_field.text);

    g_title_tex = make_text_texture("Export to PlayStation 2", FONT_WEIGHT_BOLD, TITLE_FONT_SIZE,
                                     &g_title_w, &g_title_h);
    g_label_name_tex = make_text_texture("Executable name", FONT_WEIGHT_REGULAR, LABEL_FONT_SIZE,
                                          &g_label_name_w, &g_label_name_h);
    g_label_path_tex = make_text_texture("Export path", FONT_WEIGHT_REGULAR, LABEL_FONT_SIZE,
                                          &g_label_path_w, &g_label_path_h);
    g_label_debug_tex = make_text_texture("Debug", FONT_WEIGHT_REGULAR, LABEL_FONT_SIZE,
                                           &g_label_debug_w, &g_label_debug_h);
    g_label_release_tex = make_text_texture("Release", FONT_WEIGHT_REGULAR, LABEL_FONT_SIZE,
                                             &g_label_release_w, &g_label_release_h);

    make_button("Browse", &g_browse_btn, &g_browse_btn_tex, &g_browse_txt_tex,
                UI_COLOR_BUTTON_BLUE.r, UI_COLOR_BUTTON_BLUE.g, UI_COLOR_BUTTON_BLUE.b);
    make_button("Cancel", &g_cancel_btn, &g_cancel_btn_tex, &g_cancel_txt_tex,
                UI_COLOR_BUTTON_BLUE.r, UI_COLOR_BUTTON_BLUE.g, UI_COLOR_BUTTON_BLUE.b);
    make_button("Export", &g_export_btn, &g_export_btn_tex, &g_export_txt_tex,
                UI_COLOR_BUTTON_BLUE.r, UI_COLOR_BUTTON_BLUE.g, UI_COLOR_BUTTON_BLUE.b);
    make_button("OK", &g_template_ok_btn, &g_template_ok_tex, &g_template_ok_txt_tex,
                UI_COLOR_BUTTON_BLUE.r, UI_COLOR_BUTTON_BLUE.g, UI_COLOR_BUTTON_BLUE.b);
    make_button("Download", &g_template_download_btn, &g_template_download_tex, &g_template_download_txt_tex,
                UI_COLOR_BUTTON_BLUE.r, UI_COLOR_BUTTON_BLUE.g, UI_COLOR_BUTTON_BLUE.b);
    make_button("Install", &g_template_install_btn, &g_template_install_tex, &g_template_install_txt_tex,
                UI_COLOR_BUTTON_BLUE.r, UI_COLOR_BUTTON_BLUE.g, UI_COLOR_BUTTON_BLUE.b);
    g_template_title_tex = make_text_texture("Export Template Required", FONT_WEIGHT_BOLD, TITLE_FONT_SIZE,
                                             &g_template_title_w, &g_template_title_h);
    g_template_line1_tex = make_text_texture("Download the export template before exporting this project.",
                                              FONT_WEIGHT_REGULAR, LABEL_FONT_SIZE,
                                              &g_template_line1_w, &g_template_line1_h);
    g_template_line2_tex = make_text_texture("You can download it or install a file already on this device.",
                                              FONT_WEIGHT_REGULAR, LABEL_FONT_SIZE,
                                              &g_template_line2_w, &g_template_line2_h);

    /* زر الإغلاق X - نفس شكل asset_browser بالضبط (مربع بزوايا
     * مدورة، لون رمادي غامق، حرف X أبيض) */
    shape_image_t close_shape = shape_provider_render_rect(CLOSE_BTN_SIZE, CLOSE_BTN_SIZE, 5, 90, 40, 40);
    g_close_btn_tex = window_create_texture(close_shape.pixels, close_shape.width, close_shape.height);
    shape_provider_free_image(&close_shape);
    g_close_x_tex = make_text_texture("X", FONT_WEIGHT_BOLD, 14, &g_close_x_w, &g_close_x_h);

    /* دائرتا الاختيار - نفس حيلة shape_provider (مستطيل بزاوية
     * استدارة = نصف الحجم يعطي دائرة كاملة). الحلقة (غير مُختار)
     * رمادية باهتة، الممتلئة (مُختار) بلون الثيم الأزرق */
    shape_image_t ring = shape_provider_render_rect(RADIO_SIZE, RADIO_SIZE, RADIO_SIZE / 2, 90, 90, 90);
    g_radio_ring_tex = window_create_texture(ring.pixels, ring.width, ring.height);
    shape_provider_free_image(&ring);

    shape_image_t filled = shape_provider_render_rect(RADIO_SIZE, RADIO_SIZE, RADIO_SIZE / 2,
                                                        UI_COLOR_BUTTON_BLUE.r, UI_COLOR_BUTTON_BLUE.g,
                                                        UI_COLOR_BUTTON_BLUE.b);
    g_radio_filled_tex = window_create_texture(filled.pixels, filled.width, filled.height);
    shape_provider_free_image(&filled);

    for (int i = 0; i < LOG_VISIBLE_LINES; i++) {
        g_log_line_tex[i] = NULL;
        g_log_line_w[i] = 0;
        g_log_line_h[i] = 0;
        g_log_line_cached[i][0] = '\0';
    }
}

void export_dialog_open(void) {
    ensure_ready();
    g_is_open = 1;
}

int export_dialog_is_open(void) {
    return g_is_open;
}

static void get_dialog_rect(int window_w, int window_h, int *out_x, int *out_y) {
    *out_x = (window_w - DIALOG_WIDTH) / 2;
    *out_y = (window_h - DIALOG_HEIGHT) / 2;
}

/* تُستدعى من asset_browser عند إغلاق نافذة اختيار المجلد */
static void on_browse_result(const char *path, void *user_data) {
    (void)user_data;
    if (path == NULL) return;
    strncpy(g_path_field.text, path, TEXT_FIELD_MAX_LEN - 1);
    g_path_field.text[TEXT_FIELD_MAX_LEN - 1] = '\0';
    g_path_field.cursor_pos = (int)strlen(g_path_field.text);
    g_path_field.scroll_offset = 0;
}

static void on_template_archive_selected(const char *path, void *user_data) {
    (void)user_data;
    if (path == NULL) return;
    g_template_error = !rebax_export_template_install(path);
    if (g_template_error) {
        if (g_template_error_tex != NULL) window_destroy_texture(g_template_error_tex);
        g_template_error_tex = make_text_texture("Installation failed. Select a valid export template archive.",
                                                  FONT_WEIGHT_REGULAR, LABEL_FONT_SIZE,
                                                  &g_template_error_w, &g_template_error_h);
    } else {
        g_template_prompt = 0;
    }
}

/* يضيف سطر عرض واحد جاهز (مجزَّأ مسبقاً لو يلزم) لقائمة العرض
 * المنزلقة - نفس منطق التمرير المستخدم بالأصل، مفصول بدالة مستقلة
 * لاستخدامه من التفاف الأسطر الطويلة تحت */
static void push_display_line(const char *text) {
    if (g_log_display_count < LOG_VISIBLE_LINES) {
        strncpy(g_log_display[g_log_display_count], text, 199);
        g_log_display[g_log_display_count][199] = '\0';
        g_log_display_count++;
    } else {
        for (int i = 1; i < LOG_VISIBLE_LINES; i++) {
            strncpy(g_log_display[i - 1], g_log_display[i], 199);
        }
        strncpy(g_log_display[LOG_VISIBLE_LINES - 1], text, 199);
        g_log_display[LOG_VISIBLE_LINES - 1][199] = '\0';
    }
}

/* حد التفاف تقريبي (بالحروف) لعرض نافذة التصدير الحالي بخط السجل -
 * أوامر البناء الطويلة (سطر gcc كامل بكل مسارات -I مثلاً) تلتف على
 * أكثر من سطر عرض بدل ما تفيض عن حافة النافذة */
#define LOG_WRAP_CHARS 84

/* يسحب كل الأسطر الجديدة من ps2_exporter، يلفّ أي سطر أطول من حد
 * النافذة على أكثر من سطر عرض، ويحدّث قائمة العرض المنزلقة (آخر
 * LOG_VISIBLE_LINES سطر عرض - تستمر تنزل لتحت مثل طرفية عادية) */
static void pump_export_log(void) {
    const char *line;
    while ((line = ps2_export_poll_next_line()) != NULL) {
        size_t len = strlen(line);
        if (len <= LOG_WRAP_CHARS) {
            push_display_line(line);
            continue;
        }
        char chunk[LOG_WRAP_CHARS + 1];
        size_t pos = 0;
        while (pos < len) {
            size_t take = len - pos;
            if (take > LOG_WRAP_CHARS) take = LOG_WRAP_CHARS;
            memcpy(chunk, line + pos, take);
            chunk[take] = '\0';
            push_display_line(chunk);
            pos += take;
        }
    }
}

void export_dialog_update(int window_w, int window_h) {
    if (!g_is_open) return;

    /* أهم سطر بهذي الدالة - يُقدِّم آلة حالة التصدير خطوة (يبدأ
     * أوامر الشل، يستطلعها لاحقاً بلا حجب). بدونه التصدير ما يتقدّم
     * إطلاقاً بعد أول استدعاء لـps2_export_start - كان ناقصاً فعلاً */
    ps2_export_update();

    ps2_export_status_t status = ps2_export_get_status();
    int is_running = (status == PS2_EXPORT_STATUS_RUNNING);

    if (status != PS2_EXPORT_STATUS_IDLE) {
        pump_export_log();
    }

    int dx, dy;
    get_dialog_rect(window_w, window_h, &dx, &dy);

    /* زر الإغلاق X - يعمل دائماً، حتى أثناء بناء (يعامَل كإلغاء) */
    int close_x = dx + DIALOG_WIDTH - DIALOG_PADDING - CLOSE_BTN_SIZE + 10;
    int close_y = dy + (TITLEBAR_HEIGHT - CLOSE_BTN_SIZE) / 2;
    if (window_mouse_left_just_pressed()) {
        int mx = window_mouse_x(), my = window_mouse_y();
        if (mx >= close_x && mx < close_x + CLOSE_BTN_SIZE
            && my >= close_y && my < close_y + CLOSE_BTN_SIZE) {
            if (is_running) ps2_export_cancel();
            g_is_open = 0;
            g_name_field.focused = 0;
            g_path_field.focused = 0;
            window_stop_text_input();
            return;
        }
    }

    if (g_template_prompt) {
        int prompt_y = dy + DIALOG_HEIGHT - DIALOG_PADDING - g_template_ok_btn.height;
        int ok_x = dx + DIALOG_WIDTH - DIALOG_PADDING - g_template_ok_btn.width;
        int install_x = ok_x - 8 - g_template_install_btn.width;
        int download_x = install_x - 8 - g_template_download_btn.width;
        if (window_mouse_left_just_pressed()) {
            int mx = window_mouse_x(), my = window_mouse_y();
            if (mx >= ok_x && mx < ok_x + g_template_ok_btn.width
                && my >= prompt_y && my < prompt_y + g_template_ok_btn.height) {
                g_template_prompt = 0;
                g_template_error = 0;
            } else if (mx >= download_x && mx < download_x + g_template_download_btn.width
                       && my >= prompt_y && my < prompt_y + g_template_download_btn.height) {
                rebax_export_template_open_download_url();
            } else if (mx >= install_x && mx < install_x + g_template_install_btn.width
                       && my >= prompt_y && my < prompt_y + g_template_install_btn.height) {
                const char *extensions[] = { "xz" };
                asset_browser_open(ASSET_BROWSER_ROOT_DEVICE, ASSET_BROWSER_MODE_PICK_FILE,
                                   extensions, 1, on_template_archive_selected, NULL);
            }
        }
        return;
    }

    /* أثناء التصدير الفعلي: بلا أي تفاعل ثانٍ (حقول/راديو/تصفح) -
     * فقط انتظار حتى ينجح أو يفشل، أو إلغاء بزر X/Cancel */
    if (is_running) {
        int cancel_x = dx + DIALOG_WIDTH - DIALOG_PADDING - g_cancel_btn.width;
        int buttons_y = dy + DIALOG_HEIGHT - DIALOG_PADDING - g_cancel_btn.height;
        if (window_mouse_left_just_pressed()) {
            int mx = window_mouse_x(), my = window_mouse_y();
            if (mx >= cancel_x && mx < cancel_x + g_cancel_btn.width
                && my >= buttons_y && my < buttons_y + g_cancel_btn.height) {
                ps2_export_cancel();
            }
        }
        return;
    }

    /* نجح أو فشل - بس زر واحد "Close" (نفس مكان Cancel) */
    if (status == PS2_EXPORT_STATUS_SUCCESS || status == PS2_EXPORT_STATUS_FAILED) {
        int close_only_x = dx + DIALOG_WIDTH - DIALOG_PADDING - g_cancel_btn.width;
        int buttons_y = dy + DIALOG_HEIGHT - DIALOG_PADDING - g_cancel_btn.height;
        if (window_mouse_left_just_pressed()) {
            int mx = window_mouse_x(), my = window_mouse_y();
            if (mx >= close_only_x && mx < close_only_x + g_cancel_btn.width
                && my >= buttons_y && my < buttons_y + g_cancel_btn.height) {
                g_is_open = 0;
            }
        }
        return;
    }

    /* IDLE - وضع الإعدادات العادي */
    int field_x = dx + DIALOG_PADDING;
    int field_w = DIALOG_WIDTH - DIALOG_PADDING * 2;
    int name_field_y = dy + TITLEBAR_HEIGHT + 40;
    int path_field_y = dy + TITLEBAR_HEIGHT + 104;
    int path_field_w = field_w - g_browse_btn.width - 8;

    text_field_update(&g_name_field, field_x, name_field_y, field_w, FIELD_HEIGHT, field_w);
    text_field_update(&g_path_field, field_x, path_field_y, path_field_w, FIELD_HEIGHT, path_field_w);

    if (g_name_field.focused || g_path_field.focused) {
        window_start_text_input();
    } else {
        window_stop_text_input();
    }

    /* زر Browse */
    int browse_x = field_x + path_field_w + 8;
    int browse_y = path_field_y;
    if (window_mouse_left_just_pressed()) {
        int mx = window_mouse_x(), my = window_mouse_y();
        if (mx >= browse_x && mx < browse_x + g_browse_btn.width
            && my >= browse_y && my < browse_y + g_browse_btn.height) {
            asset_browser_open(ASSET_BROWSER_ROOT_DEVICE, ASSET_BROWSER_MODE_PICK_FOLDER,
                                NULL, 0, on_browse_result, NULL);
        }
    }

    /* دائرتا Debug/Release - يمين حقل المسار بصف جديد */
    int radio_y = path_field_y + FIELD_HEIGHT + 32;
    int debug_radio_x = field_x;
    int release_radio_x = field_x + 140;

    if (window_mouse_left_just_pressed()) {
        int mx = window_mouse_x(), my = window_mouse_y();
        if (mx >= debug_radio_x && mx < debug_radio_x + RADIO_SIZE + 60
            && my >= radio_y - 4 && my < radio_y + RADIO_SIZE + 4) {
            g_is_release = 0;
        } else if (mx >= release_radio_x && mx < release_radio_x + RADIO_SIZE + 70
                   && my >= radio_y - 4 && my < radio_y + RADIO_SIZE + 4) {
            g_is_release = 1;
        }
    }

    /* زر Cancel (يغلق النافذة بلا تصدير) */
    int cancel_x = dx + DIALOG_WIDTH - DIALOG_PADDING - g_export_btn.width - 8 - g_cancel_btn.width;
    int buttons_y = dy + DIALOG_HEIGHT - DIALOG_PADDING - g_cancel_btn.height;
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

    /* زر Export - يشتغل فقط لو الاسم ومسار التصدير مو فاضيين */
    int export_x = dx + DIALOG_WIDTH - DIALOG_PADDING - g_export_btn.width;
    if (window_mouse_left_just_pressed()) {
        int mx = window_mouse_x(), my = window_mouse_y();
        if (mx >= export_x && mx < export_x + g_export_btn.width
            && my >= buttons_y && my < buttons_y + g_export_btn.height) {
            if (g_name_field.text[0] != '\0' && g_path_field.text[0] != '\0') {
                if (rebax_export_template_is_required()
                    && !rebax_export_template_is_installed()) {
                    g_template_prompt = 1;
                    g_template_error = 0;
                    return;
                }
                g_name_field.focused = 0;
                g_path_field.focused = 0;
                window_stop_text_input();
                g_log_display_count = 0;
                ps2_export_start(g_name_field.text, g_is_release, g_path_field.text);
            }
        }
    }
}

/* يرسم نصاً كسطر مخرجات - يعيد بناء نسيجه فقط لو تغيّر فعلاً (نفس
 * نمط path_msg بـproject_dialog.c، مطبَّق هنا لكل سطر بمصفوفة) */
static void draw_log_line(int index, int x, int y) {
    if (strcmp(g_log_display[index], g_log_line_cached[index]) != 0 || g_log_line_tex[index] == NULL) {
        if (g_log_line_tex[index] != NULL) {
            window_destroy_texture(g_log_line_tex[index]);
            g_log_line_tex[index] = NULL;
        }
        /* سطر فاضٍ (مثلاً سطر فارغ بين خطوات) - font_render_text يرجع
         * صورة بعرض صفر لنص فاضٍ، فنتجنب حتى محاولة إنشاء نسيج منها */
        if (g_log_display[index][0] != '\0') {
            font_text_image_t img = font_render_text(g_log_display[index], FONT_WEIGHT_REGULAR,
                                                       LOG_LINE_FONT_SIZE);
            if (img.pixels != NULL) {
                g_log_line_tex[index] = window_create_texture(img.pixels, img.width, img.height);
                g_log_line_w[index] = img.width;
                g_log_line_h[index] = img.height;
                font_free_text_image(&img);
            }
        } else {
            g_log_line_w[index] = 0;
            g_log_line_h[index] = 0;
        }
        strncpy(g_log_line_cached[index], g_log_display[index], 199);
    }
    if (g_log_line_tex[index] != NULL) {
        /* الخطأ كان هنا بالضبط - كانت 0,0 بدل الأبعاد الحقيقية،
         * فيرسم مستطيلاً بمساحة صفر (بلا أي ظهور) رغم أن النسيج
         * نفسه مبني صح */
        window_draw_texture(g_log_line_tex[index], x, y, g_log_line_w[index], g_log_line_h[index]);
    }
}

void export_dialog_draw(int window_w, int window_h) {
    if (!g_is_open) return;

    int dx, dy;
    get_dialog_rect(window_w, window_h, &dx, &dy);

    window_fill_rect(dx, dy, DIALOG_WIDTH, DIALOG_HEIGHT,
                      UI_COLOR_BLACK_MUTED.r, UI_COLOR_BLACK_MUTED.g, UI_COLOR_BLACK_MUTED.b);

    if (g_title_tex != NULL) {
        window_draw_texture(g_title_tex, dx + DIALOG_PADDING, dy + (TITLEBAR_HEIGHT - g_title_h) / 2,
                             g_title_w, g_title_h);
    }

    int close_x = dx + DIALOG_WIDTH - DIALOG_PADDING - CLOSE_BTN_SIZE + 10;
    int close_y = dy + (TITLEBAR_HEIGHT - CLOSE_BTN_SIZE) / 2;
    if (g_close_btn_tex != NULL) {
        window_draw_texture(g_close_btn_tex, close_x, close_y, CLOSE_BTN_SIZE, CLOSE_BTN_SIZE);
    }
    if (g_close_x_tex != NULL) {
        window_draw_texture(g_close_x_tex,
                             close_x + (CLOSE_BTN_SIZE - g_close_x_w) / 2,
                             close_y + (CLOSE_BTN_SIZE - g_close_x_h) / 2,
                             g_close_x_w, g_close_x_h);
    }

    if (g_template_prompt) {
        int text_x = dx + DIALOG_PADDING;
        int text_y = dy + TITLEBAR_HEIGHT + 42;
        if (g_template_title_tex != NULL)
            window_draw_texture(g_template_title_tex, text_x, text_y,
                                g_template_title_w, g_template_title_h);
        if (g_template_line1_tex != NULL)
            window_draw_texture(g_template_line1_tex, text_x, text_y + 58,
                                g_template_line1_w, g_template_line1_h);
        if (g_template_line2_tex != NULL)
            window_draw_texture(g_template_line2_tex, text_x, text_y + 86,
                                g_template_line2_w, g_template_line2_h);
        if (g_template_error && g_template_error_tex != NULL)
            window_draw_texture(g_template_error_tex, text_x, text_y + 128,
                                g_template_error_w, g_template_error_h);

        int prompt_y = dy + DIALOG_HEIGHT - DIALOG_PADDING - g_template_ok_btn.height;
        int ok_x = dx + DIALOG_WIDTH - DIALOG_PADDING - g_template_ok_btn.width;
        int install_x = ok_x - 8 - g_template_install_btn.width;
        int download_x = install_x - 8 - g_template_download_btn.width;
        if (g_template_download_tex != NULL) {
            window_draw_texture(g_template_download_tex, download_x, prompt_y,
                                g_template_download_btn.width, g_template_download_btn.height);
            window_draw_texture(g_template_download_txt_tex,
                                download_x + g_template_download_btn.text_offset_x,
                                prompt_y + g_template_download_btn.text_offset_y,
                                g_template_download_btn.text_img.width,
                                g_template_download_btn.text_img.height);
        }
        if (g_template_install_tex != NULL) {
            window_draw_texture(g_template_install_tex, install_x, prompt_y,
                                g_template_install_btn.width, g_template_install_btn.height);
            window_draw_texture(g_template_install_txt_tex,
                                install_x + g_template_install_btn.text_offset_x,
                                prompt_y + g_template_install_btn.text_offset_y,
                                g_template_install_btn.text_img.width,
                                g_template_install_btn.text_img.height);
        }
        if (g_template_ok_tex != NULL) {
            window_draw_texture(g_template_ok_tex, ok_x, prompt_y,
                                g_template_ok_btn.width, g_template_ok_btn.height);
            window_draw_texture(g_template_ok_txt_tex,
                                ok_x + g_template_ok_btn.text_offset_x,
                                prompt_y + g_template_ok_btn.text_offset_y,
                                g_template_ok_btn.text_img.width,
                                g_template_ok_btn.text_img.height);
        }
        return;
    }

    ps2_export_status_t status = ps2_export_get_status();

    if (status == PS2_EXPORT_STATUS_IDLE) {
        int field_x = dx + DIALOG_PADDING;
        int field_w = DIALOG_WIDTH - DIALOG_PADDING * 2;
        int name_field_y = dy + TITLEBAR_HEIGHT + 40;
        int path_field_y = dy + TITLEBAR_HEIGHT + 104;
        int path_field_w = field_w - g_browse_btn.width - 8;

        if (g_label_name_tex != NULL) {
            window_draw_texture(g_label_name_tex, field_x, name_field_y - g_label_name_h - 4,
                                 g_label_name_w, g_label_name_h);
        }
        text_field_draw(&g_name_field, field_x, name_field_y, field_w, FIELD_HEIGHT, FIELD_FONT_SIZE);

        if (g_label_path_tex != NULL) {
            window_draw_texture(g_label_path_tex, field_x, path_field_y - g_label_path_h - 4,
                                 g_label_path_w, g_label_path_h);
        }
        text_field_draw(&g_path_field, field_x, path_field_y, path_field_w, FIELD_HEIGHT, FIELD_FONT_SIZE);

        int browse_x = field_x + path_field_w + 8;
        int browse_y = path_field_y;
        if (g_browse_btn_tex != NULL) {
            window_draw_texture(g_browse_btn_tex, browse_x, browse_y, g_browse_btn.width, g_browse_btn.height);
            window_draw_texture(g_browse_txt_tex, browse_x + g_browse_btn.text_offset_x,
                                 browse_y + g_browse_btn.text_offset_y,
                                 g_browse_btn.text_img.width, g_browse_btn.text_img.height);
        }

        /* دائرتا Debug/Release */
        int radio_y = path_field_y + FIELD_HEIGHT + 32;
        int debug_radio_x = field_x;
        int release_radio_x = field_x + 140;

        window_texture_t *debug_tex = (g_is_release == 0) ? g_radio_filled_tex : g_radio_ring_tex;
        window_texture_t *release_tex = (g_is_release == 1) ? g_radio_filled_tex : g_radio_ring_tex;

        if (debug_tex != NULL) window_draw_texture(debug_tex, debug_radio_x, radio_y, RADIO_SIZE, RADIO_SIZE);
        if (g_label_debug_tex != NULL) {
            window_draw_texture(g_label_debug_tex, debug_radio_x + RADIO_SIZE + 8,
                                 radio_y + (RADIO_SIZE - g_label_debug_h) / 2,
                                 g_label_debug_w, g_label_debug_h);
        }
        if (release_tex != NULL) window_draw_texture(release_tex, release_radio_x, radio_y, RADIO_SIZE, RADIO_SIZE);
        if (g_label_release_tex != NULL) {
            window_draw_texture(g_label_release_tex, release_radio_x + RADIO_SIZE + 8,
                                 radio_y + (RADIO_SIZE - g_label_release_h) / 2,
                                 g_label_release_w, g_label_release_h);
        }

        int cancel_x = dx + DIALOG_WIDTH - DIALOG_PADDING - g_export_btn.width - 8 - g_cancel_btn.width;
        int export_x = dx + DIALOG_WIDTH - DIALOG_PADDING - g_export_btn.width;
        int buttons_y = dy + DIALOG_HEIGHT - DIALOG_PADDING - g_cancel_btn.height;

        if (g_cancel_btn_tex != NULL) {
            window_draw_texture(g_cancel_btn_tex, cancel_x, buttons_y, g_cancel_btn.width, g_cancel_btn.height);
            window_draw_texture(g_cancel_txt_tex, cancel_x + g_cancel_btn.text_offset_x,
                                 buttons_y + g_cancel_btn.text_offset_y,
                                 g_cancel_btn.text_img.width, g_cancel_btn.text_img.height);
        }
        if (g_export_btn_tex != NULL) {
            window_draw_texture(g_export_btn_tex, export_x, buttons_y, g_export_btn.width, g_export_btn.height);
            window_draw_texture(g_export_txt_tex, export_x + g_export_btn.text_offset_x,
                                 buttons_y + g_export_btn.text_offset_y,
                                 g_export_btn.text_img.width, g_export_btn.text_img.height);
        }
        return;
    }

    /* أي حالة ثانية (RUNNING/SUCCESS/FAILED) - لوحة مخرجات حية بدل
     * الحقول، بخط أحادي المسافة تقريباً (نفس عائلة الخط العادية -
     * المشروع ما فيه خط monospace مخصَّص بعد) */
    int log_x = dx + DIALOG_PADDING;
    int log_y = dy + TITLEBAR_HEIGHT + 12;
    int line_h = LOG_LINE_FONT_SIZE + 6;
    for (int i = 0; i < g_log_display_count; i++) {
        draw_log_line(i, log_x, log_y + i * line_h);
    }

    int buttons_y = dy + DIALOG_HEIGHT - DIALOG_PADDING - g_cancel_btn.height;
    int btn_x = dx + DIALOG_WIDTH - DIALOG_PADDING - g_cancel_btn.width;
    const char *btn_label = (status == PS2_EXPORT_STATUS_RUNNING) ? "Cancel" : "Close";

    /* نص الزر يتغيّر حسب الحالة (Cancel وقت البناء، Close بعده) -
     * بلا حاجة لزر منفصل، نعيد رسم نصه فقط لو الحالة تغيّرت */
    static ps2_export_status_t last_drawn_status = PS2_EXPORT_STATUS_IDLE;
    if (last_drawn_status != status) {
        if (g_cancel_txt_tex != NULL) window_destroy_texture(g_cancel_txt_tex);
        int tw, th;
        g_cancel_txt_tex = make_text_texture(btn_label, FONT_WEIGHT_REGULAR, LABEL_FONT_SIZE, &tw, &th);
        last_drawn_status = status;
    }
    if (g_cancel_btn_tex != NULL) {
        window_draw_texture(g_cancel_btn_tex, btn_x, buttons_y, g_cancel_btn.width, g_cancel_btn.height);
    }
    if (g_cancel_txt_tex != NULL) {
        window_draw_texture(g_cancel_txt_tex, btn_x + g_cancel_btn.text_offset_x,
                             buttons_y + g_cancel_btn.text_offset_y,
                             g_cancel_btn.text_img.width, g_cancel_btn.text_img.height);
    }
}

void export_dialog_shutdown(void) {
    if (g_title_tex) window_destroy_texture(g_title_tex);
    if (g_label_name_tex) window_destroy_texture(g_label_name_tex);
    if (g_label_path_tex) window_destroy_texture(g_label_path_tex);
    if (g_label_debug_tex) window_destroy_texture(g_label_debug_tex);
    if (g_label_release_tex) window_destroy_texture(g_label_release_tex);
    if (g_browse_btn_tex) window_destroy_texture(g_browse_btn_tex);
    if (g_browse_txt_tex) window_destroy_texture(g_browse_txt_tex);
    if (g_cancel_btn_tex) window_destroy_texture(g_cancel_btn_tex);
    if (g_cancel_txt_tex) window_destroy_texture(g_cancel_txt_tex);
    if (g_export_btn_tex) window_destroy_texture(g_export_btn_tex);
    if (g_export_txt_tex) window_destroy_texture(g_export_txt_tex);
    if (g_template_ok_tex) window_destroy_texture(g_template_ok_tex);
    if (g_template_ok_txt_tex) window_destroy_texture(g_template_ok_txt_tex);
    if (g_template_download_tex) window_destroy_texture(g_template_download_tex);
    if (g_template_download_txt_tex) window_destroy_texture(g_template_download_txt_tex);
    if (g_template_install_tex) window_destroy_texture(g_template_install_tex);
    if (g_template_install_txt_tex) window_destroy_texture(g_template_install_txt_tex);
    if (g_template_title_tex) window_destroy_texture(g_template_title_tex);
    if (g_template_line1_tex) window_destroy_texture(g_template_line1_tex);
    if (g_template_line2_tex) window_destroy_texture(g_template_line2_tex);
    if (g_template_error_tex) window_destroy_texture(g_template_error_tex);
    if (g_close_btn_tex) window_destroy_texture(g_close_btn_tex);
    if (g_close_x_tex) window_destroy_texture(g_close_x_tex);
    if (g_radio_ring_tex) window_destroy_texture(g_radio_ring_tex);
    if (g_radio_filled_tex) window_destroy_texture(g_radio_filled_tex);
    for (int i = 0; i < LOG_VISIBLE_LINES; i++) {
        if (g_log_line_tex[i]) window_destroy_texture(g_log_line_tex[i]);
    }

    labeled_button_free(&g_browse_btn);
    labeled_button_free(&g_cancel_btn);
    labeled_button_free(&g_export_btn);
    labeled_button_free(&g_template_ok_btn);
    labeled_button_free(&g_template_download_btn);
    labeled_button_free(&g_template_install_btn);
    text_field_free(&g_name_field);
    text_field_free(&g_path_field);

    g_is_open = 0;
    g_template_prompt = 0;
    g_ready = 0;
}
