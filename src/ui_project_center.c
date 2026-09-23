/*
 * ============================================================
 * ui_project_center.c
 * ============================================================
 */

#include <stddef.h>

#include "ui_project_center.h"
#include "window.h"
#include "labeled_button.h"
#include "project_dialog.h"

/* إعدادات الزر التجريبي - نفس نصف قطر استدارة الزر الأصلي (3 بكسل)
 * عشان الشكل ما يتغير إطلاقاً */
#define TEST_BUTTON_TEXT_SIZE  14
#define TEST_BUTTON_PADDING_X  12
#define TEST_BUTTON_PADDING_Y  8
#define TEST_BUTTON_MARGIN     12
#define TEST_BUTTON_RADIUS     3

static window_texture_t *g_test_button_texture = NULL;
static window_texture_t *g_test_text_texture   = NULL;
static labeled_button_t  g_test_button;
static int g_button_ready = 0;

void ui_project_center_layout(int window_w, int window_h,
                               ui_rect_t *out_searchbar,
                               ui_rect_t *out_project_panel,
                               ui_rect_t *out_buttons_panel) {
    out_searchbar->x = 0;
    out_searchbar->y = 0;
    out_searchbar->w = window_w;
    out_searchbar->h = UI_SEARCHBAR_HEIGHT;

    out_project_panel->x = 0;
    out_project_panel->y = UI_SEARCHBAR_HEIGHT;
    out_project_panel->w = window_w - UI_RIGHT_PANEL_WIDTH - UI_PANEL_GAP;
    out_project_panel->h = window_h - UI_SEARCHBAR_HEIGHT;

    out_buttons_panel->x = window_w - UI_RIGHT_PANEL_WIDTH;
    out_buttons_panel->y = UI_SEARCHBAR_HEIGHT;
    out_buttons_panel->w = UI_RIGHT_PANEL_WIDTH;
    out_buttons_panel->h = window_h - UI_SEARCHBAR_HEIGHT;
}

static void ensure_test_button_ready(void) {
    if (g_button_ready) {
        return;
    }
    g_button_ready = 1;

    g_test_button = labeled_button_create(
        "Create  +", FONT_WEIGHT_REGULAR, TEST_BUTTON_TEXT_SIZE,
        TEST_BUTTON_PADDING_X, TEST_BUTTON_PADDING_Y, TEST_BUTTON_RADIUS,
        UI_COLOR_BUTTON_BLUE.r, UI_COLOR_BUTTON_BLUE.g, UI_COLOR_BUTTON_BLUE.b
    );

    if (g_test_button.width <= 0) {
        return;
    }

    g_test_button_texture = window_create_texture(
        g_test_button.shape_img.pixels,
        g_test_button.shape_img.width,
        g_test_button.shape_img.height
    );
    g_test_text_texture = window_create_texture(
        g_test_button.text_img.pixels,
        g_test_button.text_img.width,
        g_test_button.text_img.height
    );
}

void ui_project_center_draw(int window_w, int window_h) {
    ui_rect_t searchbar, project_panel, buttons_panel;
    ui_project_center_layout(window_w, window_h, &searchbar, &project_panel, &buttons_panel);

    window_fill_rect(
        project_panel.x, project_panel.y, project_panel.w, project_panel.h,
        UI_COLOR_GRAY_MUTED.r, UI_COLOR_GRAY_MUTED.g, UI_COLOR_GRAY_MUTED.b
    );

    window_fill_rect(
        buttons_panel.x, buttons_panel.y, buttons_panel.w, buttons_panel.h,
        UI_COLOR_BLACK_MUTED.r, UI_COLOR_BLACK_MUTED.g, UI_COLOR_BLACK_MUTED.b
    );

    ensure_test_button_ready();

    if (g_test_button.width > 0) {
        int button_x = window_w - g_test_button.width - TEST_BUTTON_MARGIN;
        int button_y = TEST_BUTTON_MARGIN;

        if (g_test_button_texture != NULL) {
            window_draw_texture(g_test_button_texture, button_x, button_y,
                                 g_test_button.width, g_test_button.height);
        }
        if (g_test_text_texture != NULL) {
            int text_x = button_x + g_test_button.text_offset_x;
            int text_y = button_y + g_test_button.text_offset_y;
            window_draw_texture(g_test_text_texture, text_x, text_y,
                                 g_test_button.text_img.width, g_test_button.text_img.height);
        }

        if (!project_dialog_is_open() && window_mouse_left_just_pressed()) {
            int mx = window_mouse_x();
            int my = window_mouse_y();
            if (mx >= button_x && mx < button_x + g_test_button.width
                && my >= button_y && my < button_y + g_test_button.height) {
                project_dialog_open();
            }
        }
    }

    /* ------------------------------------------------------------
     * مستقبلاً: هنا تُضاف قائمة المشاريع (أيقونات + أسماء) داخل
     * project_panel، وشريط البحث داخل searchbar
     * ------------------------------------------------------------ */
}

void ui_project_center_shutdown(void) {
    if (g_test_button_texture != NULL) {
        window_destroy_texture(g_test_button_texture);
        g_test_button_texture = NULL;
    }
    if (g_test_text_texture != NULL) {
        window_destroy_texture(g_test_text_texture);
        g_test_text_texture = NULL;
    }
    labeled_button_free(&g_test_button);
    font_shutdown();
    g_button_ready = 0;
}
