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
#include <ctype.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#if defined(_WIN32)
#include <direct.h>
#define REBAX_MKDIR(path, mode) _mkdir(path)
#else
#include <sys/stat.h>
#define REBAX_MKDIR(path, mode) mkdir(path, mode)
#endif

#include "editor/panels/file_system.h"
#include "platform/common/window.h"
#include "editor/ui/text_field.h"
#include "project/current_project.h"
#include "editor/ui/theme.h"
#include "editor/ui/tree.h"
#include "editor/ui/file_icons.h"
#include "filesystem/path_utils.h"
#include "editor/ui/icon_atlas.h"
#include "formats/image_decoder.h"
#include "editor/panels/scene_tabs.h"
#include "scene/current_scene.h"
#include "scene/scene_data.h"
#include "editor/panels/code_editor.h"
#include "editor/core/workspace.h"
#include "editor/viewport/2d.h"
#include "editor/dialogs/context_menu.h"
#include "editor/dialogs/prompt.h"
#include "editor/dialogs/image_viewer.h"
#include "filesystem/rebax_fs.h"
#include "filesystem/rebax_fs_tree.h"

#define MAX_ROWS 6000
#define THUMB_CACHE 384
#define THUMB_BUDGET_TICKS 6
#define FILE_TREE_THUMB_SIZE 16
#define FILE_TREE_THUMB_MAX_BYTES (8 * 1024 * 1024)
#define MAX_EXPANDED 64
#define TOOLBAR_HEIGHT 30
#define SEARCH_HEIGHT 24

typedef struct {
    char path[600];
    char display_name[128];
    int depth;
    int is_dir;
    int has_children;
    long long modified;
    window_texture_t *thumbnail_tex;
} fs_row_t;

typedef struct {
    char path[600];
    long long modified;
    window_texture_t *tex;
    int used;
} thumb_entry_t;

static thumb_entry_t g_thumbs[THUMB_CACHE];
static int g_thumb_next = 0;

static fs_row_t g_rows[MAX_ROWS];
static int g_row_count = 0;
static char g_expanded[MAX_EXPANDED][600];
static int g_expanded_count = 0;
static text_field_t g_search_field;
static int g_ready = 0;
static int g_needs_rebuild = 1;
static float g_refresh_timer = 0.0f;
static unsigned long long g_assets_signature = 0;
#define ASSETS_REFRESH_INTERVAL 2.0f
static char g_last_project_path[600] = {0};
static char g_selected_path[600] = {0};
static ui_tree_t g_tree;
static ui_tree_item_t g_tree_items[MAX_ROWS];
static char g_last_click_path[600];
static float g_last_click_remaining;

static int has_rscene_extension(const char *name) {
    const char *dot = strrchr(name ? name : "", '.');
    static const char ext[] = ".rscene";
    if (dot == NULL || strlen(dot) != sizeof(ext) - 1) return 0;
    for (size_t i = 0; i < sizeof(ext) - 1; i++)
        if (tolower((unsigned char)dot[i]) != ext[i]) return 0;
    return 1;
}

static int script_extension_supported(const char *name) {
    static const char *extensions[] = {
        "c", "h", "cc", "hh", "cpp", "hpp", "cxx", "hxx", "c++", "h++", "ipp", "inl"
    };
    const char *dot = strrchr(name ? name : "", '.');
    if (dot == NULL || dot[1] == '\0') return 0;
    for (size_t i = 0; i < sizeof(extensions) / sizeof(extensions[0]); i++) {
        size_t n = strlen(extensions[i]);
        if (strlen(dot + 1) != n) continue;
        size_t j = 0;
        while (j < n && tolower((unsigned char)dot[j + 1]) == extensions[i][j]) j++;
        if (j == n) return 1;
    }
    return 0;
}

static int image_extension_supported(const char *name) {
    static const char *extensions[] = { "png", "jpg", "jpeg", "bmp", "tga", "gif" };
    const char *dot = strrchr(name ? name : "", '.');
    if (dot == NULL || dot[1] == '\0') return 0;
    for (size_t i = 0; i < sizeof(extensions) / sizeof(extensions[0]); i++) {
        size_t n = strlen(extensions[i]), j = 0;
        if (strlen(dot + 1) != n) continue;
        while (j < n && tolower((unsigned char)dot[j + 1]) == extensions[i][j]) j++;
        if (j == n) return 1;
    }
    return 0;
}

static int scene_type_dimension(const char *type) {
    if (strstr(type, "2D") != NULL || strstr(type, "2d") != NULL) return 1;
    if (strstr(type, "3D") != NULL || strstr(type, "3d") != NULL) return 2;
    return 0;
}

static int scene_default_dimension(const char *path) {
    FILE *file = fopen(path, "rb");
    if (file == NULL) return EDITOR_CAMERA_MODE_2D;
    char line[1024];
    int roots_2d = 0, roots_3d = 0, all_2d = 0, all_3d = 0;
    int dimension = 0, is_root = 0, saw_node = 0;
    while (fgets(line, sizeof(line), file) != NULL) {
        if (line[0] == '[' && line[1] == 'n') {
            if (saw_node && dimension == 1) { all_2d++; if (is_root) roots_2d++; }
            if (saw_node && dimension == 2) { all_3d++; if (is_root) roots_3d++; }
            saw_node = 1; dimension = 0; is_root = 0;
        } else if (strncmp(line, "type=", 5) == 0) {
            char *end = strpbrk(line + 5, "\r\n");
            if (end) *end = '\0';
            dimension = scene_type_dimension(line + 5);
        } else if (strncmp(line, "parent=", 7) == 0) {
            is_root = atoi(line + 7) < 0;
        }
    }
    if (saw_node && dimension == 1) { all_2d++; if (is_root) roots_2d++; }
    if (saw_node && dimension == 2) { all_3d++; if (is_root) roots_3d++; }
    fclose(file);
    int count_2d = roots_2d + roots_3d > 0 ? roots_2d : all_2d;
    int count_3d = roots_2d + roots_3d > 0 ? roots_3d : all_3d;
    if (count_3d > 0 && count_3d >= count_2d) return EDITOR_CAMERA_MODE_3D;
    if (count_2d > 0) return EDITOR_CAMERA_MODE_2D;
    return EDITOR_CAMERA_MODE_2D;
}

static void open_scene_resource(const char *path) {
    int dimension = scene_default_dimension(path);
    const char *active_path = current_scene_get_path();
    if (active_path == NULL || strcmp(active_path, path) != 0) {
        if (scene_data_get_node_count() > 0 || current_scene_is_dirty()) {
            if (!scene_tabs_new()) return;
        }
        if (!current_scene_load(path)) return;
        viewport_2d_focus_scene();
    }
    editor_workspace_set_camera_mode((editor_camera_mode_t)dimension);
}

void file_system_panel_open_scene(const char *path) {
    open_scene_resource(path);
}

static void open_file_resource(const fs_row_t *row) {
    if (row == NULL || row->is_dir) return;
    if (has_rscene_extension(row->display_name)) {
        open_scene_resource(row->path);
    } else if (script_extension_supported(row->display_name)) {
        editor_workspace_activate_script();
        code_editor_open_path(row->path);
    } else if (image_extension_supported(row->display_name)) {
        image_viewer_open(row->path);
    }
}

enum {
    MENU_OPEN = 1, MENU_PREVIEW, MENU_TOGGLE, MENU_NEW_FOLDER, MENU_NEW_SCRIPT,
    MENU_RENAME, MENU_DUPLICATE, MENU_DELETE, MENU_COPY_PATH
};

static char g_target_path[600];
static int g_target_is_dir;
static int is_expanded(const char *path);
static void toggle_expanded(const char *path);

static int is_separator(char c) { return c == '/' || c == '\\'; }

static void split_path(const char *path, char *dir, size_t dir_size, const char **name) {
    const char *slash = NULL;
    for (const char *p = path; *p; p++) if (is_separator(*p)) slash = p;
    if (slash == NULL) { dir[0] = '\0'; *name = path; return; }
    size_t n = (size_t)(slash - path);
    if (n >= dir_size) n = dir_size - 1;
    memcpy(dir, path, n);
    dir[n] = '\0';
    *name = slash + 1;
}

static int valid_name(const char *name) {
    if (name == NULL || name[0] == '\0' || strcmp(name, ".") == 0 || strcmp(name, "..") == 0) return 0;
    for (const char *p = name; *p; p++) if (is_separator(*p) || *p == ':' || *p == '*' || *p == '?' || *p == '"' || *p == '<' || *p == '>' || *p == '|') return 0;
    return 1;
}

static void unique_copy_path(const char *path, char *out, size_t out_size) {
    char dir[600];
    const char *name;
    split_path(path, dir, sizeof(dir), &name);
    const char *dot = strrchr(name, '.');
    size_t stem = dot && dot != name ? (size_t)(dot - name) : strlen(name);
    for (int i = 1; i < 1000; i++) {
        if (i == 1) snprintf(out, out_size, "%s/%.*s_copy%s", dir, (int)stem, name, name + stem);
        else snprintf(out, out_size, "%s/%.*s_copy%d%s", dir, (int)stem, name, i, name + stem);
        if (!rebax_fs_exists(out)) return;
    }
}

static void on_rename(const char *text, void *unused) {
    (void)unused;
    char dir[600], target[640];
    const char *name;
    if (!valid_name(text)) return;
    split_path(g_target_path, dir, sizeof(dir), &name);
    snprintf(target, sizeof(target), "%s/%s", dir, text);
    if (rebax_fs_exists(target)) return;
    rebax_fs_move(g_target_path, target);
    g_needs_rebuild = 1;
}

static void on_new_folder(const char *text, void *unused) {
    (void)unused;
    char target[640];
    if (!valid_name(text)) return;
    snprintf(target, sizeof(target), "%s/%s", g_target_path, text);
    if (rebax_fs_exists(target)) return;
    rebax_fs_mkdir_p(target);
    g_needs_rebuild = 1;
}

static void on_new_script(const char *text, void *unused) {
    (void)unused;
    char target[640];
    if (!valid_name(text)) return;
    snprintf(target, sizeof(target), "%s/%s", g_target_path, text);
    if (rebax_fs_exists(target)) return;
    const char *header = script_extension_supported(text) ? "#include <rebax.h>\n\n" : "";
    if (!rebax_fs_write_atomic(target, header, strlen(header))) return;
    g_needs_rebuild = 1;
    if (script_extension_supported(text)) {
        editor_workspace_activate_script();
        code_editor_open_path(target);
    }
}

static void on_delete(const char *text, void *unused) {
    (void)text; (void)unused;
    rebax_fs_remove_recursive(g_target_path);
    g_needs_rebuild = 1;
}

static void on_menu_choice(int id, void *user_data) {
    const fs_row_t *row = (const fs_row_t *)user_data;
    char dir[600], target[640];
    const char *name;
    (void)row;
    split_path(g_target_path, dir, sizeof(dir), &name);
    switch (id) {
        case MENU_OPEN:
        case MENU_PREVIEW: {
            fs_row_t temp;
            memset(&temp, 0, sizeof(temp));
            snprintf(temp.path, sizeof(temp.path), "%s", g_target_path);
            snprintf(temp.display_name, sizeof(temp.display_name), "%s", name);
            open_file_resource(&temp);
            break;
        }
        case MENU_TOGGLE: toggle_expanded(g_target_path); g_needs_rebuild = 1; break;
        case MENU_NEW_FOLDER: prompt_input_open("New Folder", "NewFolder", "Create", on_new_folder, NULL); break;
        case MENU_NEW_SCRIPT: prompt_input_open("New Script", "script.c", "Create", on_new_script, NULL); break;
        case MENU_RENAME: prompt_input_open("Rename", name, "Rename", on_rename, NULL); break;
        case MENU_DUPLICATE:
            unique_copy_path(g_target_path, target, sizeof(target));
            if (g_target_is_dir) rebax_fs_copy_tree(g_target_path, target);
            else rebax_fs_copy_file(g_target_path, target);
            g_needs_rebuild = 1;
            break;
        case MENU_DELETE: prompt_confirm_open("Delete", "This item will be deleted permanently.", "Delete", 1, on_delete, NULL); break;
        case MENU_COPY_PATH: window_set_clipboard_text(g_target_path); break;
        default: break;
    }
}

static int path_is_inside(const char *path, const char *section) {
    const char *project = current_project_get_path();
    char root[700];
    if (project == NULL) return 0;
    snprintf(root, sizeof(root), "%s/%s", project, section);
    size_t n = strlen(root);
    return strncmp(path, root, n) == 0 && (path[n] == '\0' || is_separator(path[n]));
}

static void open_row_menu(const fs_row_t *row, int x, int y) {
    context_menu_item_t items[CONTEXT_MENU_MAX_ITEMS];
    int n = 0;
    int is_root = row->is_dir && row->depth == 0;
    snprintf(g_target_path, sizeof(g_target_path), "%s", row->path);
    g_target_is_dir = row->is_dir;
    if (row->is_dir) {
        items[n++] = (context_menu_item_t){ is_expanded(row->path) ? "Collapse" : "Expand", MENU_TOGGLE, 1, 0 };
        items[n++] = (context_menu_item_t){ "New Folder", MENU_NEW_FOLDER, 1, 1 };
        items[n++] = (context_menu_item_t){ "New Script", MENU_NEW_SCRIPT, path_is_inside(row->path, "source"), 0 };
    } else if (has_rscene_extension(row->display_name)) {
        items[n++] = (context_menu_item_t){ "Open Scene", MENU_OPEN, 1, 0 };
    } else if (script_extension_supported(row->display_name)) {
        items[n++] = (context_menu_item_t){ "Open in Editor", MENU_OPEN, 1, 0 };
    } else if (image_extension_supported(row->display_name)) {
        items[n++] = (context_menu_item_t){ "Preview", MENU_PREVIEW, 1, 0 };
    }
    int has_primary = n > 0;
    items[n++] = (context_menu_item_t){ "Rename", MENU_RENAME, !is_root, has_primary };
    items[n++] = (context_menu_item_t){ "Duplicate", MENU_DUPLICATE, !is_root, 0 };
    items[n++] = (context_menu_item_t){ "Delete", MENU_DELETE, !is_root, 0 };
    items[n++] = (context_menu_item_t){ "Copy Path", MENU_COPY_PATH, 1, 1 };
    context_menu_open(x, y, items, n, on_menu_choice, NULL);
}

static int is_expanded(const char *path) {
    for (int i = 0; i < g_expanded_count; i++) if (strcmp(g_expanded[i], path) == 0) return 1;
    return 0;
}

static void toggle_expanded(const char *path) {
    for (int i = 0; i < g_expanded_count; i++) {
        if (strcmp(g_expanded[i], path) == 0) {
            for (int j = i; j < g_expanded_count - 1; j++) strcpy(g_expanded[j], g_expanded[j + 1]);
            g_expanded_count--;
            return;
        }
    }
    if (g_expanded_count < MAX_EXPANDED) {
        strncpy(g_expanded[g_expanded_count], path, sizeof(g_expanded[0]) - 1);
        g_expanded[g_expanded_count][sizeof(g_expanded[0]) - 1] = '\0';
        g_expanded_count++;
    }
}

static window_texture_t *load_tree_thumbnail(const char *path);

static int dir_has_children(const char *path) {
    DIR *dir = opendir(path);
    if (dir == NULL) return 0;
    int found = 0;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) { found = 1; break; }
    }
    closedir(dir);
    return found;
}

typedef struct {
    char *name;
    int is_dir;
    long long modified;
} dir_item_t;

static int compare_dir_items(const void *a, const void *b) {
    const dir_item_t *x = (const dir_item_t *)a;
    const dir_item_t *y = (const dir_item_t *)b;
    if (x->is_dir != y->is_dir) return x->is_dir ? -1 : 1;
    return path_utils_compare_names_ci(&x->name, &y->name);
}

static void add_directory_contents(const char *dir_path, int depth) {
    DIR *dir = opendir(dir_path);
    if (dir == NULL) return;
    dir_item_t *items = malloc(sizeof(dir_item_t) * MAX_ROWS);
    int count = 0;
    struct dirent *entry;
    if (items == NULL) {
        closedir(dir);
        return;
    }
    while ((entry = readdir(dir)) != NULL && count < MAX_ROWS) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
        char full_path[600];
        snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, entry->d_name);
        struct stat status;
        if (stat(full_path, &status) != 0) continue;
        items[count].name = strdup(entry->d_name);
        if (items[count].name == NULL) continue;
        items[count].is_dir = S_ISDIR(status.st_mode) ? 1 : 0;
        items[count].modified = (long long)status.st_mtime;
        count++;
    }
    closedir(dir);
    qsort(items, (size_t)count, sizeof(dir_item_t), compare_dir_items);
    for (int i = 0; i < count; i++) {
        if (g_row_count < MAX_ROWS) {
            char full_path[600];
            snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, items[i].name);
            fs_row_t *row = &g_rows[g_row_count++];
            memset(row, 0, sizeof(*row));
            strncpy(row->path, full_path, sizeof(row->path) - 1);
            strncpy(row->display_name, items[i].name, sizeof(row->display_name) - 1);
            row->depth = depth;
            row->is_dir = items[i].is_dir;
            row->has_children = row->is_dir ? dir_has_children(full_path) : 0;
            row->modified = items[i].modified;
            if (row->is_dir && is_expanded(full_path)) add_directory_contents(full_path, depth + 1);
        }
        free(items[i].name);
    }
    free(items);
}

static void free_rows(void) {
    g_row_count = 0;
}

static void free_thumbnails(void) {
    for (int i = 0; i < THUMB_CACHE; i++) {
        if (g_thumbs[i].tex != NULL) window_destroy_texture(g_thumbs[i].tex);
        memset(&g_thumbs[i], 0, sizeof(g_thumbs[i]));
    }
    g_thumb_next = 0;
}

static thumb_entry_t *find_thumbnail(const fs_row_t *row) {
    for (int i = 0; i < THUMB_CACHE; i++) {
        if (g_thumbs[i].used && g_thumbs[i].modified == row->modified &&
            strcmp(g_thumbs[i].path, row->path) == 0) {
            return &g_thumbs[i];
        }
    }
    return NULL;
}

static void resolve_visible_thumbnails(int list_y, int list_h) {
    int row_h = g_tree.style.row_height;
    int first = g_tree.scroll_offset / row_h;
    int last = (g_tree.scroll_offset + list_h) / row_h + 1;
    unsigned int started = window_ticks();
    (void)list_y;
    if (first < 0) first = 0;
    if (last >= g_row_count) last = g_row_count - 1;
    for (int i = first; i <= last; i++) {
        fs_row_t *row = &g_rows[i];
        const char *dot = row->is_dir ? NULL : strrchr(row->display_name, '.');
        thumb_entry_t *entry;
        if (dot == NULL || !file_icons_is_raster_image_ext(dot + 1)) continue;
        entry = find_thumbnail(row);
        if (entry == NULL) {
            if (window_ticks() - started > THUMB_BUDGET_TICKS) continue;
            entry = &g_thumbs[g_thumb_next];
            g_thumb_next = (g_thumb_next + 1) % THUMB_CACHE;
            if (entry->tex != NULL) window_destroy_texture(entry->tex);
            snprintf(entry->path, sizeof(entry->path), "%s", row->path);
            entry->modified = row->modified;
            entry->tex = load_tree_thumbnail(row->path);
            entry->used = 1;
        }
        row->thumbnail_tex = entry->tex;
        g_tree_items[i].thumbnail_tex = entry->tex;
        if (entry->tex == NULL) g_tree_items[i].icon_id = ICON_broken_image;
    }
}

static window_texture_t *load_tree_thumbnail(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0 || st.st_size <= 0 || st.st_size > FILE_TREE_THUMB_MAX_BYTES) return NULL;
    int w = 0, h = 0;
    unsigned char *pixels = image_loader_editor_load(path, &w, &h, 0, 0, 0);
    if (pixels == NULL || w <= 0 || h <= 0) {
        free(pixels);
        return NULL;
    }
    unsigned char *scaled = malloc((size_t)FILE_TREE_THUMB_SIZE * FILE_TREE_THUMB_SIZE * 4);
    if (scaled == NULL) { free(pixels); return NULL; }
    for (int y = 0; y < FILE_TREE_THUMB_SIZE; y++) {
        int sy = (int)((long)y * h / FILE_TREE_THUMB_SIZE);
        for (int x = 0; x < FILE_TREE_THUMB_SIZE; x++) {
            int sx = (int)((long)x * w / FILE_TREE_THUMB_SIZE);
            memcpy(scaled + ((size_t)y * FILE_TREE_THUMB_SIZE + x) * 4,
                   pixels + ((size_t)sy * w + sx) * 4, 4);
        }
    }
    window_texture_t *texture = window_create_texture(scaled, FILE_TREE_THUMB_SIZE, FILE_TREE_THUMB_SIZE);
    free(scaled);
    free(pixels);
    return texture;
}

static void sync_tree_items(void) {
    int selected = -1;
    for (int i = 0; i < g_row_count; i++) {
        g_tree_items[i].name = g_rows[i].display_name;
        g_tree_items[i].icon_id = g_rows[i].is_dir ? ICON_folder : file_icons_for_name(g_rows[i].display_name);
        g_tree_items[i].thumbnail_tex = NULL;
        g_tree_items[i].depth = g_rows[i].depth;
        g_tree_items[i].has_children = g_rows[i].is_dir;
        g_tree_items[i].expanded = g_rows[i].is_dir && is_expanded(g_rows[i].path);
        if (g_selected_path[0] != '\0' && strcmp(g_selected_path, g_rows[i].path) == 0) selected = i;
    }
    ui_tree_set_items(&g_tree, g_tree_items, g_row_count);
    ui_tree_set_selected(&g_tree, selected);
}

static unsigned long long hash_bytes(unsigned long long hash, const void *data, size_t size) {
    const unsigned char *bytes = (const unsigned char *)data;
    for (size_t i = 0; i < size; i++) hash = (hash ^ bytes[i]) * 1099511628211ULL;
    return hash;
}

static unsigned long long directory_signature(const char *dir_path, int depth, unsigned long long hash) {
    DIR *dir = opendir(dir_path);
    if (dir == NULL) return hash;
    struct dirent *entry;
    int visited = 0;
    while ((entry = readdir(dir)) != NULL && visited < MAX_ROWS) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
        visited++;
        char full_path[600];
        snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, entry->d_name);
        struct stat status;
        if (stat(full_path, &status) != 0) continue;
        long long size = (long long)status.st_size;
        long long modified = (long long)status.st_mtime;
        hash = hash_bytes(hash, entry->d_name, strlen(entry->d_name));
        hash = hash_bytes(hash, &size, sizeof(size));
        hash = hash_bytes(hash, &modified, sizeof(modified));
        if (S_ISDIR(status.st_mode) && depth < 16 && is_expanded(full_path)) {
            hash = directory_signature(full_path, depth + 1, hash);
        }
    }
    closedir(dir);
    return hash;
}

static const char *const k_root_dirs[2] = { "assets", "source" };
static const char *const k_root_labels[2] = { "Assets://", "Source://" };

static unsigned long long assets_signature(void) {
    const char *project_path = current_project_get_path();
    if (project_path == NULL) return 0;
    unsigned long long hash = 14695981039346656037ULL;
    for (int r = 0; r < 2; r++) {
        char root_path[600];
        snprintf(root_path, sizeof(root_path), "%s/%s", project_path, k_root_dirs[r]);
        hash = directory_signature(root_path, 0, hash);
    }
    return hash;
}

static void rebuild_rows(void) {
    free_rows();
    const char *project_path = current_project_get_path();
    if (project_path == NULL) { g_needs_rebuild = 0; sync_tree_items(); return; }
    for (int r = 0; r < 2; r++) {
        char root_path[600];
        snprintf(root_path, sizeof(root_path), "%s/%s", project_path, k_root_dirs[r]);
        struct stat status;
        if (stat(root_path, &status) != 0) REBAX_MKDIR(root_path, 0755);
        if (g_row_count >= MAX_ROWS) break;
        fs_row_t *root = &g_rows[g_row_count++];
        memset(root, 0, sizeof(*root));
        strncpy(root->path, root_path, sizeof(root->path) - 1);
        strncpy(root->display_name, k_root_labels[r], sizeof(root->display_name) - 1);
        root->is_dir = 1;
        root->has_children = dir_has_children(root_path);
        if (is_expanded(root_path)) add_directory_contents(root_path, 1);
    }
    g_needs_rebuild = 0;
    sync_tree_items();
}

static void ensure_ready(void) {
    if (g_ready) return;
    g_ready = 1;
    text_field_init(&g_search_field);
        ui_tree_style_t style = ui_tree_default_style();
    style.highlight_indented = 1;
    style.font_pixel_size = 15;
    ui_tree_init(&g_tree, MAX_ROWS, &style);
    icon_atlas_init();
}

void file_system_panel_update(int x, int y, int w, int h) {
    ensure_ready();
    g_last_click_remaining -= window_get_delta_time();
    if (g_last_click_remaining < 0.0f) {
        g_last_click_remaining = 0.0f;
        g_last_click_path[0] = '\0';
    }
    const char *project_path = current_project_get_path();
    if (project_path != NULL && strcmp(project_path, g_last_project_path) != 0) {
        strncpy(g_last_project_path, project_path, sizeof(g_last_project_path) - 1);
        g_last_project_path[sizeof(g_last_project_path) - 1] = '\0';
        g_needs_rebuild = 1;
        free_thumbnails();
    }
    g_refresh_timer += window_get_delta_time();
    if (!g_needs_rebuild && g_refresh_timer >= ASSETS_REFRESH_INTERVAL) {
        g_refresh_timer = 0.0f;
        if (assets_signature() != g_assets_signature) g_needs_rebuild = 1;
    }
    if (g_needs_rebuild) {
        rebuild_rows();
        g_assets_signature = assets_signature();
        g_refresh_timer = 0.0f;
    } else sync_tree_items();

    int toolbar_icon_y = y + (TOOLBAR_HEIGHT - UI_TREE_ICON_SIZE) / 2;
    int more_x = x + w - UI_TREE_ICON_SIZE - UI_TREE_ROW_PAD;
    int search_icon_x = x + UI_TREE_ROW_PAD;
    int search_field_x = search_icon_x + UI_TREE_ICON_SIZE + UI_TREE_ROW_PAD;
    int search_field_w = more_x - UI_TREE_ROW_PAD - search_field_x;
    int search_field_y = y + (TOOLBAR_HEIGHT - SEARCH_HEIGHT) / 2;
    text_field_update(&g_search_field, search_field_x, search_field_y, search_field_w, SEARCH_HEIGHT, search_field_w);
    (void)toolbar_icon_y;

    int list_y = y + TOOLBAR_HEIGHT + 4;
    ui_tree_update(&g_tree, x, list_y, w, h - TOOLBAR_HEIGHT - 4);
    resolve_visible_thumbnails(list_y, h - TOOLBAR_HEIGHT - 4);
    {
        int menu_x = 0, menu_y = 0;
        int menu_row = ui_tree_menu_index(&g_tree, &menu_x, &menu_y);
        if (menu_row >= 0 && menu_row < g_row_count) {
            strncpy(g_selected_path, g_rows[menu_row].path, sizeof(g_selected_path) - 1);
            g_selected_path[sizeof(g_selected_path) - 1] = '\0';
            open_row_menu(&g_rows[menu_row], menu_x, menu_y);
        }
    }
    int toggled = ui_tree_toggled_index(&g_tree);
    if (toggled >= 0 && toggled < g_row_count && g_rows[toggled].is_dir) {
        toggle_expanded(g_rows[toggled].path);
        g_needs_rebuild = 1;
    }
    int clicked = ui_tree_clicked_index(&g_tree);
    if (clicked >= 0 && clicked < g_row_count) {
        strncpy(g_selected_path, g_rows[clicked].path, sizeof(g_selected_path) - 1);
        g_selected_path[sizeof(g_selected_path) - 1] = '\0';
        if (!g_rows[clicked].is_dir && g_last_click_remaining > 0.0f &&
            strcmp(g_last_click_path, g_rows[clicked].path) == 0) {
            open_file_resource(&g_rows[clicked]);
            g_last_click_remaining = 0.0f;
            g_last_click_path[0] = '\0';
        } else if (!g_rows[clicked].is_dir) {
            strncpy(g_last_click_path, g_rows[clicked].path, sizeof(g_last_click_path) - 1);
            g_last_click_path[sizeof(g_last_click_path) - 1] = '\0';
            g_last_click_remaining = 0.45f;
        } else if (g_last_click_remaining > 0.0f && strcmp(g_last_click_path, g_rows[clicked].path) == 0) {
            toggle_expanded(g_rows[clicked].path);
            g_needs_rebuild = 1;
            g_last_click_remaining = 0.0f;
            g_last_click_path[0] = '\0';
        } else {
            strncpy(g_last_click_path, g_rows[clicked].path, sizeof(g_last_click_path) - 1);
            g_last_click_path[sizeof(g_last_click_path) - 1] = '\0';
            g_last_click_remaining = 0.45f;
        }
    }
}

void file_system_panel_draw(int x, int y, int w, int h) {
    int toolbar_icon_y = y + (TOOLBAR_HEIGHT - UI_TREE_ICON_SIZE) / 2;
    int more_x = x + w - UI_TREE_ICON_SIZE - UI_TREE_ROW_PAD;
    int search_icon_x = x + UI_TREE_ROW_PAD;
    int search_field_x = search_icon_x + UI_TREE_ICON_SIZE + UI_TREE_ROW_PAD;
    int search_field_w = more_x - UI_TREE_ROW_PAD - search_field_x;
    int search_field_y = y + (TOOLBAR_HEIGHT - SEARCH_HEIGHT) / 2;
    icon_atlas_draw(ICON_search, search_icon_x, toolbar_icon_y, UI_TREE_ICON_SIZE);
    text_field_draw(&g_search_field, search_field_x, search_field_y, search_field_w, SEARCH_HEIGHT, 13);
    icon_atlas_draw(ICON_more, more_x, toolbar_icon_y, UI_TREE_ICON_SIZE);
    ui_tree_draw(&g_tree, x, y + TOOLBAR_HEIGHT + 4, w, h - TOOLBAR_HEIGHT - 4);
}

void file_system_panel_shutdown(void) {
    free_rows();
    free_thumbnails();
    ui_tree_shutdown(&g_tree);
    text_field_free(&g_search_field);
    g_ready = 0;
    g_needs_rebuild = 1;
    g_refresh_timer = 0.0f;
    g_assets_signature = 0;
    g_last_project_path[0] = '\0';
    g_selected_path[0] = '\0';
    g_last_click_path[0] = '\0';
    g_last_click_remaining = 0.0f;
    g_expanded_count = 0;
}
