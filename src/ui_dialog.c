#include <stddef.h>

#include "ui_dialog.h"
#include "shape_provider.h"
#include "ui_common.h"
#include "ui_theme.h"

static window_texture_t *g_backdrop_tex = NULL;
static window_texture_t *g_close_bg_tex = NULL;
static window_texture_t *g_close_x_tex = NULL;
static int g_close_size = 0;
static int g_close_x_w = 0, g_close_x_h = 0;

void ui_dialog_draw_backdrop(int window_w, int window_h) {
    if (g_backdrop_tex == NULL) {
        unsigned char pixels[4 * 4 * 4];
        for (int i = 0; i < 16; i++) {
            pixels[i * 4] = 8; pixels[i * 4 + 1] = 8; pixels[i * 4 + 2] = 8; pixels[i * 4 + 3] = 140;
        }
        g_backdrop_tex = window_create_texture(pixels, 4, 4);
    }
    if (g_backdrop_tex != NULL) window_draw_texture(g_backdrop_tex, 0, 0, window_w, window_h);
}

void ui_dialog_draw_frame(int x, int y, int w, int h) {
    window_fill_rect(x, y, w, h, UI_COLOR_PANEL_BG.r, UI_COLOR_PANEL_BG.g, UI_COLOR_PANEL_BG.b);
}

void ui_dialog_draw_title_left(window_texture_t *title, int title_w, int title_h,
                               int x, int y, int titlebar_height, int padding) {
    if (title != NULL) window_draw_texture(title, x + padding,
                                           y + (titlebar_height - title_h) / 2, title_w, title_h);
}

void ui_dialog_draw_title_centered(window_texture_t *title, int title_w, int title_h,
                                   int x, int y, int w, int titlebar_height) {
    if (title != NULL) window_draw_texture(title, x + (w - title_w) / 2,
                                           y + (titlebar_height - title_h) / 2, title_w, title_h);
}

static void ensure_close_button(int size) {
    if (g_close_size == size && g_close_bg_tex != NULL && g_close_x_tex != NULL) return;
    if (g_close_bg_tex != NULL) window_destroy_texture(g_close_bg_tex);
    if (g_close_x_tex != NULL) window_destroy_texture(g_close_x_tex);
    g_close_bg_tex = g_close_x_tex = NULL;
    g_close_size = size;
    shape_image_t shape = shape_provider_render_rect(size, size, 5, 90, 40, 40);
    if (shape.pixels != NULL) {
        g_close_bg_tex = window_create_texture(shape.pixels, shape.width, shape.height);
        shape_provider_free_image(&shape);
    }
    g_close_x_tex = ui_make_text_texture("X", FONT_WEIGHT_BOLD, 14, &g_close_x_w, &g_close_x_h);
}

void ui_dialog_draw_close_button(int x, int y, int size) {
    ensure_close_button(size);
    if (g_close_bg_tex != NULL) window_draw_texture(g_close_bg_tex, x, y, size, size);
    if (g_close_x_tex != NULL) window_draw_texture(g_close_x_tex,
                                                    x + (size - g_close_x_w) / 2,
                                                    y + (size - g_close_x_h) / 2,
                                                    g_close_x_w, g_close_x_h);
}

int ui_dialog_close_button_hit(int mouse_x, int mouse_y, int x, int y, int size) {
    return mouse_x >= x && mouse_x < x + size && mouse_y >= y && mouse_y < y + size;
}

void ui_dialog_shutdown(void) {
    if (g_backdrop_tex != NULL) window_destroy_texture(g_backdrop_tex);
    if (g_close_bg_tex != NULL) window_destroy_texture(g_close_bg_tex);
    if (g_close_x_tex != NULL) window_destroy_texture(g_close_x_tex);
    g_backdrop_tex = g_close_bg_tex = g_close_x_tex = NULL;
    g_close_size = g_close_x_w = g_close_x_h = 0;
}
