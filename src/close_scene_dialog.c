/*
 * ============================================================
 * close_scene_dialog.c
 * ============================================================
 * راجع close_scene_dialog.h للتوثيق الكامل. نافذة بسيطة جداً -
 * عنوان + سطر رسالة + ثلاث أزرار (Save بلون أزرق العلامة، Don't
 * Save وCancel بالرمادي الموحَّد) - بنفس أسلوب النافذة المنبثقة
 * الموجود أصلاً بـasset_browser.c (خلفية معتمة + صندوق بمنتصف
 * الشاشة).
 * ============================================================
 */

#include <stddef.h>

#include "close_scene_dialog.h"
#include "window.h"
#include "font.h"
#include "labeled_button.h"
#include "ui_project_center.h"

#define DIALOG_WIDTH   360
#define DIALOG_HEIGHT  140
#define BUTTON_GAP       10
#define BUTTON_ROW_H     36

static int g_is_open = 0;
static int g_tab_index = -1;
static close_scene_result_t g_pending_result = CLOSE_SCENE_RESULT_NONE;

static window_texture_t *g_title_tex = NULL;
static int g_title_w = 0, g_title_h = 0;

static labeled_button_t  g_btn_save, g_btn_dont_save, g_btn_cancel;
static window_texture_t *g_save_bg = NULL,      *g_save_txt = NULL;
static window_texture_t *g_dont_save_bg = NULL, *g_dont_save_txt = NULL;
static window_texture_t *g_cancel_bg = NULL,    *g_cancel_txt = NULL;

static int g_ready = 0;

static void ensure_ready(void) {
    if (g_ready) return;
    g_ready = 1;

    font_text_image_t title = font_render_text("Unsaved changes", FONT_WEIGHT_BOLD, 16);
    if (title.pixels != NULL) {
        g_title_tex = window_create_texture(title.pixels, title.width, title.height);
        g_title_w = title.width;
        g_title_h = title.height;
        font_free_text_image(&title);
    }

    g_btn_save = labeled_button_create("Save", FONT_WEIGHT_REGULAR, 13, 18, 8, 4,
                                        UI_COLOR_BUTTON_BLUE.r, UI_COLOR_BUTTON_BLUE.g, UI_COLOR_BUTTON_BLUE.b);
    g_save_bg  = window_create_texture(g_btn_save.shape_img.pixels, g_btn_save.width, g_btn_save.height);
    g_save_txt = window_create_texture(g_btn_save.text_img.pixels, g_btn_save.text_img.width, g_btn_save.text_img.height);

    g_btn_dont_save = labeled_button_create("Don't Save", FONT_WEIGHT_REGULAR, 13, 18, 8, 4,
                                             UI_COLOR_GRAY_MUTED.r, UI_COLOR_GRAY_MUTED.g, UI_COLOR_GRAY_MUTED.b);
    g_dont_save_bg  = window_create_texture(g_btn_dont_save.shape_img.pixels, g_btn_dont_save.width, g_btn_dont_save.height);
    g_dont_save_txt = window_create_texture(g_btn_dont_save.text_img.pixels, g_btn_dont_save.text_img.width, g_btn_dont_save.text_img.height);

    g_btn_cancel = labeled_button_create("Cancel", FONT_WEIGHT_REGULAR, 13, 18, 8, 4,
                                          UI_COLOR_GRAY_MUTED.r, UI_COLOR_GRAY_MUTED.g, UI_COLOR_GRAY_MUTED.b);
    g_cancel_bg  = window_create_texture(g_btn_cancel.shape_img.pixels, g_btn_cancel.width, g_btn_cancel.height);
    g_cancel_txt = window_create_texture(g_btn_cancel.text_img.pixels, g_btn_cancel.text_img.width, g_btn_cancel.text_img.height);
}

void close_scene_dialog_open(int tab_index) {
    ensure_ready();
    g_tab_index = tab_index;
    g_pending_result = CLOSE_SCENE_RESULT_NONE;
    g_is_open = 1;
}

int close_scene_dialog_is_open(void) { return g_is_open; }
int close_scene_dialog_get_tab_index(void) { return g_tab_index; }

/* ثلاثة الأزرار متجاورة يميناً لليسار (Cancel أقصى اليمين، زي أي
 * نافذة تأكيد قياسية) - نحسب مواضعها هنا، ونعيد استخدامها بالرسم */
static void compute_button_positions(int dx, int dy, int *cancel_x, int *dont_save_x, int *save_x, int *buttons_y) {
    *buttons_y = dy + DIALOG_HEIGHT - 16 - BUTTON_ROW_H;
    *cancel_x = dx + DIALOG_WIDTH - 16 - g_btn_cancel.width;
    *dont_save_x = *cancel_x - BUTTON_GAP - g_btn_dont_save.width;
    *save_x = *dont_save_x - BUTTON_GAP - g_btn_save.width;
}

void close_scene_dialog_update(int window_w, int window_h) {
    if (!g_is_open) return;

    int dx = (window_w - DIALOG_WIDTH) / 2;
    int dy = (window_h - DIALOG_HEIGHT) / 2;

    int cancel_x, dont_save_x, save_x, buttons_y;
    compute_button_positions(dx, dy, &cancel_x, &dont_save_x, &save_x, &buttons_y);

    int mx = window_mouse_x(), my = window_mouse_y();
    if (!window_mouse_left_just_pressed()) return;

    if (mx >= save_x && mx < save_x + g_btn_save.width && my >= buttons_y && my < buttons_y + g_btn_save.height) {
        g_pending_result = CLOSE_SCENE_RESULT_SAVE;
        g_is_open = 0;
    } else if (mx >= dont_save_x && mx < dont_save_x + g_btn_dont_save.width && my >= buttons_y && my < buttons_y + g_btn_dont_save.height) {
        g_pending_result = CLOSE_SCENE_RESULT_DONT_SAVE;
        g_is_open = 0;
    } else if (mx >= cancel_x && mx < cancel_x + g_btn_cancel.width && my >= buttons_y && my < buttons_y + g_btn_cancel.height) {
        g_pending_result = CLOSE_SCENE_RESULT_CANCEL;
        g_is_open = 0;
    }
}

void close_scene_dialog_draw(int window_w, int window_h) {
    if (!g_is_open) return;

    /* خلفية معتمة تغطي الشاشة كاملة - نفس أسلوب asset_browser.c */
    window_fill_rect(0, 0, window_w, window_h, 0, 0, 0);

    int dx = (window_w - DIALOG_WIDTH) / 2;
    int dy = (window_h - DIALOG_HEIGHT) / 2;

    window_fill_rect(dx, dy, DIALOG_WIDTH, DIALOG_HEIGHT, UI_COLOR_PANEL_BG.r, UI_COLOR_PANEL_BG.g, UI_COLOR_PANEL_BG.b);

    if (g_title_tex != NULL) {
        window_draw_texture(g_title_tex, dx + 16, dy + 16, g_title_w, g_title_h);
    }

    int cancel_x, dont_save_x, save_x, buttons_y;
    compute_button_positions(dx, dy, &cancel_x, &dont_save_x, &save_x, &buttons_y);

    window_draw_texture(g_save_bg, save_x, buttons_y, g_btn_save.width, g_btn_save.height);
    window_draw_texture(g_save_txt, save_x + g_btn_save.text_offset_x, buttons_y + g_btn_save.text_offset_y,
                         g_btn_save.text_img.width, g_btn_save.text_img.height);

    window_draw_texture(g_dont_save_bg, dont_save_x, buttons_y, g_btn_dont_save.width, g_btn_dont_save.height);
    window_draw_texture(g_dont_save_txt, dont_save_x + g_btn_dont_save.text_offset_x, buttons_y + g_btn_dont_save.text_offset_y,
                         g_btn_dont_save.text_img.width, g_btn_dont_save.text_img.height);

    window_draw_texture(g_cancel_bg, cancel_x, buttons_y, g_btn_cancel.width, g_btn_cancel.height);
    window_draw_texture(g_cancel_txt, cancel_x + g_btn_cancel.text_offset_x, buttons_y + g_btn_cancel.text_offset_y,
                         g_btn_cancel.text_img.width, g_btn_cancel.text_img.height);
}

close_scene_result_t close_scene_dialog_consume_result(void) {
    close_scene_result_t r = g_pending_result;
    g_pending_result = CLOSE_SCENE_RESULT_NONE;
    return r;
}

void close_scene_dialog_shutdown(void) {
    if (g_title_tex) window_destroy_texture(g_title_tex);
    if (g_save_bg) window_destroy_texture(g_save_bg);
    if (g_save_txt) window_destroy_texture(g_save_txt);
    if (g_dont_save_bg) window_destroy_texture(g_dont_save_bg);
    if (g_dont_save_txt) window_destroy_texture(g_dont_save_txt);
    if (g_cancel_bg) window_destroy_texture(g_cancel_bg);
    if (g_cancel_txt) window_destroy_texture(g_cancel_txt);
    labeled_button_free(&g_btn_save);
    labeled_button_free(&g_btn_dont_save);
    labeled_button_free(&g_btn_cancel);
    g_title_tex = NULL;
    g_save_bg = g_save_txt = g_dont_save_bg = g_dont_save_txt = g_cancel_bg = g_cancel_txt = NULL;
    g_ready = 0;
    g_is_open = 0;
}
