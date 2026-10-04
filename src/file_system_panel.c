/* File system: path data construction, while display and interaction are in ui_tree. */
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

#include "file_system_panel.h"
#include "window.h"
#include "text_field.h"
#include "current_project.h"
#include "ui_theme.h"
#include "ui_tree.h"
#include "file_icons.h"
#include "path_utils.h"
#include "icon_atlas.h"
#include "nodes_editor/image_loader.h"
#include "scene_tabs.h"
#include "current_scene.h"
#include "scene_data.h"
#include "code_editor.h"
#include "editor_workspace.h"
#include "viewport_2d.h"

#define MAX_ROWS 256
#define FILE_TREE_THUMB_SIZE 16
#define FILE_TREE_THUMB_MAX_BYTES (8 * 1024 * 1024)
#define MAX_EXPANDED 64
#define TOOLBAR_HEIGHT 26
#define SEARCH_HEIGHT 20

typedef struct {
    char path[600];
    char display_name[128];
    int depth;
    int is_dir;
    int has_children;
    window_texture_t *thumbnail_tex; /* owned by this row; NULL means atlas icon */
} fs_row_t;

static fs_row_t g_rows[MAX_ROWS];
static int g_row_count = 0;
static char g_expanded[MAX_EXPANDED][600];
static int g_expanded_count = 0;
static text_field_t g_search_field;
static int g_ready = 0;
static int g_needs_rebuild = 1;
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

static void open_file_resource(const fs_row_t *row) {
    if (row == NULL || row->is_dir) return;
    if (has_rscene_extension(row->display_name)) {
        open_scene_resource(row->path);
    } else if (script_extension_supported(row->display_name)) {
        editor_workspace_activate_script();
        code_editor_open_path(row->path);
    }
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

static void add_directory_contents(const char *dir_path, int depth) {
    DIR *dir = opendir(dir_path);
    if (dir == NULL) return;
    char *names[256];
    int count = 0;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL && count < 256) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
        names[count] = strdup(entry->d_name);
        if (names[count] != NULL) count++;
    }
    closedir(dir);
    qsort(names, (size_t)count, sizeof(char *), path_utils_compare_names);
    for (int i = 0; i < count && g_row_count < MAX_ROWS; i++) {
        char full_path[600];
        snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, names[i]);
        struct stat status;
        if (stat(full_path, &status) != 0) { free(names[i]); continue; }
        fs_row_t *row = &g_rows[g_row_count++];
        memset(row, 0, sizeof(*row));
        strncpy(row->path, full_path, sizeof(row->path) - 1);
        strncpy(row->display_name, names[i], sizeof(row->display_name) - 1);
        row->depth = depth;
        row->is_dir = S_ISDIR(status.st_mode);
        row->has_children = row->is_dir ? dir_has_children(full_path) : 0;
        if (!row->is_dir) {
            const char *dot = strrchr(row->display_name, '.');
            if (dot != NULL && file_icons_is_raster_image_ext(dot + 1)) {
                row->thumbnail_tex = load_tree_thumbnail(row->path);
            }
        }
        free(names[i]);
        if (row->is_dir && is_expanded(full_path)) add_directory_contents(full_path, depth + 1);
    }
}

static void free_rows(void) {
    for (int i = 0; i < g_row_count; i++) {
        if (g_rows[i].thumbnail_tex != NULL) {
            window_destroy_texture(g_rows[i].thumbnail_tex);
            g_rows[i].thumbnail_tex = NULL;
        }
    }
    g_row_count = 0;
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
        g_tree_items[i].icon_id = g_rows[i].is_dir ? ICON_Folder : file_icons_for_name(g_rows[i].display_name);
        if (!g_rows[i].is_dir && file_icons_is_raster_image_ext(strrchr(g_rows[i].display_name, '.') != NULL
                                                                ? strrchr(g_rows[i].display_name, '.') + 1 : "")
            && g_rows[i].thumbnail_tex == NULL) {
            g_tree_items[i].icon_id = ICON_Broken_Image;
        }
        g_tree_items[i].thumbnail_tex = g_rows[i].thumbnail_tex;
        g_tree_items[i].depth = g_rows[i].depth;
        g_tree_items[i].has_children = g_rows[i].is_dir;
        g_tree_items[i].expanded = g_rows[i].is_dir && is_expanded(g_rows[i].path);
        if (g_selected_path[0] != '\0' && strcmp(g_selected_path, g_rows[i].path) == 0) selected = i;
    }
    ui_tree_set_items(&g_tree, g_tree_items, g_row_count);
    ui_tree_set_selected(&g_tree, selected);
}

static void rebuild_rows(void) {
    free_rows();
    const char *project_path = current_project_get_path();
    if (project_path == NULL) { g_needs_rebuild = 0; sync_tree_items(); return; }
    char assets_path[600];
    snprintf(assets_path, sizeof(assets_path), "%s/Assets", project_path);
    struct stat status;
    if (stat(assets_path, &status) != 0) REBAX_MKDIR(assets_path, 0755);
    fs_row_t *root = &g_rows[g_row_count++];
    memset(root, 0, sizeof(*root));
    strncpy(root->path, assets_path, sizeof(root->path) - 1);
    strncpy(root->display_name, "Assets://", sizeof(root->display_name) - 1);
    root->is_dir = 1;
    root->has_children = dir_has_children(assets_path);
    if (is_expanded(assets_path)) add_directory_contents(assets_path, 1);
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
    }
    if (g_needs_rebuild) rebuild_rows(); else sync_tree_items();

    int toolbar_icon_y = y + (TOOLBAR_HEIGHT - UI_TREE_ICON_SIZE) / 2;
    int more_x = x + w - UI_TREE_ICON_SIZE - UI_TREE_ROW_PAD;
    int search_icon_x = x + UI_TREE_ROW_PAD;
    int search_field_x = search_icon_x + UI_TREE_ICON_SIZE + UI_TREE_ROW_PAD;
    int search_field_w = more_x - UI_TREE_ROW_PAD - search_field_x;
    int search_field_y = y + (TOOLBAR_HEIGHT - SEARCH_HEIGHT) / 2;
    text_field_update(&g_search_field, search_field_x, search_field_y, search_field_w, SEARCH_HEIGHT, search_field_w);
    (void)toolbar_icon_y; /* The tool buttons are still only a placeholder as before. */

    int list_y = y + TOOLBAR_HEIGHT + 4;
    ui_tree_update(&g_tree, x, list_y, w, h - TOOLBAR_HEIGHT - 4);
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
        } else {
            g_last_click_remaining = 0.0f;
            g_last_click_path[0] = '\0';
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
    ui_tree_shutdown(&g_tree);
    text_field_free(&g_search_field);
    g_ready = 0;
    g_needs_rebuild = 1;
    g_last_project_path[0] = '\0';
    g_selected_path[0] = '\0';
    g_last_click_path[0] = '\0';
    g_last_click_remaining = 0.0f;
    g_expanded_count = 0;
}
