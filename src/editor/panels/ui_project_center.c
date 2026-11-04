/* Project center screen: a recent projects list stored inside Rebax/Editor. */
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "ui_project_center.h"
#include "window.h"
#include "font.h"
#include "ui_common.h"
#include "project_dialog.h"
#include "project_catalog.h"
#include "file_manager.h"
#include "current_project.h"
#include "app_state.h"
#include "scene_tabs.h"
#include "scene_data.h"
#include "current_scene.h"

#define ROW_HEIGHT 58
#define LIST_PADDING 18
#define BUTTON_MARGIN 12
#define BUTTON_COUNT 4

typedef struct {
    labeled_button_t button;
    window_texture_t *shape;
    window_texture_t *text;
    const char *label;
} center_button_t;

typedef struct {
    char path[PROJECT_CATALOG_PATH_MAX];
    char name[PROJECT_CATALOG_NAME_MAX];
    window_texture_t *name_tex;
    window_texture_t *path_tex;
    window_texture_t *date_tex;
    int name_w, name_h, path_w, path_h, date_w, date_h;
} project_visual_t;

static const char *g_button_labels[BUTTON_COUNT] = {
    "Create Project", "Import Project", "Open Selected", "Remove from List"
};
static center_button_t g_buttons[BUTTON_COUNT];
static project_visual_t g_visuals[PROJECT_CATALOG_MAX];
static int g_visual_count = -1;
static int g_ready;
static int g_selected = -1;
static int g_scroll_first;
static int g_last_clicked_project = -1;
static float g_clock_seconds;
static float g_last_project_click_seconds = -1.0f;
static char g_selected_path[PROJECT_CATALOG_PATH_MAX];
static char g_notice[180];
static int g_notice_frames;
static window_texture_t *g_title_tex;
static window_texture_t *g_subtitle_tex;
static window_texture_t *g_empty_tex;
static window_texture_t *g_notice_tex;
static int g_notice_w, g_notice_h;
static int g_empty_w, g_empty_h;
static int g_title_w, g_title_h, g_subtitle_w, g_subtitle_h;

void ui_project_center_layout(int window_w, int window_h,
                               ui_rect_t *out_searchbar,
                               ui_rect_t *out_project_panel,
                               ui_rect_t *out_buttons_panel) {
    if (!out_searchbar || !out_project_panel || !out_buttons_panel) return;
    out_searchbar->x = 0; out_searchbar->y = 0;
    out_searchbar->w = window_w; out_searchbar->h = UI_SEARCHBAR_HEIGHT;
    out_project_panel->x = 20; out_project_panel->y = UI_SEARCHBAR_HEIGHT + 12;
    out_project_panel->w = window_w - UI_RIGHT_PANEL_WIDTH - UI_PANEL_GAP - 28;
    out_project_panel->h = window_h - UI_SEARCHBAR_HEIGHT - 30;
    out_buttons_panel->x = window_w - UI_RIGHT_PANEL_WIDTH - 12;
    out_buttons_panel->y = UI_SEARCHBAR_HEIGHT + 12;
    out_buttons_panel->w = UI_RIGHT_PANEL_WIDTH;
    out_buttons_panel->h = window_h - UI_SEARCHBAR_HEIGHT - 30;
}

static void set_notice(const char *message) {
    snprintf(g_notice, sizeof(g_notice), "%s", message ? message : "");
    g_notice_frames = 240;
    if (g_notice_tex) window_destroy_texture(g_notice_tex);
    g_notice_tex = ui_make_text_texture(g_notice, FONT_WEIGHT_REGULAR, 11, &g_notice_w, &g_notice_h);
}

static void destroy_visuals(void) {
    for (int i = 0; i < PROJECT_CATALOG_MAX; i++) {
        if (g_visuals[i].name_tex) window_destroy_texture(g_visuals[i].name_tex);
        if (g_visuals[i].path_tex) window_destroy_texture(g_visuals[i].path_tex);
        if (g_visuals[i].date_tex) window_destroy_texture(g_visuals[i].date_tex);
        memset(&g_visuals[i], 0, sizeof(g_visuals[i]));
    }
    g_visual_count = -1;
}

static void create_button(center_button_t *button, const char *label) {
    button->label = label;
    button->button = labeled_button_create(label, FONT_WEIGHT_REGULAR, 13, 12, 7, 4,
                                            UI_COLOR_BUTTON_BLUE.r, UI_COLOR_BUTTON_BLUE.g,
                                            UI_COLOR_BUTTON_BLUE.b);
    if (button->button.width > 0) {
        button->shape = window_create_texture(button->button.shape_img.pixels,
                                               button->button.shape_img.width,
                                               button->button.shape_img.height);
        button->text = window_create_texture(button->button.text_img.pixels,
                                              button->button.text_img.width,
                                              button->button.text_img.height);
    }
}

static void ensure_ready(void) {
    if (g_ready) return;
    g_ready = 1;
    font_init();
    project_catalog_load();
    g_title_tex = ui_make_text_texture("Projects", FONT_WEIGHT_BOLD, 18, &g_title_w, &g_title_h);
    g_subtitle_tex = ui_make_text_texture("Open a recent project or add an existing Rebax project.",
                                          FONT_WEIGHT_REGULAR, 12, &g_subtitle_w, &g_subtitle_h);
    g_empty_tex = ui_make_text_texture("No recent projects. Create or import one to begin.",
                                       FONT_WEIGHT_REGULAR, 12, &g_empty_w, &g_empty_h);
    for (int i = 0; i < BUTTON_COUNT; i++) create_button(&g_buttons[i], g_button_labels[i]);
}

static void build_visuals(void) {
    ensure_ready();
    int count = project_catalog_count();
    int changed = (count != g_visual_count);
    if (!changed) {
        for (int i = 0; i < count; i++) {
            const project_catalog_entry_t *entry = project_catalog_get(i);
            if (!entry || strcmp(entry->path, g_visuals[i].path) != 0 ||
                strcmp(entry->name, g_visuals[i].name) != 0) { changed = 1; break; }
        }
    }
    if (!changed) return;
    destroy_visuals();
    count = project_catalog_count();
    for (int i = 0; i < count; i++) {
        const project_catalog_entry_t *entry = project_catalog_get(i);
        if (!entry) continue;
        project_visual_t *v = &g_visuals[i];
        snprintf(v->path, sizeof(v->path), "%s", entry->path);
        snprintf(v->name, sizeof(v->name), "%s", entry->name);
        v->name_tex = ui_make_text_texture(v->name, FONT_WEIGHT_BOLD, 14, &v->name_w, &v->name_h);
        v->path_tex = ui_make_text_texture(v->path, FONT_WEIGHT_REGULAR, 11, &v->path_w, &v->path_h);
        char date[64] = "Last opened: unknown";
        if (entry->last_opened > 0) {
            time_t t = (time_t)entry->last_opened;
            struct tm *tmv = localtime(&t);
            if (tmv) strftime(date, sizeof(date), "Last opened: %Y-%m-%d %H:%M", tmv);
        }
        v->date_tex = ui_make_text_texture(date, FONT_WEIGHT_REGULAR, 10, &v->date_w, &v->date_h);
    }
    g_visual_count = count;
    if (g_selected_path[0]) {
        g_selected = -1;
        for (int i = 0; i < count; i++) if (strcmp(g_visuals[i].path, g_selected_path) == 0) g_selected = i;
    }
}

static int button_hit(const center_button_t *button, int x, int y, int w, int index) {
    int bx = x + (w - button->button.width) / 2;
    int by = y + 76 + index * (button->button.height + 12);
    return ui_point_in_rect(window_mouse_x(), window_mouse_y(), bx, by,
                            button->button.width, button->button.height);
}

static void open_selected_project(void);

static void on_import_project(const char *path, void *user_data) {
    (void)user_data;
    if (!path) return;
    if (!project_catalog_is_valid(path)) {
        set_notice("This is not a valid Rebax project (project.rebax or Assets is missing).");
        return;
    }
    if (project_catalog_add(path)) {
        snprintf(g_selected_path, sizeof(g_selected_path), "%s", path);
        build_visuals();
        open_selected_project();
    } else set_notice("Could not save the project list in the Editor folder.");
}

static void open_selected_project(void) {
    const project_catalog_entry_t *entry = project_catalog_get(g_selected);
    if (!entry) { set_notice("Select a project first."); return; }
    if (!project_catalog_is_valid(entry->path)) {
        set_notice("Project folder is unavailable or invalid.");
        project_catalog_remove(g_selected);
        g_selected = -1;
        g_selected_path[0] = '\0';
        return;
    }
    char path[PROJECT_CATALOG_PATH_MAX];
    snprintf(path, sizeof(path), "%s", entry->path);
    project_catalog_add(path);
    const char *current_path = current_project_get_path();
    if (current_path != NULL && strcmp(current_path, path) == 0) {
        app_state_set_screen(APP_SCREEN_EDITOR);
        return;
    }
    for (int i = 0; i < scene_tabs_count(); i++) {
        if (scene_tabs_get_dirty(i)) {
            set_notice("Save or close modified scenes before switching projects.");
            return;
        }
    }
    current_project_set_path(path);
    scene_tabs_shutdown();
    scene_tabs_init();
    app_state_set_screen(APP_SCREEN_EDITOR);
}

void ui_project_center_draw(int window_w, int window_h) {
    ensure_ready();
    g_clock_seconds += window_get_delta_time();
    build_visuals();
    ui_rect_t searchbar, projects, actions;
    ui_project_center_layout(window_w, window_h, &searchbar, &projects, &actions);
    window_fill_rect(0, 0, window_w, window_h, 27, 29, 33);
    window_fill_rect(searchbar.x, searchbar.y, searchbar.w, searchbar.h, 19, 19, 19);
    window_fill_rect(projects.x, projects.y, projects.w, projects.h, 40, 42, 46);
    window_fill_rect(actions.x, actions.y, actions.w, actions.h, 31, 32, 36);
    if (g_title_tex) window_draw_texture(g_title_tex, projects.x + LIST_PADDING, projects.y + 16, g_title_w, g_title_h);
    if (g_subtitle_tex) window_draw_texture(g_subtitle_tex, projects.x + LIST_PADDING, projects.y + 42, g_subtitle_w, g_subtitle_h);

    int list_top = projects.y + 76;
    int list_w = projects.w - LIST_PADDING * 2;
    int visible = (projects.h - 92) / ROW_HEIGHT;
    if (visible < 1) visible = 1;
    int count = project_catalog_count();
    int interactive = !project_dialog_is_open() && !file_manager_is_open();
    int max_first = count > visible ? count - visible : 0;
    int mx = window_mouse_x(), my = window_mouse_y();
    if (interactive && ui_point_in_rect(mx, my, projects.x + LIST_PADDING, list_top,
                                         list_w, visible * ROW_HEIGHT) && window_mouse_wheel_delta() != 0) {
        g_scroll_first -= window_mouse_wheel_delta();
        if (g_scroll_first < 0) g_scroll_first = 0;
        if (g_scroll_first > max_first) g_scroll_first = max_first;
    }
    if (interactive) {
        if (window_key_just_pressed_up() && count > 0) {
            if (g_selected < 0) g_selected = 0; else if (g_selected > 0) g_selected--;
        }
        if (window_key_just_pressed_down() && count > 0) {
            if (g_selected < 0) g_selected = 0; else if (g_selected + 1 < count) g_selected++;
        }
        if (g_selected >= 0 && g_selected < g_scroll_first) g_scroll_first = g_selected;
        if (g_selected >= g_scroll_first + visible) g_scroll_first = g_selected - visible + 1;
        if (g_scroll_first > max_first) g_scroll_first = max_first;
        if (g_selected >= 0 && g_selected < count) {
            const project_catalog_entry_t *entry = project_catalog_get(g_selected);
            if (entry) snprintf(g_selected_path, sizeof(g_selected_path), "%s", entry->path);
        }
        if (window_key_just_pressed_enter()) open_selected_project();
    }
    if (count == 0) {
        window_fill_rect(projects.x + LIST_PADDING, list_top, list_w, 58, 48, 50, 55);
        if (g_empty_tex) window_draw_texture_tinted(g_empty_tex, projects.x + LIST_PADDING + 14,
                                                     list_top + 20, g_empty_w, g_empty_h, 180, 184, 190);
    } else {
        for (int i = g_scroll_first; i < count && i < g_scroll_first + visible; i++) {
            int ry = list_top + (i - g_scroll_first) * ROW_HEIGHT;
            int selected = i == g_selected;
            const ui_color_t *color = selected ? &UI_COLOR_HOVER_BLUE : &UI_COLOR_PANEL_BG;
            window_fill_rect(projects.x + LIST_PADDING, ry, list_w, ROW_HEIGHT - 5, color->r, color->g, color->b);
            if (selected) window_fill_rect(projects.x + LIST_PADDING, ry, 3, ROW_HEIGHT - 5,
                                           UI_COLOR_BUTTON_BLUE.r, UI_COLOR_BUTTON_BLUE.g, UI_COLOR_BUTTON_BLUE.b);
            project_visual_t *v = &g_visuals[i];
            int tx = projects.x + LIST_PADDING + 14;
            if (v->name_tex) window_draw_texture(v->name_tex, tx, ry + 7, v->name_w, v->name_h);
            if (v->path_tex) window_draw_texture_tinted(v->path_tex, tx, ry + 27, v->path_w, v->path_h, 175, 180, 186);
            if (v->date_tex) window_draw_texture_tinted(v->date_tex, projects.x + projects.w - v->date_w - LIST_PADDING - 12,
                                                          ry + 8, v->date_w, v->date_h, 145, 150, 158);
            if (interactive && window_mouse_left_just_pressed() && ui_point_in_rect(window_mouse_x(), window_mouse_y(),
                    projects.x + LIST_PADDING, ry, list_w, ROW_HEIGHT - 5)) {
                int double_click = (g_last_clicked_project == i &&
                                    g_clock_seconds - g_last_project_click_seconds <= 0.45f);
                g_selected = i;
                snprintf(g_selected_path, sizeof(g_selected_path), "%s", v->path);
                if (double_click) open_selected_project();
                g_last_clicked_project = i;
                g_last_project_click_seconds = g_clock_seconds;
            }
        }
    }
    for (int i = 0; i < BUTTON_COUNT; i++) {
        center_button_t *b = &g_buttons[i];
        int bx = actions.x + (actions.w - b->button.width) / 2;
        int by = actions.y + 76 + i * (b->button.height + 12);
        if (b->shape) window_draw_texture(b->shape, bx, by, b->button.width, b->button.height);
        if (b->text) window_draw_texture(b->text, bx + b->button.text_offset_x, by + b->button.text_offset_y,
                                         b->button.text_img.width, b->button.text_img.height);
        if (interactive && window_mouse_left_just_pressed() && button_hit(b, actions.x, actions.y, actions.w, i)) {
            if (i == 0) project_dialog_open();
            else if (i == 1) file_manager_open(FILE_MANAGER_ROOT_DEVICE, FILE_MANAGER_MODE_PICK_FOLDER,
                                               NULL, 0, on_import_project, NULL);
            else if (i == 2) open_selected_project();
            else if (g_selected >= 0 && project_catalog_remove(g_selected)) {
                g_selected = -1; g_selected_path[0] = '\0';
                set_notice("Removed from this list. Project files were not deleted.");
            }
        }
    }
    if (g_notice_frames > 0) {
        if (g_notice_tex) window_draw_texture_tinted(g_notice_tex, actions.x + 12,
                                      actions.y + actions.h - g_notice_h - 18,
                                      g_notice_w, g_notice_h, 220, 195, 125);
        g_notice_frames--;
    }
    (void)list_w;
}

void ui_project_center_shutdown(void) {
    destroy_visuals();
    for (int i = 0; i < BUTTON_COUNT; i++) {
        if (g_buttons[i].shape) window_destroy_texture(g_buttons[i].shape);
        if (g_buttons[i].text) window_destroy_texture(g_buttons[i].text);
        labeled_button_free(&g_buttons[i].button);
        memset(&g_buttons[i], 0, sizeof(g_buttons[i]));
    }
    if (g_title_tex) window_destroy_texture(g_title_tex);
    if (g_subtitle_tex) window_destroy_texture(g_subtitle_tex);
    if (g_empty_tex) window_destroy_texture(g_empty_tex);
    if (g_notice_tex) window_destroy_texture(g_notice_tex);
    g_title_tex = g_subtitle_tex = g_empty_tex = g_notice_tex = NULL;
    g_selected_path[0] = '\0'; g_selected = -1;
    g_last_clicked_project = -1; g_last_project_click_seconds = -1.0f; g_clock_seconds = 0.0f;
    g_ready = 0;
}
