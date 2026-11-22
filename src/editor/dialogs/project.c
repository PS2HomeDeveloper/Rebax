/*
 *  ____  _____ ____    _    __  __
 * |  _ \| ____| __ )  / \   \ \/ /
 * | |_) |  _| |  _ \ / _ \   \  / 
 * |  _ <| |___| |_) / ___ \  /  \ 
 * |_| \_\_____|____/_/   \_\/_/\_\
 *
 * Rebax Engine
 * Copyright (c) 2026 PS2HomeDeveloper
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#include "editor/dialogs/project.h"
#include "platform/common/window.h"
#include "editor/ui/labeled_button.h"
#include "editor/ui/font.h"
#include "editor/ui/text_field.h"
#include "project/current_project.h"
#include "project/project_create.h"
#include "project/project_catalog.h"
#include "project/project_file.h"
#include "project/project_types.h"
#include "editor/core/project_session.h"
#include "editor/panels/scene_tabs.h"
#include "editor/ui/loading_screen.h"
#include "editor/panels/file_manager.h"
#include "editor/ui/theme.h"
#include "editor/ui/base.h"
#include "editor/ui/dialog.h"

#define DIALOG_WIDTH   520
#define DIALOG_HEIGHT  428
#define DIALOG_PADDING 24
#define FIELD_HEIGHT   32
#define LABEL_FONT_SIZE 15
#define FIELD_FONT_SIZE 16
#define TITLE_FONT_SIZE 18
#define TITLEBAR_HEIGHT 44
#define CLOSE_BTN_SIZE 22

static const unsigned char COLOR_VALID_R = 0x5C, COLOR_VALID_G = 0xB8, COLOR_VALID_B = 0x5C;
static const unsigned char COLOR_INVALID_R = 0xD9, COLOR_INVALID_G = 0x5C, COLOR_INVALID_B = 0x5C;

static int g_is_open = 0;
static int g_ready = 0;

static text_field_t g_name_field;
static text_field_t g_path_field;

static int g_path_valid = 0;
static int g_scene_switch_blocked = 0;
static const char *g_create_error = NULL;
static char g_last_name[TEXT_FIELD_MAX_LEN];
static char g_last_path[TEXT_FIELD_MAX_LEN];

#define CHIP_HEIGHT 32
#define CHIP_GAP 8
static project_type_t g_type = PROJECT_TYPE_MODERN;
static dev_system_t g_dev = DEV_SYSTEM_NODES;
static window_texture_t *g_type_tex[PROJECT_TYPE_COUNT];
static int g_type_w[PROJECT_TYPE_COUNT], g_type_h[PROJECT_TYPE_COUNT];
static window_texture_t *g_type_hint_tex[PROJECT_TYPE_COUNT];
static int g_type_hint_w[PROJECT_TYPE_COUNT], g_type_hint_h[PROJECT_TYPE_COUNT];
static window_texture_t *g_dev_tex[DEV_SYSTEM_COUNT];
static int g_dev_w[DEV_SYSTEM_COUNT], g_dev_h[DEV_SYSTEM_COUNT];
static window_texture_t *g_soon_tex = NULL;
static int g_soon_w = 0, g_soon_h = 0;
static window_texture_t *g_label_type_tex = NULL;
static int g_label_type_w = 0, g_label_type_h = 0;
static window_texture_t *g_label_dev_tex = NULL;
static int g_label_dev_w = 0, g_label_dev_h = 0;
#define TYPE_ROW_Y 248
#define DEV_ROW_Y 320
static const char *g_path_message = "";

static window_texture_t *g_path_msg_tex = NULL;
static const char *g_path_msg_cached_text = NULL;
static int g_path_msg_cached_valid = -1;
static int g_path_msg_w = 0, g_path_msg_h = 0;

static labeled_button_t  g_browse_btn;
static labeled_button_t  g_cancel_btn;
static labeled_button_t  g_create_btn;
static window_texture_t *g_browse_btn_tex = NULL;
static window_texture_t *g_browse_txt_tex = NULL;
static window_texture_t *g_cancel_btn_tex = NULL;
static window_texture_t *g_cancel_txt_tex = NULL;
static window_texture_t *g_create_btn_tex = NULL;
static window_texture_t *g_create_txt_tex = NULL;

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

    g_label_type_tex = ui_make_text_texture("Project type", FONT_WEIGHT_REGULAR, LABEL_FONT_SIZE, &g_label_type_w, &g_label_type_h);
    g_label_dev_tex = ui_make_text_texture("Development system", FONT_WEIGHT_REGULAR, LABEL_FONT_SIZE, &g_label_dev_w, &g_label_dev_h);
    for (int i = 0; i < PROJECT_TYPE_COUNT; i++)
        g_type_tex[i] = ui_make_text_texture(project_type_label((project_type_t)i), FONT_WEIGHT_REGULAR, 14, &g_type_w[i], &g_type_h[i]);
    for (int i = 0; i < PROJECT_TYPE_COUNT; i++) {
        const char *hint = project_type_hint((project_type_t)i);
        g_type_hint_tex[i] = hint[0] ? ui_make_text_texture(hint, FONT_WEIGHT_REGULAR, 11, &g_type_hint_w[i], &g_type_hint_h[i]) : NULL;
    }
    for (int i = 0; i < DEV_SYSTEM_COUNT; i++)
        g_dev_tex[i] = ui_make_text_texture(dev_system_label((dev_system_t)i), FONT_WEIGHT_REGULAR, 14, &g_dev_w[i], &g_dev_h[i]);
    g_soon_tex = ui_make_text_texture("soon", FONT_WEIGHT_REGULAR, 11, &g_soon_w, &g_soon_h);

    ui_make_blue_button("Browse", LABEL_FONT_SIZE, &g_browse_btn, &g_browse_btn_tex, &g_browse_txt_tex);
    ui_make_blue_button("Cancel", LABEL_FONT_SIZE, &g_cancel_btn, &g_cancel_btn_tex, &g_cancel_txt_tex);
    ui_make_blue_button("Create", LABEL_FONT_SIZE, &g_create_btn, &g_create_btn_tex, &g_create_txt_tex);
}

void project_dialog_open(void) {
    ensure_ready();
    g_scene_switch_blocked = 0;
    g_create_error = NULL;
    g_last_name[0] = g_last_path[0] = '\0';
    g_type = PROJECT_TYPE_MODERN;
    g_dev = DEV_SYSTEM_NODES;
    g_is_open = 1;
}

int project_dialog_is_open(void) {
    return g_is_open;
}

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

static void update_path_validation(void) {
    if (strcmp(g_last_name, g_name_field.text) != 0 || strcmp(g_last_path, g_path_field.text) != 0) {
        g_create_error = NULL;
        g_scene_switch_blocked = 0;
        snprintf(g_last_name, sizeof(g_last_name), "%s", g_name_field.text);
        snprintf(g_last_path, sizeof(g_last_path), "%s", g_path_field.text);
    }
    if (g_create_error != NULL) {
        g_path_valid = 0;
        g_path_message = g_create_error;
    } else if (g_scene_switch_blocked) {
        g_path_valid = 0;
        g_path_message = "Save or close modified scenes before creating another project";
    } else if (g_name_field.text[0] != '\0' && project_name_error(g_name_field.text) != NULL) {
        g_path_valid = 0;
        g_path_message = project_name_error(g_name_field.text);
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

static void chip_rect(int index, int count, int field_x, int field_w, int row_y, int *x, int *w) {
    *w = (field_w - CHIP_GAP * (count - 1)) / count;
    *x = field_x + index * (*w + CHIP_GAP);
    (void)row_y;
}

static void draw_chip(window_texture_t *tex, int tw, int th, int x, int y, int w, int selected, int enabled) {
    ui_color_t chip = selected ? UI_COLOR_BUTTON_BLUE : UI_COLOR_SURFACE_CONTROL;
    unsigned char r = chip.r, g = chip.g, b = chip.b;
    window_fill_rect(x, y, w, CHIP_HEIGHT, r, g, b);
    if (tex == NULL) return;
    unsigned char c = enabled ? 0xF0 : 0x80;
    window_draw_texture_tinted(tex, x + (w - tw) / 2, y + (CHIP_HEIGHT - th) / 2, tw, th, c, c, c);
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

    int name_field_y = dy + 80;
    int path_field_y = dy + 148;

    int path_field_w = field_w - g_browse_btn.width - 8;

    text_field_update(&g_name_field, field_x, name_field_y, field_w, FIELD_HEIGHT, field_w);
    text_field_update(&g_path_field, field_x, path_field_y, path_field_w, FIELD_HEIGHT, path_field_w);

    if (g_name_field.focused || g_path_field.focused) {
        window_start_text_input();
    } else {
        window_stop_text_input();
    }

    update_path_validation();

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

    if (window_mouse_left_just_pressed()) {
        int mx = window_mouse_x(), my = window_mouse_y();
        for (int i = 0; i < PROJECT_TYPE_COUNT; i++) {
            int cx, cw;
            chip_rect(i, PROJECT_TYPE_COUNT, field_x, field_w, 0, &cx, &cw);
            if (mx >= cx && mx < cx + cw && my >= dy + TYPE_ROW_Y && my < dy + TYPE_ROW_Y + CHIP_HEIGHT) g_type = (project_type_t)i;
        }
        for (int i = 0; i < DEV_SYSTEM_COUNT; i++) {
            int cx, cw;
            chip_rect(i, DEV_SYSTEM_COUNT, field_x, field_w, 0, &cx, &cw);
            if (mx >= cx && mx < cx + cw && my >= dy + DEV_ROW_Y && my < dy + DEV_ROW_Y + CHIP_HEIGHT
                && dev_system_is_available((dev_system_t)i)) g_dev = (dev_system_t)i;
        }
    }

    int create_x = dx + DIALOG_WIDTH - DIALOG_PADDING - g_create_btn.width;
    int cancel_x = create_x - 8 - g_cancel_btn.width;
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

    if (window_mouse_left_just_pressed()) {
        int mx = window_mouse_x(), my = window_mouse_y();
        if (mx >= create_x && mx < create_x + g_create_btn.width
            && my >= buttons_y && my < buttons_y + g_create_btn.height) {
            if (g_name_field.text[0] != '\0' && g_path_valid) {
                char full_path[1100];
                snprintf(full_path, sizeof(full_path), "%s/%s", g_path_field.text, g_name_field.text);
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
                project_create_options_t options = {g_name_field.text, g_path_field.text, g_type, g_dev};
                project_create_result_t result = project_create(&options);
                char error[160];
                if (result != PROJECT_CREATE_OK) {
                    g_create_error = project_create_error_text(result);
                } else if (!project_session_open(full_path, error, sizeof(error))) {
                    g_create_error = "The project was created but could not be opened.";
                } else {
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
    int name_field_y = dy + 80;
    int path_field_y = dy + 148;
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
        window_draw_texture(g_browse_btn_tex, browse_x, browse_y,
                             g_browse_btn.width, g_browse_btn.height);
        window_draw_texture(g_browse_txt_tex,
                             browse_x + g_browse_btn.text_offset_x,
                             browse_y + g_browse_btn.text_offset_y,
                             g_browse_btn.text_img.width, g_browse_btn.text_img.height);
    }

    if (g_label_type_tex != NULL)
        window_draw_texture(g_label_type_tex, field_x, dy + TYPE_ROW_Y - g_label_type_h - 4, g_label_type_w, g_label_type_h);
    for (int i = 0; i < PROJECT_TYPE_COUNT; i++) {
        int cx, cw;
        chip_rect(i, PROJECT_TYPE_COUNT, field_x, field_w, 0, &cx, &cw);
        draw_chip(g_type_tex[i], g_type_w[i], g_type_h[i], cx, dy + TYPE_ROW_Y, cw, g_type == (project_type_t)i, 1);
    }
    if (g_type_hint_tex[g_type] != NULL)
        window_draw_texture_tinted(g_type_hint_tex[g_type], field_x, dy + TYPE_ROW_Y + CHIP_HEIGHT + 6, g_type_hint_w[g_type], g_type_hint_h[g_type], UI_RGB(UI_COLOR_TEXT_FAINT));
    if (g_label_dev_tex != NULL)
        window_draw_texture(g_label_dev_tex, field_x, dy + DEV_ROW_Y - g_label_dev_h - 4, g_label_dev_w, g_label_dev_h);
    for (int i = 0; i < DEV_SYSTEM_COUNT; i++) {
        int cx, cw, ok = dev_system_is_available((dev_system_t)i);
        chip_rect(i, DEV_SYSTEM_COUNT, field_x, field_w, 0, &cx, &cw);
        draw_chip(g_dev_tex[i], g_dev_w[i], g_dev_h[i], cx, dy + DEV_ROW_Y, cw, g_dev == (dev_system_t)i, ok);
        if (!ok && g_soon_tex != NULL)
            window_draw_texture_tinted(g_soon_tex, cx + cw - g_soon_w - 6, dy + DEV_ROW_Y + CHIP_HEIGHT - g_soon_h - 2,
                                       g_soon_w, g_soon_h, UI_RGB(UI_COLOR_WARNING));
    }

    if (g_path_message != g_path_msg_cached_text || g_path_valid != g_path_msg_cached_valid) {
        if (g_path_msg_tex != NULL) {
            window_destroy_texture(g_path_msg_tex);
            g_path_msg_tex = NULL;
        }
        font_text_image_t msg = font_render_text(g_path_message, FONT_WEIGHT_REGULAR, 13);
        if (msg.pixels != NULL) {
            g_path_msg_tex = window_create_texture(msg.pixels, msg.px_w, msg.px_h);
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
    if (g_label_type_tex) window_destroy_texture(g_label_type_tex);
    if (g_label_dev_tex) window_destroy_texture(g_label_dev_tex);
    if (g_soon_tex) window_destroy_texture(g_soon_tex);
    for (int i = 0; i < PROJECT_TYPE_COUNT; i++) if (g_type_tex[i]) window_destroy_texture(g_type_tex[i]);
    for (int i = 0; i < DEV_SYSTEM_COUNT; i++) if (g_dev_tex[i]) window_destroy_texture(g_dev_tex[i]);
    for (int i = 0; i < PROJECT_TYPE_COUNT; i++) if (g_type_hint_tex[i]) window_destroy_texture(g_type_hint_tex[i]);
    memset(g_type_hint_tex, 0, sizeof(g_type_hint_tex));
    memset(g_type_tex, 0, sizeof(g_type_tex));
    memset(g_dev_tex, 0, sizeof(g_dev_tex));
    g_soon_tex = g_label_type_tex = g_label_dev_tex = NULL;

    labeled_button_free(&g_browse_btn);
    labeled_button_free(&g_cancel_btn);
    labeled_button_free(&g_create_btn);
    text_field_free(&g_name_field);
    text_field_free(&g_path_field);

    g_is_open = 0;
    g_ready = 0;
}
