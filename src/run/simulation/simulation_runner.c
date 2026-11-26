#include <stdio.h>

#include "run/simulation/simulation_runner.h"
#include "run/simulation/sim_profile.h"
#include "run/simulation/sim_clock.h"
#include "run/simulation/sim_input.h"
#include "run/simulation/sim_world.h"
#include "project/current_project.h"
#include "platform/common/window.h"
#include "editor/ui/draw_kit.h"
#include "core/i18n.h"
#include "editor/ui/dialog.h"
#include "editor/ui/windows.h"

static int g_running;
static int g_paused;
static int g_overlay = 1;
static float g_fps;
static int g_alternate_aspect;
static char g_error[256];
static sim_profile_t g_profile;
static sim_clock_t g_clock;
static sim_world_t *g_world;
static window_render_target_t *g_target;
static int g_target_width;
static int g_target_height;

static void set_error(const char *message) {
    snprintf(g_error, sizeof(g_error), "%s", message != NULL ? message : "");
}

static void target_destroy(void) {
    window_destroy_render_target(g_target);
    g_target = NULL;
    g_target_width = 0;
    g_target_height = 0;
}

static int target_create(void) {
    int width = g_profile.width;
    int height = g_profile.height;
    if (g_alternate_aspect && g_profile.alternate_width > 0 &&
        g_profile.alternate_height > 0) {
        width = g_profile.alternate_width;
        height = g_profile.alternate_height;
    }
    target_destroy();
    g_target = window_create_render_target(width, height, g_profile.nearest_filter);
    if (g_target == NULL) return 0;
    g_target_width = width;
    g_target_height = height;
    sim_world_set_origin(g_world, width / 2, height / 2);
    return 1;
}

static int rebuild(void) {
    if (!sim_world_build_from_scene(g_world)) {
        set_error("Simulation world could not be rebuilt");
        return 0;
    }
    sim_clock_init(&g_clock, g_profile.frame_rate);
    return target_create();
}

static int window_is_open(void);
static void window_update(int window_w, int window_h);
static void window_draw(int window_w, int window_h);

static ui_window_t g_window = UI_WINDOW_INIT(window_is_open, window_update, window_draw);
static ui_dialog_drag_t g_drag;

int simulation_runner_start(void) {
    const project_settings_t *settings;
    project_type_t type;
    if (g_running) return 0;
    settings = current_project_get_settings();
    type = settings != NULL ? settings->type : PROJECT_TYPE_MODERN;
    sim_profile_load(&g_profile, type);
    g_world = sim_world_create();
    if (g_world == NULL) {
        set_error("Simulation could not start");
        return 0;
    }
    sim_world_set_limits(g_world, g_profile.texture_memory_mb,
                         g_profile.max_texture_size);
    if (!sim_world_build_from_scene(g_world) || !target_create()) {
        sim_world_destroy(g_world);
        g_world = NULL;
        target_destroy();
        set_error("Simulation world could not be created");
        return 0;
    }
    sim_input_set_profile(type == PROJECT_TYPE_PS1 || type == PROJECT_TYPE_PS2);
    sim_clock_init(&g_clock, g_profile.frame_rate);
    ui_dialog_drag_reset(&g_drag);
    ui_windows_show(&g_window);
    g_alternate_aspect = 0;
    g_paused = 0;
    g_running = 1;
    set_error("");
    return 1;
}

void simulation_runner_stop(void) {
    target_destroy();
    sim_world_destroy(g_world);
    g_world = NULL;
    g_running = 0;
    g_paused = 0;
}

int simulation_runner_is_running(void) {
    return g_running;
}

const char *simulation_runner_error(void) {
    return g_error;
}

void simulation_runner_update(void) {
    int steps;
    int i;
    if (!g_running || g_world == NULL) return;
    sim_input_begin_frame();
    if (window_key_just_pressed_escape()) {
        simulation_runner_stop();
        return;
    }
    if (window_get_delta_time() > 0.0f) {
        g_fps = g_fps * 0.9f + (1.0f / window_get_delta_time()) * 0.1f;
    }
    steps = 0;
    sim_clock_update(&g_clock, window_get_delta_time(), &steps);
    for (i = 0; i < steps; i++) sim_world_update(g_world, (float)g_clock.step);
    if (sim_world_warning_count(g_world) > 0) {
        set_error(sim_world_warning(g_world));
    }
}

#define TITLEBAR_HEIGHT 40
#define TOOLBAR_HEIGHT 44
#define CLOSE_SIZE 22
#define BUTTON_HEIGHT 30
#define VIEW_MARGIN 8

static int window_is_open(void) {
    return g_running;
}

static void layout_view(int x, int y, int w, int h, int *view_x, int *view_y,
                        int *view_w, int *view_h) {
    int available_w = w - VIEW_MARGIN * 2;
    int available_h = h - TITLEBAR_HEIGHT - TOOLBAR_HEIGHT - VIEW_MARGIN * 2;
    float scale_x;
    float scale_y;
    float scale;
    if (available_w < 1) available_w = 1;
    if (available_h < 1) available_h = 1;
    scale_x = (float)available_w / (float)g_target_width;
    scale_y = (float)available_h / (float)g_target_height;
    scale = scale_x < scale_y ? scale_x : scale_y;
    if (g_profile.integer_scale && scale >= 1.0f) scale = (float)((int)scale);
    if (scale <= 0.0f) scale = 0.01f;
    *view_w = (int)((float)g_target_width * scale);
    *view_h = (int)((float)g_target_height * scale);
    *view_x = x + VIEW_MARGIN + (available_w - *view_w) / 2;
    *view_y = y + TITLEBAR_HEIGHT + TOOLBAR_HEIGHT + VIEW_MARGIN +
              (available_h - *view_h) / 2;
}

static void place_window(int interactive, int window_w, int window_h,
                         int *x, int *y, int *w, int *h) {
    int def_w = g_target_width + VIEW_MARGIN * 2;
    int def_h = g_target_height + TITLEBAR_HEIGHT + TOOLBAR_HEIGHT + VIEW_MARGIN * 2;
    if (def_w < 520) def_w = 520;
    if (def_h < 320) def_h = 320;
    ui_dialog_place(&g_drag, interactive, window_w, window_h, def_w, def_h,
                    420, 260, TITLEBAR_HEIGHT, 1, x, y, w, h);
}

static int button_width(const char *label) {
    return ui_kit_text_width(label, 14) + 28;
}

typedef struct {
    const char *label;
    int enabled;
    int primary;
    int x;
    int w;
} toolbar_button_t;

static int toolbar_layout(toolbar_button_t *buttons, int count, int x, int y) {
    int cursor = x + VIEW_MARGIN;
    int i;
    (void)y;
    for (i = 0; i < count; i++) {
        buttons[i].w = button_width(buttons[i].label);
        buttons[i].x = cursor;
        cursor += buttons[i].w + 6;
    }
    return cursor;
}

static void toolbar_fill(toolbar_button_t *buttons) {
    buttons[0].label = g_paused ? tr("Resume") : tr("Pause");
    buttons[0].enabled = 1;
    buttons[0].primary = 0;
    buttons[1].label = tr("Restart");
    buttons[1].enabled = 1;
    buttons[1].primary = 0;
    buttons[2].label = tr("Step");
    buttons[2].enabled = 1;
    buttons[2].primary = 0;
    buttons[3].label = tr("Aspect");
    buttons[3].enabled = g_profile.alternate_width > 0;
    buttons[3].primary = 0;
    buttons[4].label = g_overlay ? tr("Hide overlay") : tr("Show overlay");
    buttons[4].enabled = 1;
    buttons[4].primary = 0;
}

static void window_update(int window_w, int window_h) {
    toolbar_button_t buttons[5];
    int x;
    int y;
    int w;
    int h;
    int by;
    int i;
    int mx;
    int my;
    if (!g_running || g_world == NULL) return;
    simulation_runner_update();
    if (!g_running) return;
    place_window(1, window_w, window_h, &x, &y, &w, &h);
    mx = window_mouse_x();
    my = window_mouse_y();
    if (window_mouse_left_just_pressed() &&
        ui_dialog_close_button_hit(mx, my, x + w - 16 - CLOSE_SIZE,
                                   y + (TITLEBAR_HEIGHT - CLOSE_SIZE) / 2, CLOSE_SIZE)) {
        simulation_runner_stop();
        return;
    }
    toolbar_fill(buttons);
    toolbar_layout(buttons, 5, x, y);
    by = y + TITLEBAR_HEIGHT + (TOOLBAR_HEIGHT - BUTTON_HEIGHT) / 2;
    for (i = 0; i < 5; i++) {
        if (!buttons[i].enabled || !ui_kit_clicked(buttons[i].x, by, buttons[i].w, BUTTON_HEIGHT)) continue;
        if (i == 0) {
            g_paused = !g_paused;
            sim_clock_pause(&g_clock, g_paused);
        } else if (i == 1) {
            rebuild();
        } else if (i == 2) {
            sim_clock_step(&g_clock);
        } else if (i == 3) {
            g_alternate_aspect = !g_alternate_aspect;
            target_create();
        } else {
            g_overlay = !g_overlay;
        }
        break;
    }
}

static void draw_overlay(int view_x, int view_y, int view_w, int view_h) {
    char line[192];
    int i;
    if (!g_overlay) return;
    window_fill_rect(view_x + 8, view_y + 8, 200, 40, 0, 0, 0);
    snprintf(line, sizeof(line), "%s: %s", tr("Profile"), g_profile.label);
    ui_kit_text(line, 11, view_x + 14, view_y + 12, 240, 240, 240);
    snprintf(line, sizeof(line), "%s: %d", tr("FPS"), (int)(g_fps + 0.5f));
    ui_kit_text(line, 11, view_x + 14, view_y + 28, 220, 220, 220);
    for (i = 0; i < sim_world_warning_count(g_world) && i < 4; i++) {
        window_fill_rect(view_x + 8, view_y + view_h - 24 - i * 22, view_w - 16, 20, 92, 54, 28);
        ui_kit_text(tr(sim_world_warning_at(g_world, i)), 10, view_x + 14,
                    view_y + view_h - 20 - i * 22, 255, 232, 190);
    }
}

static void window_draw(int window_w, int window_h) {
    toolbar_button_t buttons[5];
    char title[160];
    int x;
    int y;
    int w;
    int h;
    int view_x;
    int view_y;
    int view_w;
    int view_h;
    int by;
    int i;
    if (!g_running || g_world == NULL || g_target == NULL) return;
    ui_dialog_draw_backdrop(window_w, window_h);
    place_window(0, window_w, window_h, &x, &y, &w, &h);
    ui_dialog_draw_frame(x, y, w, h);
    snprintf(title, sizeof(title), "%s  -  %s", tr("Simulation"), g_profile.label);
    ui_kit_text(title, 14, x + 16, y + (TITLEBAR_HEIGHT - ui_kit_text_height(title, 14)) / 2,
                235, 235, 235);
    ui_dialog_draw_close_button(x + w - 16 - CLOSE_SIZE,
                                y + (TITLEBAR_HEIGHT - CLOSE_SIZE) / 2, CLOSE_SIZE);
    toolbar_fill(buttons);
    toolbar_layout(buttons, 5, x, y);
    by = y + TITLEBAR_HEIGHT + (TOOLBAR_HEIGHT - BUTTON_HEIGHT) / 2;
    for (i = 0; i < 5; i++) {
        ui_kit_button(buttons[i].label, buttons[i].x, by, buttons[i].w, BUTTON_HEIGHT,
                      buttons[i].enabled, buttons[i].primary);
    }
    layout_view(x, y, w, h, &view_x, &view_y, &view_w, &view_h);
    if (window_begin_render_target(g_target)) {
        window_clear(0, 0, 0);
        sim_world_draw(g_world);
        window_end_render_target();
    }
    window_set_clip_rect(view_x, view_y, view_w, view_h);
    window_draw_render_target(g_target, view_x, view_y, view_w, view_h, g_profile.nearest_filter);
    draw_overlay(view_x, view_y, view_w, view_h);
    window_clear_clip_rect();
}
