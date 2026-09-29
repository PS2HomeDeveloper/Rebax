/* نظام الملفات: بناء بيانات المسارات، بينما العرض والتفاعل في ui_tree. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

#define MAX_ROWS 256
#define MAX_EXPANDED 64
#define TOOLBAR_HEIGHT 26
#define SEARCH_HEIGHT 20

typedef struct {
    char path[600];
    char display_name[128];
    int depth;
    int is_dir;
    int has_children;
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
        free(names[i]);
        if (row->is_dir && is_expanded(full_path)) add_directory_contents(full_path, depth + 1);
    }
}

static void free_rows(void) { g_row_count = 0; }

static void sync_tree_items(void) {
    int selected = -1;
    for (int i = 0; i < g_row_count; i++) {
        g_tree_items[i].name = g_rows[i].display_name;
        g_tree_items[i].icon_id = g_rows[i].is_dir ? ICON_Folder : file_icons_for_name(g_rows[i].display_name);
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
    style.highlight_indented = 0;
    style.font_pixel_size = 14;
    ui_tree_init(&g_tree, MAX_ROWS, &style);
    icon_atlas_init();
}

void file_system_panel_update(int x, int y, int w, int h) {
    ensure_ready();
    const char *project_path = current_project_get_path();
    if (project_path != NULL && strcmp(project_path, g_last_project_path) != 0) {
        strncpy(g_last_project_path, project_path, sizeof(g_last_project_path) - 1);
        g_last_project_path[sizeof(g_last_project_path) - 1] = '\0';
        g_needs_rebuild = 1;
    }
    if (g_needs_rebuild) rebuild_rows(); else sync_tree_items();

    int toolbar_icon_y = y + (TOOLBAR_HEIGHT - UI_TREE_ICON_SIZE) / 2;
    int more_x = x + w - UI_TREE_ICON_SIZE - UI_TREE_ROW_PAD;
    int import_x = more_x - UI_TREE_ICON_SIZE - UI_TREE_ROW_PAD;
    int search_icon_x = x + UI_TREE_ROW_PAD;
    int search_field_x = search_icon_x + UI_TREE_ICON_SIZE + UI_TREE_ROW_PAD;
    int search_field_w = import_x - UI_TREE_ROW_PAD - search_field_x;
    int search_field_y = y + (TOOLBAR_HEIGHT - SEARCH_HEIGHT) / 2;
    text_field_update(&g_search_field, search_field_x, search_field_y, search_field_w, SEARCH_HEIGHT, search_field_w);
    (void)toolbar_icon_y; /* أزرار الأدوات ما زالت شكلاً فقط كما كانت. */

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
    }
}

void file_system_panel_draw(int x, int y, int w, int h) {
    int toolbar_icon_y = y + (TOOLBAR_HEIGHT - UI_TREE_ICON_SIZE) / 2;
    int more_x = x + w - UI_TREE_ICON_SIZE - UI_TREE_ROW_PAD;
    int import_x = more_x - UI_TREE_ICON_SIZE - UI_TREE_ROW_PAD;
    int search_icon_x = x + UI_TREE_ROW_PAD;
    int search_field_x = search_icon_x + UI_TREE_ICON_SIZE + UI_TREE_ROW_PAD;
    int search_field_w = import_x - UI_TREE_ROW_PAD - search_field_x;
    int search_field_y = y + (TOOLBAR_HEIGHT - SEARCH_HEIGHT) / 2;
    icon_atlas_draw(ICON_search, search_icon_x, toolbar_icon_y, UI_TREE_ICON_SIZE);
    text_field_draw(&g_search_field, search_field_x, search_field_y, search_field_w, SEARCH_HEIGHT, 13);
    icon_atlas_draw(ICON_more, more_x, toolbar_icon_y, UI_TREE_ICON_SIZE);
    icon_atlas_draw(ICON_import_file, import_x, toolbar_icon_y, UI_TREE_ICON_SIZE);
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
    g_expanded_count = 0;
}
