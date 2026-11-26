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
#include <strings.h>
#include <stddef.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>

#if defined(_WIN32)
#include <direct.h>
#define REBAX_MKDIR(path, mode) _mkdir(path)
#else
#define REBAX_MKDIR(path, mode) mkdir(path, mode)
#endif

#include "formats/image_decoder.h"

#include "editor/panels/file_manager.h"
#include "platform/common/window.h"
#include "editor/ui/shape_provider.h"
#include "editor/ui/icon_atlas.h"
#include "editor/ui/labeled_button.h"
#include "editor/ui/text_field.h"
#include "editor/ui/font.h"
#include "project/current_project.h"
#include "editor/ui/theme.h"
#include "editor/ui/base.h"
#include "editor/ui/selection.h"
#include "editor/ui/dialog.h"
#include "editor/ui/windows.h"
#include "filesystem/path_utils.h"
#include "editor/ui/file_icons.h"
#include "platform/android/storage.h"
#include "editor/ui/draw_kit.h"
#include "editor/ui/scrollbar.h"
#include "core/i18n.h"
#include "filesystem/storage_volumes.h"
#include "filesystem/rebax_fs.h"

#define DEFAULT_W           640
#define DEFAULT_H           500
#define DIALOG_WIDTH        g_dw
#define DIALOG_HEIGHT       g_dh
static int g_dw = DEFAULT_W, g_dh = DEFAULT_H;
static ui_dialog_drag_t g_drag;
static ui_window_t g_win = UI_WINDOW_INIT(file_manager_is_open, file_manager_update, file_manager_draw);
#define DIALOG_PADDING         16
#define TITLEBAR_HEIGHT        36
#define CLOSE_BTN_SIZE          22
#define CRUMB_BAR_HEIGHT        28
#define TOOLBAR_HEIGHT          34
#define GRID_TOP_GAP             8
#define FOOTER_HEIGHT           48
#define FILENAME_ROW_HEIGHT     36

#define CELL_W                  84
#define CELL_H                  96
#define GRID_ICON_SIZE          48
#define CELL_CORNER_RADIUS       8
#define LABEL_FONT_SIZE         14

#define SCROLL_STEP_PX           40

#define AB_MAX_ENTRIES        20000
#define THUMB_BUDGET_TICKS        6
#define AB_MAX_PATH              900
#define AB_MAX_EXTS               16
#define AB_MAX_CRUMBS             24

#define THUMB_MAX_FILE_SIZE (8 * 1024 * 1024)

typedef struct {
    char full_path[AB_MAX_PATH];
    char display_name[160];
    int  is_dir;

    window_texture_t *thumb_tex;
    int  thumb_w, thumb_h;

    window_texture_t *label_tex;
    int  label_w, label_h;
    int  thumb_tried;
} ab_entry_t;

static int g_ready = 0;
static int g_is_open = 0;

static file_manager_root_t g_root_mode;
static file_manager_mode_t g_pick_mode;
static file_manager_callback_t g_callback = NULL;
static void *g_user_data = NULL;

static char g_extensions[AB_MAX_EXTS][16];
static int  g_extension_count = 0;

static char g_root_path[AB_MAX_PATH];
static char g_root_label[64];
static char g_current_path[AB_MAX_PATH];
static int g_skip_input_frames = 0;

static ab_entry_t *g_entries = NULL;
static int g_entry_capacity = 0;
static int g_entry_count = 0;
static int g_scroll_dragging = 0;
static int g_press_index = -1;
static int g_press_moved = 0;
static int g_rename_index = -1;
static int g_rename_is_new = 0;
static text_field_t g_rename_field;
static int g_rename_ready = 0;
static char g_inline_error[120];

static char g_crumb_label[AB_MAX_CRUMBS][64];
static char g_crumb_path[AB_MAX_CRUMBS][AB_MAX_PATH];
static window_texture_t *g_crumb_tex[AB_MAX_CRUMBS];
static int  g_crumb_w[AB_MAX_CRUMBS], g_crumb_h[AB_MAX_CRUMBS];
static int  g_crumb_x[AB_MAX_CRUMBS], g_crumb_w_click[AB_MAX_CRUMBS];
static int  g_crumb_count = 0;
static window_texture_t *g_crumb_sep_tex = NULL;
static int  g_crumb_sep_w = 0, g_crumb_sep_h = 0;

static text_field_t g_search_field;

static text_field_t g_filename_field;
static window_texture_t *g_filename_label_tex = NULL;
static int g_filename_label_w = 0, g_filename_label_h = 0;

typedef enum { PROMPT_NONE, PROMPT_NEW_FOLDER, PROMPT_RENAME, PROMPT_DELETE } prompt_t;
static prompt_t g_prompt = PROMPT_NONE;
static text_field_t g_prompt_field;
static int g_prompt_ready = 0;
static char g_prompt_error[120];
static int g_manage = 0;
static int g_in_devices = 0;

static int g_selected_index = -1;
static int g_scroll_offset = 0;

static window_texture_t *g_title_tex = NULL;
static int g_title_w = 0, g_title_h = 0;

static window_texture_t *g_filter_label_tex = NULL;
static int g_filter_label_w = 0, g_filter_label_h = 0;

static labeled_button_t  g_cancel_btn, g_select_btn;
static window_texture_t *g_cancel_btn_tex = NULL, *g_cancel_txt_tex = NULL;
static window_texture_t *g_select_btn_tex = NULL, *g_select_txt_tex = NULL;

static window_texture_t *g_hover_tex = NULL;
static window_texture_t *g_selected_tex = NULL;

static int extension_matches_filter(const char *name) {
    if (g_extension_count <= 0) return 1;
    const char *dot = strrchr(name, '.');
    if (dot == NULL) return 0;
    const char *ext = dot + 1;
    for (int i = 0; i < g_extension_count; i++) {
        if (strcasecmp(ext, g_extensions[i]) == 0) return 1;
    }
    return 0;
}

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

static void cancel_rename(void) {
    if (g_rename_index < 0) return;
    g_rename_index = -1;
    g_rename_is_new = 0;
    window_stop_text_input();
}

static void free_entries(void) {
    cancel_rename();
    g_press_index = -1;
    for (int i = 0; i < g_entry_count; i++) {
        if (g_entries[i].thumb_tex != NULL) window_destroy_texture(g_entries[i].thumb_tex);
        if (g_entries[i].label_tex != NULL) window_destroy_texture(g_entries[i].label_tex);
    }
    g_entry_count = 0;
}

static void add_entry(const char *full_path, const char *display_name, int is_dir) {
    if (g_entry_count >= AB_MAX_ENTRIES) return;
    if (g_entry_count >= g_entry_capacity) {
        int grown_capacity = g_entry_capacity > 0 ? g_entry_capacity * 2 : 256;
        ab_entry_t *grown = realloc(g_entries, (size_t)grown_capacity * sizeof(*grown));
        if (grown == NULL) return;
        g_entries = grown;
        g_entry_capacity = grown_capacity;
    }

    ab_entry_t *e = &g_entries[g_entry_count++];
    memset(e, 0, sizeof(*e));
    snprintf(e->full_path, sizeof(e->full_path), "%s", full_path);
    snprintf(e->display_name, sizeof(e->display_name), "%s", display_name);
    e->is_dir = is_dir;
}

static void prepare_entry_visuals(ab_entry_t *e, unsigned int started) {
    if (e->label_tex == NULL) {
        char fitted[200];
        ui_kit_fit_text(e->display_name, LABEL_FONT_SIZE, CELL_W - 8, fitted, (int)sizeof(fitted));
        font_text_image_t txt = font_render_text(fitted, FONT_WEIGHT_REGULAR, LABEL_FONT_SIZE);
        if (txt.pixels != NULL) {
            e->label_tex = window_create_texture(txt.pixels, txt.px_w, txt.px_h);
            e->label_w = txt.width;
            e->label_h = txt.height;
            font_free_text_image(&txt);
        }
    }
    if (e->is_dir || e->thumb_tried || e->thumb_tex != NULL) return;
    if (window_ticks() - started > THUMB_BUDGET_TICKS) return;
    e->thumb_tried = 1;
    const char *dot = strrchr(e->display_name, '.');
    if (dot != NULL && file_icons_is_raster_image_ext(dot + 1)) {
        struct stat st;
        if (stat(e->full_path, &st) == 0 && st.st_size > 0 && st.st_size <= THUMB_MAX_FILE_SIZE) {
            int w;
            int h;
            unsigned char *pixels = image_loader_editor_load(e->full_path, &w, &h, 0, 0, 0);
            if (pixels != NULL) {
                e->thumb_tex = make_thumbnail_texture(pixels, w, h, GRID_ICON_SIZE, &e->thumb_w, &e->thumb_h);
                free(pixels);
            }
        }
    }
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
        g_crumb_tex[i] = ui_make_text_texture(g_crumb_label[i], FONT_WEIGHT_REGULAR, 13,
                                            &g_crumb_w[i], &g_crumb_h[i]);
    }
}

static void load_directory(const char *path) {
    g_in_devices = 0;
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

    char **dir_names = malloc(sizeof(char *) * AB_MAX_ENTRIES);
    char **file_names = malloc(sizeof(char *) * AB_MAX_ENTRIES);
    int dir_count = 0, file_count = 0;
    if (dir_names == NULL || file_names == NULL) {
        free(dir_names);
        free(file_names);
        closedir(d);
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(d)) != NULL) {
        if (entry->d_name[0] == '.') continue;

        char full[AB_MAX_PATH];
        snprintf(full, sizeof(full), "%s/%s", path, entry->d_name);

        struct stat st;
        if (stat(full, &st) != 0) continue;

        if (S_ISDIR(st.st_mode)) {
            if (dir_count < AB_MAX_ENTRIES) dir_names[dir_count++] = strdup(entry->d_name);
        } else {
            if (g_pick_mode == FILE_MANAGER_MODE_PICK_FILE
                && !extension_matches_filter(entry->d_name)) {
                continue;
            }
            if (file_count < AB_MAX_ENTRIES) file_names[file_count++] = strdup(entry->d_name);
        }
    }
    closedir(d);

    qsort(dir_names, dir_count, sizeof(char *), path_utils_compare_names_ci);
    qsort(file_names, file_count, sizeof(char *), path_utils_compare_names_ci);

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
    free(dir_names);
    free(file_names);

    strncpy(g_current_path, path, sizeof(g_current_path) - 1);
    g_current_path[sizeof(g_current_path) - 1] = '\0';
    rebuild_breadcrumb();
}

static void load_devices(void) {
    free_entries();
    g_selected_index = -1;
    g_scroll_offset = 0;
    g_manage = 0;
    storage_volume_t vols[24];
    int n = storage_volumes_list(vols, 24);
    for (int i = 0; i < n; i++) add_entry(vols[i].path, vols[i].label, 1);
    free_crumb_textures();
    snprintf(g_crumb_label[0], sizeof(g_crumb_label[0]), "Devices");
    g_crumb_path[0][0] = '\0';
    g_crumb_count = 1;
    g_crumb_tex[0] = ui_make_text_texture(g_crumb_label[0], FONT_WEIGHT_REGULAR, 13, &g_crumb_w[0], &g_crumb_h[0]);
    g_in_devices = 1;
}

static int valid_entry_name(const char *name) {
    if (!name[0] || strcmp(name, ".") == 0 || strcmp(name, "..") == 0) return 0;
    return strpbrk(name, "/\\:*?\"<>|") == NULL;
}

static void run_prompt_action(void) {
    const char *name = g_prompt_field.text;
    char target[AB_MAX_PATH];
    g_prompt_error[0] = '\0';
    if (g_prompt == PROMPT_DELETE) {
        if (g_selected_index >= 0 && g_selected_index < g_entry_count) {
            char path[AB_MAX_PATH];
            snprintf(path, sizeof(path), "%s", g_entries[g_selected_index].full_path);
            if (!rebax_fs_remove_recursive(path)) { snprintf(g_prompt_error, sizeof(g_prompt_error), "Could not delete it"); return; }
        }
    } else {
        if (!valid_entry_name(name)) { snprintf(g_prompt_error, sizeof(g_prompt_error), "This name is not valid"); return; }
        snprintf(target, sizeof(target), "%s/%s", g_current_path, name);
        if (rebax_fs_exists(target)) { snprintf(g_prompt_error, sizeof(g_prompt_error), "This name already exists"); return; }
        if (g_prompt == PROMPT_NEW_FOLDER) {
            if (!rebax_fs_mkdir_p(target)) { snprintf(g_prompt_error, sizeof(g_prompt_error), "Could not create the folder"); return; }
        } else if (g_selected_index >= 0 && g_selected_index < g_entry_count) {
            if (rename(g_entries[g_selected_index].full_path, target) != 0) { snprintf(g_prompt_error, sizeof(g_prompt_error), "Could not rename it"); return; }
        }
    }
    g_prompt = PROMPT_NONE;
    window_stop_text_input();
    char here[AB_MAX_PATH];
    snprintf(here, sizeof(here), "%s", g_current_path);
    load_directory(here);
}

static int g_scroll_to = -1;

static void begin_rename(int index, int is_new) {
    if (index < 0 || index >= g_entry_count) return;
    if (!g_rename_ready) {
        g_rename_ready = 1;
        text_field_init(&g_rename_field);
    }
    g_rename_index = index;
    g_rename_is_new = is_new;
    g_inline_error[0] = '\0';
    snprintf(g_rename_field.text, sizeof(g_rename_field.text), "%s", g_entries[index].display_name);
    g_rename_field.cursor_pos = (int)strlen(g_rename_field.text);
    g_rename_field.scroll_offset = 0;
    g_rename_field.focused = 1;
    g_scroll_to = index;
    window_start_text_input();
}

static void reload_current(void) {
    char here[AB_MAX_PATH];
    snprintf(here, sizeof(here), "%s", g_current_path);
    load_directory(here);
}

static void commit_rename(void) {
    if (g_rename_index < 0 || g_rename_index >= g_entry_count) {
        cancel_rename();
        return;
    }
    char old_path[AB_MAX_PATH];
    char target[AB_MAX_PATH];
    char name[TEXT_FIELD_MAX_LEN];
    snprintf(old_path, sizeof(old_path), "%s", g_entries[g_rename_index].full_path);
    snprintf(name, sizeof(name), "%s", g_rename_field.text);
    int unchanged = strcmp(name, g_entries[g_rename_index].display_name) == 0;
    cancel_rename();
    if (unchanged) return;
    g_inline_error[0] = '\0';
    if (!valid_entry_name(name)) {
        snprintf(g_inline_error, sizeof(g_inline_error), "This name is not valid");
        return;
    }
    snprintf(target, sizeof(target), "%s/%s", g_current_path, name);
    if (rebax_fs_exists(target)) {
        snprintf(g_inline_error, sizeof(g_inline_error), "This name already exists");
        return;
    }
    if (rename(old_path, target) != 0) {
        snprintf(g_inline_error, sizeof(g_inline_error), "Could not rename it");
        return;
    }
    reload_current();
    for (int i = 0; i < g_entry_count; i++) {
        if (strcmp(g_entries[i].display_name, name) == 0) {
            g_selected_index = g_manage ? i : -1;
            g_scroll_to = i;
        }
    }
}

static void begin_new_folder(void) {
    char name[160];
    char target[AB_MAX_PATH];
    snprintf(name, sizeof(name), "%s", tr("New folder"));
    for (int n = 2;; n++) {
        snprintf(target, sizeof(target), "%s/%s", g_current_path, name);
        if (!rebax_fs_exists(target)) break;
        if (n > 999) return;
        snprintf(name, sizeof(name), "%s %d", tr("New folder"), n);
    }
    g_inline_error[0] = '\0';
    if (!rebax_fs_mkdir_p(target)) {
        snprintf(g_inline_error, sizeof(g_inline_error), "Could not create the folder");
        return;
    }
    reload_current();
    for (int i = 0; i < g_entry_count; i++) {
        if (strcmp(g_entries[i].display_name, name) == 0) {
            begin_rename(i, 1);
            return;
        }
    }
}

static void activate_entry(int i) {
    ab_entry_t *e = &g_entries[i];
    if (g_manage) {
        g_selected_index = i;
    } else if (e->is_dir) {
        char next_path[AB_MAX_PATH];
        snprintf(next_path, sizeof(next_path), "%s", e->full_path);
        g_search_field.text[0] = '\0';
        g_search_field.cursor_pos = 0;
        g_search_field.scroll_offset = 0;
        load_directory(next_path);
    } else if (g_pick_mode == FILE_MANAGER_MODE_PICK_FILE) {
        g_selected_index = i;
    }
}

static void open_prompt(prompt_t kind) {
    if (!g_prompt_ready) { g_prompt_ready = 1; text_field_init(&g_prompt_field); }
    g_prompt = kind;
    g_prompt_error[0] = '\0';
    g_prompt_field.text[0] = '\0';
    if (kind == PROMPT_RENAME && g_selected_index >= 0 && g_selected_index < g_entry_count)
        snprintf(g_prompt_field.text, sizeof(g_prompt_field.text), "%s", g_entries[g_selected_index].display_name);
    g_prompt_field.cursor_pos = (int)strlen(g_prompt_field.text);
    g_prompt_field.scroll_offset = 0;
    g_prompt_field.focused = (kind != PROMPT_DELETE);
    if (g_prompt_field.focused) window_start_text_input();
}

typedef struct { int x, y, w, h; } fm_rect_t;
#define PROMPT_W 440
#define PROMPT_H 130

static fm_rect_t prompt_rect(int dx, int dy) {
    fm_rect_t r = { dx + (DIALOG_WIDTH - PROMPT_W) / 2, dy + 150, PROMPT_W, PROMPT_H };
    return r;
}

static void tool_rects(int dx, int toolbar_y, fm_rect_t out[5]) {
    static const char *labels[5] = { "Devices", "New folder", "Manage", "Rename", "Delete" };
    int right = dx + DIALOG_WIDTH - DIALOG_PADDING;
    int count = g_manage ? 5 : 3;
    int x = right;
    for (int i = count - 1; i >= 0; i--) {
        int w = ui_kit_text_width(labels[i], 13) + 22;
        x -= w;
        out[i].x = x; out[i].y = toolbar_y + 1; out[i].w = w; out[i].h = TOOLBAR_HEIGHT - 8;
        x -= 6;
    }
}

static void ensure_ready(void) {
    if (g_ready) return;
    g_ready = 1;

    font_init();
    icon_atlas_init();
    text_field_init(&g_search_field);
    text_field_init(&g_filename_field);
    g_filename_label_tex = ui_make_text_texture("File name:", FONT_WEIGHT_REGULAR, 13,
                                              &g_filename_label_w, &g_filename_label_h);

    ui_make_blue_button("Cancel", 15, &g_cancel_btn, &g_cancel_btn_tex, &g_cancel_txt_tex);
    ui_make_blue_button("Select", 15, &g_select_btn, &g_select_btn_tex, &g_select_txt_tex);

    g_crumb_sep_tex = ui_make_text_texture("/", FONT_WEIGHT_REGULAR, 13, &g_crumb_sep_w, &g_crumb_sep_h);

    g_hover_tex = ui_selection_make_hover(CELL_W - 4, CELL_H - 4, CELL_CORNER_RADIUS);
    g_selected_tex = ui_selection_make_selected(CELL_W - 4, CELL_H - 4, CELL_CORNER_RADIUS);

}

static int open_common(file_manager_root_t root, file_manager_callback_t callback, void *user_data) {
    ensure_ready();

    g_root_mode = root;
    g_callback = callback;
    g_user_data = user_data;

    if (root == FILE_MANAGER_ROOT_ASSETS || root == FILE_MANAGER_ROOT_SOURCE) {
        const char *project_path = current_project_get_path();
        if (project_path == NULL) {
            if (callback != NULL) callback(NULL, user_data);
            return 0;
        }
        const char *dir_name = root == FILE_MANAGER_ROOT_SOURCE ? "source" : "assets";
        snprintf(g_root_path, sizeof(g_root_path), "%s/%s", project_path, dir_name);
        struct stat st;
        if (stat(g_root_path, &st) != 0) REBAX_MKDIR(g_root_path, 0755);
        strncpy(g_root_label, root == FILE_MANAGER_ROOT_SOURCE ? "Source://" : "Assets://", sizeof(g_root_label) - 1);
        g_root_label[sizeof(g_root_label) - 1] = '\0';
    } else {
        strncpy(g_root_path, "/", sizeof(g_root_path) - 1);
        g_root_path[sizeof(g_root_path) - 1] = '\0';
        strncpy(g_root_label, "/", sizeof(g_root_label) - 1);
        g_root_label[sizeof(g_root_label) - 1] = '\0';
    }

    g_search_field.text[0] = '\0';
    g_search_field.cursor_pos = 0;
    g_search_field.scroll_offset = 0;

    if (root == FILE_MANAGER_ROOT_DEVICE) {
#if defined(__ANDROID__)
        load_directory("/storage/emulated/0");
#else
        struct stat st;
        const char *home = getenv("HOME");
        if (stat("/storage/emulated/0", &st) == 0 && S_ISDIR(st.st_mode)) {
            load_directory("/storage/emulated/0");
        } else if (home != NULL && stat(home, &st) == 0 && S_ISDIR(st.st_mode)) {
            load_directory(home);
        } else {
            load_directory(g_root_path);
        }
#endif
    } else {
        load_directory(g_root_path);
    }

    g_manage = 0;
    g_prompt = PROMPT_NONE;
    g_skip_input_frames = 2;
    g_is_open = 1;
    ui_dialog_drag_reset(&g_drag);
    ui_windows_show(&g_win);
    return 1;
}

void file_manager_open(file_manager_root_t root, file_manager_mode_t mode,
                         const char **extensions, int extension_count,
                         file_manager_callback_t callback, void *user_data) {
    android_storage_request_if_needed();
    g_pick_mode = mode;

    g_extension_count = 0;
    if (mode == FILE_MANAGER_MODE_PICK_FILE) {
        for (int i = 0; i < extension_count && i < AB_MAX_EXTS; i++) {
            strncpy(g_extensions[g_extension_count], extensions[i], sizeof(g_extensions[0]) - 1);
            g_extensions[g_extension_count][sizeof(g_extensions[0]) - 1] = '\0';
            g_extension_count++;
        }
    }

    if (!open_common(root, callback, user_data)) return;

    if (g_title_tex != NULL) { window_destroy_texture(g_title_tex); g_title_tex = NULL; }
    g_title_tex = ui_make_text_texture(mode == FILE_MANAGER_MODE_PICK_FOLDER ? "Select Folder" : "Select File",
                                     FONT_WEIGHT_BOLD, 17, &g_title_w, &g_title_h);

    if (g_select_btn_tex != NULL) { window_destroy_texture(g_select_btn_tex); g_select_btn_tex = NULL; }
    if (g_select_txt_tex != NULL) { window_destroy_texture(g_select_txt_tex); g_select_txt_tex = NULL; }
    ui_make_blue_button("Select", 15, &g_select_btn, &g_select_btn_tex, &g_select_txt_tex);

    if (g_filter_label_tex != NULL) { window_destroy_texture(g_filter_label_tex); g_filter_label_tex = NULL; }
    if (g_extension_count > 0) {
        char joined[160] = "";
        for (int i = 0; i < g_extension_count; i++) {
            char part[32];
            snprintf(part, sizeof(part), "%s*.%s", (i > 0) ? "  " : "", g_extensions[i]);
            strncat(joined, part, sizeof(joined) - strlen(joined) - 1);
        }
        g_filter_label_tex = ui_make_text_texture(joined, FONT_WEIGHT_REGULAR, 12,
                                                &g_filter_label_w, &g_filter_label_h);
    }
}

void file_manager_open_save(file_manager_root_t root, const char *default_filename,
                              file_manager_callback_t callback, void *user_data) {
    android_storage_request_if_needed();
    g_pick_mode = FILE_MANAGER_MODE_SAVE_FILE;
    g_extension_count = 0;

    if (!open_common(root, callback, user_data)) return;

    if (g_title_tex != NULL) { window_destroy_texture(g_title_tex); g_title_tex = NULL; }
    g_title_tex = ui_make_text_texture("Save As", FONT_WEIGHT_BOLD, 17, &g_title_w, &g_title_h);

    if (g_select_btn_tex != NULL) { window_destroy_texture(g_select_btn_tex); g_select_btn_tex = NULL; }
    if (g_select_txt_tex != NULL) { window_destroy_texture(g_select_txt_tex); g_select_txt_tex = NULL; }
    ui_make_blue_button("Save", 15, &g_select_btn, &g_select_btn_tex, &g_select_txt_tex);

    if (g_filter_label_tex != NULL) { window_destroy_texture(g_filter_label_tex); g_filter_label_tex = NULL; }

    g_filename_field.text[0] = '\0';
    if (default_filename != NULL) {
        strncpy(g_filename_field.text, default_filename, TEXT_FIELD_MAX_LEN - 1);
        g_filename_field.text[TEXT_FIELD_MAX_LEN - 1] = '\0';
    }
    g_filename_field.cursor_pos = (int)strlen(g_filename_field.text);
    g_filename_field.scroll_offset = 0;
}

int file_manager_is_open(void) { return g_is_open; }

static void finish(const char *picked_path) {
    file_manager_callback_t cb = g_callback;
    void *ud = g_user_data;
    g_is_open = 0;
    g_callback = NULL;
    g_user_data = NULL;
    window_stop_text_input();
    if (cb != NULL) cb(picked_path, ud);
}

void file_manager_update(int window_w, int window_h) {
    if (!g_is_open) return;
    if (g_skip_input_frames > 0) { g_skip_input_frames--; return; }

    int dx, dy;
    ui_dialog_place(&g_drag, 1, window_w, window_h, DEFAULT_W, DEFAULT_H, DEFAULT_W, DEFAULT_H, TITLEBAR_HEIGHT, 1, &dx, &dy, &g_dw, &g_dh);

    int mx = window_mouse_x(), my = window_mouse_y();

    int close_x = dx + DIALOG_WIDTH - DIALOG_PADDING - CLOSE_BTN_SIZE;
    int close_y = dy + (TITLEBAR_HEIGHT - CLOSE_BTN_SIZE) / 2;
    if (window_mouse_left_just_pressed()
        && mx >= close_x && mx < close_x + CLOSE_BTN_SIZE
        && my >= close_y && my < close_y + CLOSE_BTN_SIZE) {
        finish(NULL);
        return;
    }

    if (g_prompt != PROMPT_NONE) {
        fm_rect_t pr = prompt_rect(dx, dy);
        int ok_x = pr.x + pr.w - 16 - 100, btn_y = pr.y + pr.h - 16 - 34;
        int cancel_x2 = ok_x - 10 - 100;
        if (g_prompt != PROMPT_DELETE) {
            int was = g_prompt_field.focused;
            text_field_update(&g_prompt_field, pr.x + 16, pr.y + 40, pr.w - 32, 30, pr.w - 32);
            if (g_prompt_field.focused != was) { if (g_prompt_field.focused) window_start_text_input(); else window_stop_text_input(); }
        }
        if (window_mouse_left_just_pressed()) {
            if (ui_kit_clicked(ok_x, btn_y, 100, 34)) run_prompt_action();
            else if (ui_kit_clicked(cancel_x2, btn_y, 100, 34)) { g_prompt = PROMPT_NONE; window_stop_text_input(); }
        }
        return;
    }

    int toolbar_y0 = dy + TITLEBAR_HEIGHT + CRUMB_BAR_HEIGHT;
    {
        fm_rect_t tr[5];
        tool_rects(dx, toolbar_y0, tr);
        int sel_ok = g_selected_index >= 0 && g_selected_index < g_entry_count;
        int can_edit = !g_in_devices;
        if (g_root_mode == FILE_MANAGER_ROOT_DEVICE && ui_kit_clicked(tr[0].x, tr[0].y, tr[0].w, tr[0].h)) { load_devices(); return; }
        if (can_edit && ui_kit_clicked(tr[1].x, tr[1].y, tr[1].w, tr[1].h)) { begin_new_folder(); return; }
        if (can_edit && ui_kit_clicked(tr[2].x, tr[2].y, tr[2].w, tr[2].h)) { g_manage = !g_manage; g_selected_index = -1; return; }
        if (g_manage && sel_ok && ui_kit_clicked(tr[3].x, tr[3].y, tr[3].w, tr[3].h)) { begin_rename(g_selected_index, 0); return; }
        if (g_manage && sel_ok && ui_kit_clicked(tr[4].x, tr[4].y, tr[4].w, tr[4].h)) { open_prompt(PROMPT_DELETE); return; }
    }

    int crumb_y = dy + TITLEBAR_HEIGHT;
    int crumb_x = dx + DIALOG_PADDING;
    for (int i = 0; i < g_crumb_count; i++) {
        g_crumb_x[i] = crumb_x;
        g_crumb_w_click[i] = g_crumb_w[i];
        crumb_x += g_crumb_w[i];
        if (i < g_crumb_count - 1) crumb_x += g_crumb_sep_w + 10;
    }
    if (window_mouse_left_just_pressed() && my >= crumb_y && my < crumb_y + CRUMB_BAR_HEIGHT) {
        for (int i = 0; i < g_crumb_count - 1; i++) {
            if (mx >= g_crumb_x[i] && mx < g_crumb_x[i] + g_crumb_w_click[i]) {
                load_directory(g_crumb_path[i]);
                break;
            }
        }
    }

    int toolbar_y = crumb_y + CRUMB_BAR_HEIGHT;
    int search_x = dx + DIALOG_PADDING;
    int search_w = 220;
    text_field_update(&g_search_field, search_x + 20, toolbar_y, search_w - 20, TOOLBAR_HEIGHT - 6, search_w - 20);

    if (g_search_field.focused) window_start_text_input();
    else if (g_rename_index < 0) window_stop_text_input();

    int grid_x = dx + DIALOG_PADDING;
    int grid_y = toolbar_y + TOOLBAR_HEIGHT + GRID_TOP_GAP;
    int grid_w = DIALOG_WIDTH - DIALOG_PADDING * 2;
    int filename_row_reserve = (g_pick_mode == FILE_MANAGER_MODE_SAVE_FILE) ? FILENAME_ROW_HEIGHT : 0;
    int grid_h = DIALOG_HEIGHT - (grid_y - dy) - FOOTER_HEIGHT - DIALOG_PADDING - filename_row_reserve;

    int columns = (grid_w - UI_SCROLLBAR_WIDTH - 4) / CELL_W;
    if (columns < 1) columns = 1;

    int visible_count = 0;
    int scroll_row = -1;
    for (int i = 0; i < g_entry_count; i++) {
        if (!ui_text_contains_ci(g_entries[i].display_name, g_search_field.text)) continue;
        if (i == g_scroll_to) scroll_row = visible_count / columns;
        visible_count++;
    }
    int total_rows = (visible_count + columns - 1) / columns;
    int content_h = total_rows * CELL_H;
    if (scroll_row >= 0) {
        if (scroll_row * CELL_H < g_scroll_offset) g_scroll_offset = scroll_row * CELL_H;
        if ((scroll_row + 1) * CELL_H > g_scroll_offset + grid_h) g_scroll_offset = (scroll_row + 1) * CELL_H - grid_h;
    }
    g_scroll_to = -1;
    if (g_rename_index < 0) {
        ui_scrollbar_update(&g_scroll_offset, &g_scroll_dragging, grid_x, grid_w, grid_y, grid_h, content_h, SCROLL_STEP_PX);
    }

    int touch = window_pointer_is_touch();
    int pressed = window_mouse_left_just_pressed() && !g_scroll_dragging && g_rename_index < 0;
    int activate = -1;
    int rename_x = 0;
    int rename_y = 0;
    int visible_index = 0;
    for (int i = 0; i < g_entry_count; i++) {
        ab_entry_t *e = &g_entries[i];
        if (!ui_text_contains_ci(e->display_name, g_search_field.text)) continue;

        int col = visible_index % columns;
        int row = visible_index / columns;
        int cell_x = grid_x + col * CELL_W;
        int cell_y = grid_y + row * CELL_H - g_scroll_offset;
        visible_index++;
        if (i == g_rename_index) {
            rename_x = cell_x;
            rename_y = cell_y;
        }

        if (cell_y + CELL_H < grid_y || cell_y > grid_y + grid_h) continue;

        int inside = (mx >= cell_x && mx < cell_x + CELL_W && my >= cell_y && my < cell_y + CELL_H
                      && my >= grid_y && my < grid_y + grid_h);
        if (inside && pressed) {
            if (touch) {
                g_press_index = i;
                g_press_moved = 0;
            } else {
                activate = i;
                break;
            }
        }
    }
    if (touch) {
        if (g_scroll_dragging == 2) g_press_moved = 1;
        if (g_press_index >= 0 && !window_mouse_left_down()) {
            if (!g_press_moved) activate = g_press_index;
            g_press_index = -1;
        }
    }
    if (activate >= 0 && activate < g_entry_count) activate_entry(activate);

    if (g_rename_index >= 0) {
        int field_x = rename_x + 2;
        int field_y = rename_y + 8 + GRID_ICON_SIZE + 4;
        window_start_text_input();
        text_field_update(&g_rename_field, field_x, field_y, CELL_W - 4, 24, CELL_W - 4);
        if (window_key_just_pressed_escape()) {
            cancel_rename();
        } else if (window_key_just_pressed_enter() || !g_rename_field.focused) {
            commit_rename();
        }
        return;
    }

    int can_select = 0;
    char final_path[AB_MAX_PATH];
    final_path[0] = '\0';
    if (g_in_devices) {
        can_select = 0;
    } else if (g_pick_mode == FILE_MANAGER_MODE_SAVE_FILE) {
        int buttons_y_early = dy + DIALOG_HEIGHT - DIALOG_PADDING - g_cancel_btn.height;
        int filename_row_y = buttons_y_early - FILENAME_ROW_HEIGHT;
        int field_label_w = 70;
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
                final_path[sizeof(final_path) - 1] = '\0';
            }
        }
    } else if (g_pick_mode == FILE_MANAGER_MODE_PICK_FOLDER) {
        can_select = 1;
        strncpy(final_path, g_current_path, sizeof(final_path) - 1);
    } else if (g_selected_index >= 0 && g_selected_index < g_entry_count
               && !g_entries[g_selected_index].is_dir) {
        can_select = 1;
        strncpy(final_path, g_entries[g_selected_index].full_path, sizeof(final_path) - 1);
    }

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

void file_manager_draw(int window_w, int window_h) {
    if (!g_is_open) return;

    int dx, dy;
    ui_dialog_place(&g_drag, 0, window_w, window_h, DEFAULT_W, DEFAULT_H, DEFAULT_W, DEFAULT_H, TITLEBAR_HEIGHT, 1, &dx, &dy, &g_dw, &g_dh);

    ui_dialog_draw_backdrop(window_w, window_h);

    ui_dialog_draw_frame(dx, dy, DIALOG_WIDTH, DIALOG_HEIGHT);

    ui_dialog_draw_title_left(g_title_tex, g_title_w, g_title_h, dx, dy, TITLEBAR_HEIGHT, DIALOG_PADDING);
    ui_dialog_draw_close_button(dx + DIALOG_WIDTH - DIALOG_PADDING - CLOSE_BTN_SIZE,
                                dy + (TITLEBAR_HEIGHT - CLOSE_BTN_SIZE) / 2, CLOSE_BTN_SIZE);

    window_fill_rect(dx + DIALOG_PADDING, dy + TITLEBAR_HEIGHT, DIALOG_WIDTH - DIALOG_PADDING * 2, 1, UI_RGB(UI_COLOR_SURFACE_BORDER));

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

    int toolbar_y = crumb_y + CRUMB_BAR_HEIGHT;
    int search_x = dx + DIALOG_PADDING;
    int search_w = 220;
    icon_atlas_draw(ICON_search, search_x, toolbar_y + (TOOLBAR_HEIGHT - 6 - 16) / 2, 16);
    text_field_draw(&g_search_field, search_x + 20, toolbar_y, search_w - 20, TOOLBAR_HEIGHT - 6, 13);

    if (g_filter_label_tex != NULL) {
        int fx = dx + DIALOG_WIDTH - DIALOG_PADDING - g_filter_label_w;
        window_draw_texture_tinted(g_filter_label_tex, fx, crumb_y + (CRUMB_BAR_HEIGHT - g_filter_label_h) / 2,
                                    g_filter_label_w, g_filter_label_h, UI_RGB(UI_COLOR_TEXT_FAINT));
    }
    {
        static const char *labels[5] = { "Devices", "New folder", "Manage", "Rename", "Delete" };
        fm_rect_t tr[5];
        tool_rects(dx, toolbar_y, tr);
        int count = g_manage ? 5 : 3;
        int sel_ok = g_selected_index >= 0 && g_selected_index < g_entry_count;
        for (int i = 0; i < count; i++) {
            int enabled = 1;
            if (i == 0 && g_root_mode != FILE_MANAGER_ROOT_DEVICE) enabled = 0;
            if ((i == 1 || i == 2) && g_in_devices) enabled = 0;
            if (i >= 3 && !sel_ok) enabled = 0;
            if (i == 0 && g_root_mode != FILE_MANAGER_ROOT_DEVICE) continue;
            ui_kit_button(labels[i], tr[i].x, tr[i].y, tr[i].w, tr[i].h, enabled, i == 2 && g_manage);
        }
    }

    int grid_x = dx + DIALOG_PADDING;
    int grid_y = toolbar_y + TOOLBAR_HEIGHT + GRID_TOP_GAP;
    int grid_w = DIALOG_WIDTH - DIALOG_PADDING * 2;
    int filename_row_reserve = (g_pick_mode == FILE_MANAGER_MODE_SAVE_FILE) ? FILENAME_ROW_HEIGHT : 0;
    int grid_h = DIALOG_HEIGHT - (grid_y - dy) - FOOTER_HEIGHT - DIALOG_PADDING - filename_row_reserve;

    int columns = (grid_w - UI_SCROLLBAR_WIDTH - 4) / CELL_W;
    if (columns < 1) columns = 1;

    window_set_clip_rect(grid_x, grid_y, grid_w, grid_h);

    int mx = window_mouse_x(), my = window_mouse_y();
    int touch = window_pointer_is_touch();
    unsigned int started = window_ticks();
    int visible_index = 0;
    int shown_count = 0;
    for (int i = 0; i < g_entry_count; i++) {
        ab_entry_t *e = &g_entries[i];
        if (!ui_text_contains_ci(e->display_name, g_search_field.text)) continue;

        int col = visible_index % columns;
        int row = visible_index / columns;
        int cell_x = grid_x + col * CELL_W;
        int cell_y = grid_y + row * CELL_H - g_scroll_offset;
        visible_index++;

        if (cell_y + CELL_H < grid_y || cell_y > grid_y + grid_h) continue;

        prepare_entry_visuals(e, started);

        int is_dim = (!e->is_dir && g_pick_mode == FILE_MANAGER_MODE_PICK_FOLDER);
        int is_hover = !touch && (mx >= cell_x && mx < cell_x + CELL_W && my >= cell_y && my < cell_y + CELL_H
                         && my >= grid_y && my < grid_y + grid_h);
        int is_selected = ((!e->is_dir || g_manage) && i == g_selected_index);

        if (is_selected && g_selected_tex != NULL) {
            window_draw_texture(g_selected_tex, cell_x + 2, cell_y + 2, CELL_W - 4, CELL_H - 4);
        } else if (is_hover && !is_dim && g_hover_tex != NULL) {
            window_draw_texture(g_hover_tex, cell_x + 2, cell_y + 2, CELL_W - 4, CELL_H - 4);
        }

        int icon_area_x = cell_x + (CELL_W - GRID_ICON_SIZE) / 2;
        int icon_area_y = cell_y + 8;
        unsigned char tint = is_dim ? 100 : 255;

        if (e->is_dir) {
            icon_atlas_draw_tinted(ICON_folder, icon_area_x, icon_area_y, GRID_ICON_SIZE, UI_RGB(UI_COLOR_WARNING));
        } else if (e->thumb_tex != NULL) {
            int tw = e->thumb_w, th = e->thumb_h;
            int tx = icon_area_x + (GRID_ICON_SIZE - tw) / 2;
            int ty = icon_area_y + (GRID_ICON_SIZE - th) / 2;
            if (is_dim) {
                window_draw_texture_tinted(e->thumb_tex, tx, ty, tw, th, tint, tint, tint);
            } else {
                window_draw_texture(e->thumb_tex, tx, ty, tw, th);
            }
        } else {
            int icon_id = file_icons_for_name(e->display_name);
            if (is_dim) {
                icon_atlas_draw_tinted(icon_id, icon_area_x, icon_area_y, GRID_ICON_SIZE, tint, tint, tint);
            } else {
                icon_atlas_draw(icon_id, icon_area_x, icon_area_y, GRID_ICON_SIZE);
            }
        }

        if (i == g_rename_index) {
            text_field_draw(&g_rename_field, cell_x + 2, icon_area_y + GRID_ICON_SIZE + 4, CELL_W - 4, 24, 13);
        } else if (e->label_tex != NULL) {
            int label_x = cell_x + (CELL_W - e->label_w) / 2;
            int label_y = icon_area_y + GRID_ICON_SIZE + 6;
            int clip_top = label_y > grid_y ? label_y : grid_y;
            int clip_bottom = label_y + LABEL_FONT_SIZE + 4;
            if (clip_bottom > grid_y + grid_h) clip_bottom = grid_y + grid_h;
            if (clip_bottom > clip_top) {
                window_set_clip_rect(cell_x + 2, clip_top, CELL_W - 4, clip_bottom - clip_top);
                if (is_dim) {
                    window_draw_texture_tinted(e->label_tex, label_x, label_y, e->label_w, e->label_h, tint, tint, tint);
                } else {
                    window_draw_texture(e->label_tex, label_x, label_y, e->label_w, e->label_h);
                }
            }
            window_set_clip_rect(grid_x, grid_y, grid_w, grid_h);
        }
    }

    window_clear_clip_rect();
    for (int i = 0; i < g_entry_count; i++) {
        if (ui_text_contains_ci(g_entries[i].display_name, g_search_field.text)) shown_count++;
    }
    ui_scrollbar_draw(g_scroll_offset, grid_x, grid_w, grid_y, grid_h,
                      ((shown_count + columns - 1) / columns) * CELL_H);
    if (g_inline_error[0] != '\0') {
        ui_kit_text(g_inline_error, 12, grid_x, grid_y + grid_h + 2, 225, 100, 100);
    }

    int can_select = 0;
    if (g_in_devices) {
        can_select = 0;
    } else if (g_pick_mode == FILE_MANAGER_MODE_SAVE_FILE) {
        can_select = (g_filename_field.text[0] != '\0');
    } else if (g_pick_mode == FILE_MANAGER_MODE_PICK_FOLDER) {
        can_select = 1;
    } else if (g_selected_index >= 0 && g_selected_index < g_entry_count
               && !g_entries[g_selected_index].is_dir) {
        can_select = 1;
    }

    int buttons_y = dy + DIALOG_HEIGHT - DIALOG_PADDING - g_cancel_btn.height;

    if (g_pick_mode == FILE_MANAGER_MODE_SAVE_FILE) {
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
        window_draw_texture_tinted(g_select_btn_tex, select_x, buttons_y, g_select_btn.width, g_select_btn.height,
                                    UI_RGB(UI_COLOR_TEXT_DISABLED));
        window_draw_texture_tinted(g_select_txt_tex, select_x + g_select_btn.text_offset_x, buttons_y + g_select_btn.text_offset_y,
                                    g_select_btn.text_img.width, g_select_btn.text_img.height, UI_RGB(UI_COLOR_TEXT_FAINT));
    }

    if (g_prompt != PROMPT_NONE) {
        fm_rect_t pr = prompt_rect(dx, dy);
        ui_kit_rect(pr.x - 1, pr.y - 1, pr.w + 2, pr.h + 2, 11, 90, 93, 100);
        ui_kit_rect(pr.x, pr.y, pr.w, pr.h, 10, 38, 39, 44);
        const char *title = g_prompt == PROMPT_NEW_FOLDER ? "New folder name" : (g_prompt == PROMPT_RENAME ? "New name" : "Delete this item?");
        ui_kit_text(title, 14, pr.x + 16, pr.y + 12, 235, 235, 235);
        if (g_prompt == PROMPT_DELETE) {
            const char *nm = (g_selected_index >= 0 && g_selected_index < g_entry_count) ? g_entries[g_selected_index].display_name : "";
            ui_kit_text(nm, 13, pr.x + 16, pr.y + 44, 200, 204, 210);
        } else {
            text_field_draw(&g_prompt_field, pr.x + 16, pr.y + 40, pr.w - 32, 30, 14);
        }
        if (g_prompt_error[0]) ui_kit_text(g_prompt_error, 12, pr.x + 16, pr.y + 78, 225, 100, 100);
        int ok_x = pr.x + pr.w - 16 - 100, btn_y = pr.y + pr.h - 16 - 34;
        ui_kit_button("Cancel", ok_x - 10 - 100, btn_y, 100, 34, 1, 0);
        ui_kit_button(g_prompt == PROMPT_DELETE ? "Delete" : "OK", ok_x, btn_y, 100, 34, 1, 1);
    }
}

void file_manager_shutdown(void) {
    free_entries();
    free(g_entries);
    g_entries = NULL;
    g_entry_capacity = 0;
    free_crumb_textures();

    if (g_rename_ready) text_field_free(&g_rename_field);
    g_rename_ready = 0;
    if (g_title_tex != NULL) window_destroy_texture(g_title_tex);
    if (g_filter_label_tex != NULL) window_destroy_texture(g_filter_label_tex);
    if (g_crumb_sep_tex != NULL) window_destroy_texture(g_crumb_sep_tex);
    if (g_hover_tex != NULL) window_destroy_texture(g_hover_tex);
    if (g_selected_tex != NULL) window_destroy_texture(g_selected_tex);
    if (g_cancel_btn_tex != NULL) window_destroy_texture(g_cancel_btn_tex);
    if (g_cancel_txt_tex != NULL) window_destroy_texture(g_cancel_txt_tex);
    if (g_select_btn_tex != NULL) window_destroy_texture(g_select_btn_tex);
    if (g_select_txt_tex != NULL) window_destroy_texture(g_select_txt_tex);
    if (g_filename_label_tex != NULL) window_destroy_texture(g_filename_label_tex);

    text_field_free(&g_search_field);
    text_field_free(&g_filename_field);
    if (g_prompt_ready) text_field_free(&g_prompt_field);
    g_prompt_ready = 0;
    labeled_button_free(&g_cancel_btn);
    labeled_button_free(&g_select_btn);

    g_title_tex = NULL;
    g_filter_label_tex = NULL;
    g_crumb_sep_tex = NULL;
    g_hover_tex = NULL;
    g_selected_tex = NULL;

    g_is_open = 0;
    g_ready = 0;
}
