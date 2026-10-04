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
#include "project_catalog.h"
#include "scene_tabs.h"
#include "loading_screen.h"
#include "file_manager.h" /* Browse button opens the built-in file browser instead of relying on an external system dialog */
#include "ui_theme.h"
#include "ui_common.h"
#include "ui_dialog.h"

#define DIALOG_WIDTH   420
#define DIALOG_HEIGHT  240
#define DIALOG_PADDING 24
#define FIELD_HEIGHT   28
#define LABEL_FONT_SIZE 15
#define FIELD_FONT_SIZE 16
#define TITLE_FONT_SIZE 18
#define TITLEBAR_HEIGHT 40
#define CLOSE_BTN_SIZE 22

/* Colors for the path validation message (green = valid, red = invalid) */
static const unsigned char COLOR_VALID_R = 0x5C, COLOR_VALID_G = 0xB8, COLOR_VALID_B = 0x5C;
static const unsigned char COLOR_INVALID_R = 0xD9, COLOR_INVALID_G = 0x5C, COLOR_INVALID_B = 0x5C;

static int g_is_open = 0;
static int g_ready = 0;

static text_field_t g_name_field;
static text_field_t g_path_field;

/* Validation state of the entered path - updated every frame during typing */
static int g_path_valid = 0;
static int g_scene_switch_blocked = 0;
static const char *g_path_message = "";

/* Cached image of the validation message - redrawn only if the message or its validity
 * actually changed from the last frame, not every frame */
static window_texture_t *g_path_msg_tex = NULL;
static const char *g_path_msg_cached_text = NULL;
static int g_path_msg_cached_valid = -1;
static int g_path_msg_w = 0, g_path_msg_h = 0;

/* Pre-sized buttons (browse / cancel / create) - built once only when the dialog is first opened */
static labeled_button_t  g_browse_btn;
static labeled_button_t  g_cancel_btn;
static labeled_button_t  g_create_btn;
static window_texture_t *g_browse_btn_tex = NULL;
static window_texture_t *g_browse_txt_tex = NULL;
static window_texture_t *g_cancel_btn_tex = NULL;
static window_texture_t *g_cancel_txt_tex = NULL;
static window_texture_t *g_create_btn_tex = NULL;
static window_texture_t *g_create_txt_tex = NULL;

/* Static text strings (title + field labels) */
static window_texture_t *g_title_tex = NULL;
static int g_title_w = 0, g_title_h = 0;
static window_texture_t *g_label_name_tex = NULL;
static int g_label_name_w = 0, g_label_name_h = 0;
static window_texture_t *g_label_path_tex = NULL;
static int g_label_path_w = 0, g_label_path_h = 0;

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

    g_title_tex = ui_make_text_texture("New Project", FONT_WEIGHT_BOLD, TITLE_FONT_SIZE,
                                     &g_title_w, &g_title_h);
    g_label_name_tex = ui_make_text_texture("Project name", FONT_WEIGHT_REGULAR, LABEL_FONT_SIZE,
                                          &g_label_name_w, &g_label_name_h);
    g_label_path_tex = ui_make_text_texture("Project path", FONT_WEIGHT_REGULAR, LABEL_FONT_SIZE,
                                          &g_label_path_w, &g_label_path_h);

    ui_make_blue_button("Browse", LABEL_FONT_SIZE, &g_browse_btn, &g_browse_btn_tex, &g_browse_txt_tex);
    ui_make_blue_button("Cancel", LABEL_FONT_SIZE, &g_cancel_btn, &g_cancel_btn_tex, &g_cancel_txt_tex);
    ui_make_blue_button("Create", LABEL_FONT_SIZE, &g_create_btn, &g_create_btn_tex, &g_create_txt_tex);
}

void project_dialog_open(void) {
    ensure_ready();
    g_scene_switch_blocked = 0;
    g_is_open = 1;
}

int project_dialog_is_open(void) {
    return g_is_open;
}

/* Checks whether the entered path is valid to create a project in: must be an absolute path (starts with /), then walk backwards until finding the first path component that actually exists on disk, and verify we have write permission there (missing subfolders after that will be created during the actual creation, so they are not required now) */
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

/* Updates g_path_valid and g_path_message based on the current path field contents - called every frame while the dialog is open */
static void update_path_validation(void) {
    if (g_scene_switch_blocked) {
        g_path_valid = 0;
        g_path_message = "Save or close modified scenes before creating another project";
    } else if (g_path_field.text[0] == '\0') {
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

/* Called once by the file_manager when the folder chooser closes - either with the selected path (Select pressed), or NULL if the operation was canceled (Cancel or the X close button) - in that case we do not change the path field at all; it stays as it was before opening the browser */
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

void project_dialog_update(int window_w, int window_h) {
    if (!g_is_open) {
        return;
    }

    int dx, dy;
    ui_center_rect(window_w, window_h, DIALOG_WIDTH, DIALOG_HEIGHT, &dx, &dy);
    int close_x = dx + DIALOG_WIDTH - DIALOG_PADDING - CLOSE_BTN_SIZE;
    int close_y = dy + (TITLEBAR_HEIGHT - CLOSE_BTN_SIZE) / 2;
    if (window_mouse_left_just_pressed() &&
        ui_dialog_close_button_hit(window_mouse_x(), window_mouse_y(), close_x, close_y, CLOSE_BTN_SIZE)) {
        g_is_open = 0;
        g_name_field.focused = 0;
        g_path_field.focused = 0;
        window_stop_text_input();
        return;
    }

    int field_x = dx + DIALOG_PADDING;
    int field_w = DIALOG_WIDTH - DIALOG_PADDING * 2;

    int name_field_y = dy + 64;
    int path_field_y = dy + 128;

    /* The path field is slightly narrower to leave room for the Browse button beside it */
    int path_field_w = field_w - g_browse_btn.width - 8;

    text_field_update(&g_name_field, field_x, name_field_y, field_w, FIELD_HEIGHT, field_w);
    text_field_update(&g_path_field, field_x, path_field_y, path_field_w, FIELD_HEIGHT, path_field_w);

    /* Centralized logic to enable/disable text input handling - only once, after both fields update their focused state. Call here only, not inside each field, to ensure correct ordering (no conflicting enable/disable calls at the exact moment of switching focus between fields). */
    if (g_name_field.focused || g_path_field.focused) {
        window_start_text_input();
    } else {
        window_stop_text_input();
    }

    /* Path validity check is updated every frame (reflects any new typing immediately) */
    update_path_validation();

    /* Browse button */
    int browse_x = field_x + path_field_w + 8;
    int browse_y = path_field_y;
    if (window_mouse_left_just_pressed()) {
        int mx = window_mouse_x(), my = window_mouse_y();
        if (mx >= browse_x && mx < browse_x + g_browse_btn.width
            && my >= browse_y && my < browse_y + g_browse_btn.height) {
            /* Choose a folder from the whole filesystem - no extension filtering (meaningless when choosing a folder), result is delivered via on_browse_result */
            file_manager_open(FILE_MANAGER_ROOT_DEVICE, FILE_MANAGER_MODE_PICK_FOLDER,
                                NULL, 0, on_browse_result, NULL);
        }
    }

    int create_x = dx + DIALOG_WIDTH - DIALOG_PADDING - g_create_btn.width;
    int cancel_x = create_x - 8 - g_cancel_btn.width;
    int buttons_y = dy + DIALOG_HEIGHT - DIALOG_PADDING - g_cancel_btn.height;

    /* Cancel button */
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

    /* Create button - enabled only when the name is not empty and the path is valid */
    if (window_mouse_left_just_pressed()) {
        int mx = window_mouse_x(), my = window_mouse_y();
        if (mx >= create_x && mx < create_x + g_create_btn.width
            && my >= buttons_y && my < buttons_y + g_create_btn.height) {
            if (g_name_field.text[0] != '\0' && g_path_valid) {
                char full_path[600];
                snprintf(full_path, sizeof(full_path), "%s/%s",
                         g_path_field.text, g_name_field.text);
                const char *previous_project = current_project_get_path();
                int switching_project = previous_project != NULL && strcmp(previous_project, full_path) != 0;
                if (switching_project) {
                    for (int i = 0; i < scene_tabs_count(); i++) {
                        if (scene_tabs_get_dirty(i)) {
                            g_scene_switch_blocked = 1;
                            update_path_validation();
                            return;
                        }
                    }
                }
                project_create_result_t result =
                    project_create(g_name_field.text, g_path_field.text);
                if (result == PROJECT_CREATE_OK) {
                    if (switching_project && scene_tabs_count() > 0) {
                        scene_tabs_shutdown();
                        scene_tabs_init();
                    }
                    current_project_set_path(full_path);
                    project_catalog_add(full_path);

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

    ui_dialog_draw_backdrop(window_w, window_h);
    int dx, dy;
    ui_center_rect(window_w, window_h, DIALOG_WIDTH, DIALOG_HEIGHT, &dx, &dy);

    ui_dialog_draw_frame(dx, dy, DIALOG_WIDTH, DIALOG_HEIGHT);
    ui_dialog_draw_title_left(g_title_tex, g_title_w, g_title_h, dx, dy, TITLEBAR_HEIGHT, DIALOG_PADDING);
    ui_dialog_draw_close_button(dx + DIALOG_WIDTH - DIALOG_PADDING - CLOSE_BTN_SIZE,
                                dy + (TITLEBAR_HEIGHT - CLOSE_BTN_SIZE) / 2, CLOSE_BTN_SIZE);

    int field_x = dx + DIALOG_PADDING;
    int field_w = DIALOG_WIDTH - DIALOG_PADDING * 2;
    int name_field_y = dy + 64;
    int path_field_y = dy + 128;
    int path_field_w = field_w - g_browse_btn.width - 8;

    /* Label "Project name" directly above its field */
    if (g_label_name_tex != NULL) {
        window_draw_texture(g_label_name_tex, field_x, name_field_y - g_label_name_h - 4,
                             g_label_name_w, g_label_name_h);
    }
    text_field_draw(&g_name_field, field_x, name_field_y, field_w, FIELD_HEIGHT, FIELD_FONT_SIZE);

    /* Label "Project path" directly above its field */
    if (g_label_path_tex != NULL) {
        window_draw_texture(g_label_path_tex, field_x, path_field_y - g_label_path_h - 4,
                             g_label_path_w, g_label_path_h);
    }
    text_field_draw(&g_path_field, field_x, path_field_y, path_field_w, FIELD_HEIGHT, FIELD_FONT_SIZE);

    /* Browse button next to the path field */
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

    /* Path validation message - redrawn only if it actually changed */
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

    /* Cancel and Create buttons at the bottom of the dialog, right-aligned */
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
