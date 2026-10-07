/*
 * ============================================================
 * export_dialog.c
 * ============================================================
 * See export_dialog.h for general documentation. Exactly the same approach as
 * project_dialog.c (fields, buttons, integrated folder browser, X close button
 * from file_manager) — no reinventing any new patterns.
 * ============================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__APPLE__)
#include <TargetConditionals.h>
#if TARGET_OS_IPHONE
#define EXPORT_UNSUPPORTED 1
#endif
#endif

#include "export_dialog.h"
#include "window.h"
#include "labeled_button.h"
#include "font.h"
#include "text_field.h"
#include "file_manager.h"
#include "shape_provider.h"
#include "ui_theme.h"
#include "ps2_exporter.h"
#include "ui_common.h"
#include "ui_dialog.h"
#include "android_storage.h"

#define DIALOG_WIDTH    620
#define DIALOG_HEIGHT   460
#define DIALOG_PADDING  24
#define FIELD_HEIGHT    28
#define LABEL_FONT_SIZE 15
#define FIELD_FONT_SIZE 16
#define TITLE_FONT_SIZE 18
#define TITLEBAR_HEIGHT 40
#define CLOSE_BTN_SIZE  24
#define RADIO_SIZE      16
#define LOG_VISIBLE_LINES 22
#define LOG_LINE_FONT_SIZE 12

static int g_is_open = 0;
static int g_ready = 0;

#if defined(EXPORT_UNSUPPORTED)
#define NOTICE_FONT_SIZE (LABEL_FONT_SIZE + 4)
#define NOTICE_LINE_COUNT 7

static const char *const NOTICE_TITLE = "Export is not supported on iOS";
static const char *const NOTICE_LINES[NOTICE_LINE_COUNT] = {
    "iOS does not allow apps to run other programs,",
    "so the PS2 build tools (make, ps2dev) cannot run.",
    "",
    "You can still create and edit your project here.",
    "To export, copy the project folder to a PC, Mac",
    "or Linux computer and export it from there.",
    ""
};

static window_texture_t *g_notice_title_tex = NULL;
static int g_notice_title_w = 0, g_notice_title_h = 0;
static window_texture_t *g_notice_line_tex[NOTICE_LINE_COUNT];
static int g_notice_line_w[NOTICE_LINE_COUNT];
static int g_notice_line_h[NOTICE_LINE_COUNT];
static labeled_button_t g_close_btn;
static window_texture_t *g_close_btn_tex = NULL;
static window_texture_t *g_close_txt_tex = NULL;

static window_texture_t *make_notice_texture(const char *text, font_weight_t weight,
                                              unsigned char r, unsigned char g, unsigned char b,
                                              int *out_w, int *out_h) {
    size_t n = strlen(text);
    if (n == 0) return NULL;
    unsigned char *rgb = (unsigned char *)malloc(n * 3);
    if (rgb == NULL) return NULL;
    for (size_t i = 0; i < n; i++) { rgb[i * 3] = r; rgb[i * 3 + 1] = g; rgb[i * 3 + 2] = b; }
    font_text_image_t img = font_render_text_colored(text, weight, NOTICE_FONT_SIZE, rgb);
    free(rgb);
    window_texture_t *tex = NULL;
    if (img.pixels != NULL) {
        tex = window_create_texture(img.pixels, img.width, img.height);
        *out_w = img.width;
        *out_h = img.height;
        font_free_text_image(&img);
    }
    return tex;
}
#endif

static text_field_t g_name_field;
static text_field_t g_path_field;

static int g_is_release = 0; /* 0 = Debug (default), 1 = Release */

static labeled_button_t  g_browse_btn;
static labeled_button_t  g_cancel_btn;
static labeled_button_t  g_export_btn;
static window_texture_t *g_browse_btn_tex = NULL;
static window_texture_t *g_browse_txt_tex = NULL;
static window_texture_t *g_cancel_btn_tex = NULL;
static window_texture_t *g_cancel_txt_tex = NULL;
static window_texture_t *g_export_btn_tex = NULL;
static window_texture_t *g_export_txt_tex = NULL;

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

/* Debug/Release radio circles - empty (outline) and filled, two colors
 * (normal/selected) - built once, drawing picks the appropriate ready-made one */
static window_texture_t *g_radio_ring_tex = NULL;
static window_texture_t *g_radio_filled_tex = NULL;

/* Currently displayed export output lines - a local copy of the last
 * LOG_VISIBLE_LINES lines received from ps2_exporter (the real reader is there,
 * this is only a display buffer) */
static char g_log_display[LOG_VISIBLE_LINES][200];
static int g_log_display_count = 0;
static window_texture_t *g_log_line_tex[LOG_VISIBLE_LINES];
static int g_log_line_w[LOG_VISIBLE_LINES];
static int g_log_line_h[LOG_VISIBLE_LINES];
static char g_log_line_cached[LOG_VISIBLE_LINES][200];

static void ensure_ready(void) {
    if (g_ready) return;
    g_ready = 1;

    if (!font_init()) return;

    text_field_init(&g_name_field);
    text_field_init(&g_path_field);
    strncpy(g_name_field.text, "MyGame", TEXT_FIELD_MAX_LEN - 1);
    g_name_field.cursor_pos = (int)strlen(g_name_field.text);

    g_title_tex = ui_make_text_texture("Export to PlayStation 2", FONT_WEIGHT_BOLD, TITLE_FONT_SIZE,
                                     &g_title_w, &g_title_h);
    g_label_name_tex = ui_make_text_texture("Executable name", FONT_WEIGHT_REGULAR, LABEL_FONT_SIZE,
                                          &g_label_name_w, &g_label_name_h);
    g_label_path_tex = ui_make_text_texture("Export path", FONT_WEIGHT_REGULAR, LABEL_FONT_SIZE,
                                          &g_label_path_w, &g_label_path_h);
    g_label_debug_tex = ui_make_text_texture("Debug", FONT_WEIGHT_REGULAR, LABEL_FONT_SIZE,
                                           &g_label_debug_w, &g_label_debug_h);
    g_label_release_tex = ui_make_text_texture("Release", FONT_WEIGHT_REGULAR, LABEL_FONT_SIZE,
                                             &g_label_release_w, &g_label_release_h);

    ui_make_blue_button("Browse", LABEL_FONT_SIZE, &g_browse_btn, &g_browse_btn_tex, &g_browse_txt_tex);
    ui_make_blue_button("Cancel", LABEL_FONT_SIZE, &g_cancel_btn, &g_cancel_btn_tex, &g_cancel_txt_tex);
    ui_make_blue_button("Export", LABEL_FONT_SIZE, &g_export_btn, &g_export_btn_tex, &g_export_txt_tex);
    /* The selection circles - same trick as shape_provider (rounded-rect with
 * corner radius = half size makes a full circle). The outline (unselected)
 * is pale gray, the filled (selected) uses the theme blue color */
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

#if defined(EXPORT_UNSUPPORTED)
    g_notice_title_tex = make_notice_texture(NOTICE_TITLE, FONT_WEIGHT_BOLD, 255, 90, 90,
                                             &g_notice_title_w, &g_notice_title_h);
    for (int i = 0; i < NOTICE_LINE_COUNT; i++) {
        g_notice_line_w[i] = g_notice_line_h[i] = 0;
        g_notice_line_tex[i] = make_notice_texture(NOTICE_LINES[i], FONT_WEIGHT_REGULAR, 255, 214, 80,
                                                   &g_notice_line_w[i], &g_notice_line_h[i]);
    }
    ui_make_blue_button("Close", LABEL_FONT_SIZE, &g_close_btn, &g_close_btn_tex, &g_close_txt_tex);
#endif
}

void export_dialog_open(void) {
    android_storage_request_if_needed();
    ensure_ready();
    g_is_open = 1;
}

int export_dialog_is_open(void) {
    return g_is_open;
}

/* Called by file_manager when the folder chooser window is closed */
static void on_browse_result(const char *path, void *user_data) {
    (void)user_data;
    if (path == NULL) return;
    strncpy(g_path_field.text, path, TEXT_FIELD_MAX_LEN - 1);
    g_path_field.text[TEXT_FIELD_MAX_LEN - 1] = '\0';
    g_path_field.cursor_pos = (int)strlen(g_path_field.text);
    g_path_field.scroll_offset = 0;
}

/* Adds one ready-made display line (pre-split if needed) to the scrolling
 * display list - the same scrolling logic used originally, extracted into a
 * separate function for reuse by long-line wrapping */
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

/* Approximate wrap limit (in characters) for displaying the current export window in the log font -
 * long build commands (a full gcc line with all -I paths, for example) wrap
 * across multiple display lines instead of overflowing past the window edge */
#define LOG_WRAP_CHARS 84

/* Pulls all new lines from ps2_exporter, wraps any line longer than the
 * window limit across multiple display lines, and updates the scrolled display
 * list (the last LOG_VISIBLE_LINES display lines - continues to scroll down
 * like a normal terminal) */
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

#if defined(EXPORT_UNSUPPORTED)
    {
        int ux, uy;
        ui_center_rect(window_w, window_h, DIALOG_WIDTH, DIALOG_HEIGHT, &ux, &uy);
        if (window_mouse_left_just_pressed()) {
            int mx = window_mouse_x(), my = window_mouse_y();
            int cx = ux + DIALOG_WIDTH - DIALOG_PADDING - CLOSE_BTN_SIZE;
            int cy = uy + (TITLEBAR_HEIGHT - CLOSE_BTN_SIZE) / 2;
            int bx = ux + DIALOG_WIDTH - DIALOG_PADDING - g_close_btn.width;
            int by = uy + DIALOG_HEIGHT - DIALOG_PADDING - g_close_btn.height;
            if ((mx >= cx && mx < cx + CLOSE_BTN_SIZE && my >= cy && my < cy + CLOSE_BTN_SIZE)
                || (mx >= bx && mx < bx + g_close_btn.width && my >= by && my < by + g_close_btn.height)) {
                g_is_open = 0;
            }
        }
        return;
    }
#endif

    /* The crucial line in this function - advances the export state machine one step (starts
     * shell commands, polls them later without blocking). Without it the export does
     * not progress at all after the first call to ps2_export_start - it was indeed missing */
    ps2_export_update();

    ps2_export_status_t status = ps2_export_get_status();
    int is_running = (status == PS2_EXPORT_STATUS_RUNNING);

    if (status != PS2_EXPORT_STATUS_IDLE) {
        pump_export_log();
    }

    int dx, dy;
    ui_center_rect(window_w, window_h, DIALOG_WIDTH, DIALOG_HEIGHT, &dx, &dy);

    /* Close X button - always enabled, even during a build (treated as Cancel) */
    int close_x = dx + DIALOG_WIDTH - DIALOG_PADDING - CLOSE_BTN_SIZE;
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

    /* During an actual export: no other interaction (fields/radio/browse) -
     * only wait until it succeeds or fails, or cancel with the X/Cancel button */
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

    /* Success or failure - only one "Close" button (same place as Cancel) */
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

    /* IDLE - normal settings mode */
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

    /* Browse button */
    int browse_x = field_x + path_field_w + 8;
    int browse_y = path_field_y;
    if (window_mouse_left_just_pressed()) {
        int mx = window_mouse_x(), my = window_mouse_y();
        if (mx >= browse_x && mx < browse_x + g_browse_btn.width
            && my >= browse_y && my < browse_y + g_browse_btn.height) {
            file_manager_open(FILE_MANAGER_ROOT_DEVICE, FILE_MANAGER_MODE_PICK_FOLDER,
                                NULL, 0, on_browse_result, NULL);
        }
    }

    /* Debug/Release radio buttons - to the right of the path field on a new row */
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

    /* Cancel button (closes the window without exporting) */
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

    /* Export button - only enabled when the name and export path are not empty */
    int export_x = dx + DIALOG_WIDTH - DIALOG_PADDING - g_export_btn.width;
    if (window_mouse_left_just_pressed()) {
        int mx = window_mouse_x(), my = window_mouse_y();
        if (mx >= export_x && mx < export_x + g_export_btn.width
            && my >= buttons_y && my < buttons_y + g_export_btn.height) {
            if (g_name_field.text[0] != '\0' && g_path_field.text[0] != '\0') {
                g_name_field.focused = 0;
                g_path_field.focused = 0;
                window_stop_text_input();
                g_log_display_count = 0;
                ps2_export_start(g_name_field.text, g_is_release, g_path_field.text);
            }
        }
    }
}

/* Renders text as an output line - rebuilds its texture only if it actually changed (same
 * path_msg pattern in project_dialog.c, applied here per line in the array) */
static void draw_log_line(int index, int x, int y) {
    if (strcmp(g_log_display[index], g_log_line_cached[index]) != 0 || g_log_line_tex[index] == NULL) {
        if (g_log_line_tex[index] != NULL) {
            window_destroy_texture(g_log_line_tex[index]);
            g_log_line_tex[index] = NULL;
        }
        /* Empty line (e.g. a blank line between steps) - font_render_text returns
         * an image with zero width for an empty string, so we avoid even attempting to create a texture from it */
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
        /* The bug was right here - it used 0,0 instead of the real dimensions,
         * so it drew a rectangle with zero area (invisible) even though the texture
         * itself was built correctly */
        window_draw_texture(g_log_line_tex[index], x, y, g_log_line_w[index], g_log_line_h[index]);
    }
}

void export_dialog_draw(int window_w, int window_h) {
    if (!g_is_open) return;

    ui_dialog_draw_backdrop(window_w, window_h);
    int dx, dy;
    ui_center_rect(window_w, window_h, DIALOG_WIDTH, DIALOG_HEIGHT, &dx, &dy);

    ui_dialog_draw_frame(dx, dy, DIALOG_WIDTH, DIALOG_HEIGHT);
    ui_dialog_draw_title_left(g_title_tex, g_title_w, g_title_h, dx, dy, TITLEBAR_HEIGHT, DIALOG_PADDING);
    ui_dialog_draw_close_button(dx + DIALOG_WIDTH - DIALOG_PADDING - CLOSE_BTN_SIZE,
                                dy + (TITLEBAR_HEIGHT - CLOSE_BTN_SIZE) / 2, CLOSE_BTN_SIZE);

#if defined(EXPORT_UNSUPPORTED)
    {
        int ty = dy + TITLEBAR_HEIGHT + 24;
        if (g_notice_title_tex != NULL) {
            window_draw_texture(g_notice_title_tex, dx + DIALOG_PADDING, ty, g_notice_title_w, g_notice_title_h);
        }
        ty += NOTICE_FONT_SIZE + 22;
        for (int i = 0; i < NOTICE_LINE_COUNT; i++) {
            if (g_notice_line_tex[i] != NULL) {
                window_draw_texture(g_notice_line_tex[i], dx + DIALOG_PADDING, ty,
                                    g_notice_line_w[i], g_notice_line_h[i]);
            }
            ty += NOTICE_FONT_SIZE + 12;
        }
        int bx = dx + DIALOG_WIDTH - DIALOG_PADDING - g_close_btn.width;
        int by = dy + DIALOG_HEIGHT - DIALOG_PADDING - g_close_btn.height;
        if (g_close_btn_tex != NULL) {
            window_draw_texture(g_close_btn_tex, bx, by, g_close_btn.width, g_close_btn.height);
            window_draw_texture(g_close_txt_tex, bx + g_close_btn.text_offset_x, by + g_close_btn.text_offset_y,
                                g_close_btn.text_img.width, g_close_btn.text_img.height);
        }
        return;
    }
#endif

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

        /* Debug/Release circles */
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

    /* Any other state (RUNNING/SUCCESS/FAILED) - live output panel instead
     * of fields, using an almost-monospaced font (same family as the regular font -
     * the project doesn't have a dedicated monospace font yet) */
    int log_x = dx + DIALOG_PADDING;
    int log_y = dy + TITLEBAR_HEIGHT + 12;
    int line_h = LOG_LINE_FONT_SIZE + 6;
    for (int i = 0; i < g_log_display_count; i++) {
        draw_log_line(i, log_x, log_y + i * line_h);
    }

    int buttons_y = dy + DIALOG_HEIGHT - DIALOG_PADDING - g_cancel_btn.height;
    int btn_x = dx + DIALOG_WIDTH - DIALOG_PADDING - g_cancel_btn.width;
    const char *btn_label = (status == PS2_EXPORT_STATUS_RUNNING) ? "Cancel" : "Close";

    /* The button label changes with state (Cancel during build, Close after) -
     * no separate button needed; we redraw the label only when the state changes */
    static ps2_export_status_t last_drawn_status = PS2_EXPORT_STATUS_IDLE;
    if (last_drawn_status != status) {
        if (g_cancel_txt_tex != NULL) window_destroy_texture(g_cancel_txt_tex);
        int tw, th;
        g_cancel_txt_tex = ui_make_text_texture(btn_label, FONT_WEIGHT_REGULAR, LABEL_FONT_SIZE, &tw, &th);
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
    if (g_radio_ring_tex) window_destroy_texture(g_radio_ring_tex);
    if (g_radio_filled_tex) window_destroy_texture(g_radio_filled_tex);
    for (int i = 0; i < LOG_VISIBLE_LINES; i++) {
        if (g_log_line_tex[i]) window_destroy_texture(g_log_line_tex[i]);
    }

#if defined(EXPORT_UNSUPPORTED)
    if (g_notice_title_tex) window_destroy_texture(g_notice_title_tex);
    g_notice_title_tex = NULL;
    for (int i = 0; i < NOTICE_LINE_COUNT; i++) {
        if (g_notice_line_tex[i]) window_destroy_texture(g_notice_line_tex[i]);
        g_notice_line_tex[i] = NULL;
    }
    if (g_close_btn_tex) window_destroy_texture(g_close_btn_tex);
    if (g_close_txt_tex) window_destroy_texture(g_close_txt_tex);
    g_close_btn_tex = g_close_txt_tex = NULL;
    labeled_button_free(&g_close_btn);
#endif

    labeled_button_free(&g_browse_btn);
    labeled_button_free(&g_cancel_btn);
    labeled_button_free(&g_export_btn);
    text_field_free(&g_name_field);
    text_field_free(&g_path_field);

    g_is_open = 0;
    g_ready = 0;
}
