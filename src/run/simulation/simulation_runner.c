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

static void layout_view(int width, int height, int *x, int *y,
                        int *draw_width, int *draw_height) {
    int toolbar = 52;
    int margin = 8;
    int available_width = width - margin * 2;
    int available_height = height - toolbar - margin * 2;
    float scale_x;
    float scale_y;
    float scale;
    if (available_width < 1) available_width = 1;
    if (available_height < 1) available_height = 1;
    scale_x = (float)available_width / (float)g_target_width;
    scale_y = (float)available_height / (float)g_target_height;
    scale = scale_x < scale_y ? scale_x : scale_y;
    if (g_profile.integer_scale && scale >= 1.0f) {
        scale = (float)((int)scale);
    }
    if (scale <= 0.0f) scale = 0.01f;
    *draw_width = (int)((float)g_target_width * scale);
    *draw_height = (int)((float)g_target_height * scale);
    *x = margin + (available_width - *draw_width) / 2;
    *y = toolbar + margin + (available_height - *draw_height) / 2;
}

static void draw_overlay(int width, int height, int view_x, int view_y,
                         int view_width, int view_height) {
    char line[192];
    int i;
    int y = view_y + 8;
    (void)width;
    (void)height;
    if (!g_overlay) return;
    window_fill_rect(view_x + 8, view_y + 8, 220, 22, 0, 0, 0);
    snprintf(line, sizeof(line), "%s: %s", tr("Profile"), g_profile.label);
    ui_kit_text(line, 11, view_x + 14, y + 3, 240, 240, 240);
    snprintf(line, sizeof(line), "%s: %d", tr("FPS"), (int)(g_fps + 0.5f));
    ui_kit_text(line, 11, view_x + 14, y + 19, 220, 220, 220);
    for (i = 0; i < sim_world_warning_count(g_world) && i < 4; i++) {
        window_fill_rect(view_x + 8, view_y + view_height - 24 - i * 22,
                         view_width - 16, 20, 92, 54, 28);
        ui_kit_text(tr(sim_world_warning_at(g_world, i)), 10,
                    view_x + 14, view_y + view_height - 20 - i * 22,
                    255, 232, 190);
    }
}

void simulation_runner_draw(int width, int height) {
    int view_x;
    int view_y;
    int view_width;
    int view_height;
    int button_x;
    int button_y = 8;
    int button_width = 76;
    int gap = 4;
    if (!g_running || g_world == NULL || g_target == NULL) return;
    window_clear(18, 20, 24);
    layout_view(width, height, &view_x, &view_y, &view_width, &view_height);
    window_set_clip_rect(view_x, view_y, view_width, view_height);
    if (window_begin_render_target(g_target)) {
        window_clear(0, 0, 0);
        sim_world_draw(g_world);
        window_end_render_target();
    }
    window_draw_render_target(g_target, view_x, view_y, view_width, view_height,
                              g_profile.nearest_filter);
    window_clear_clip_rect();
    button_x = 8;
    ui_kit_button(g_paused ? tr("Resume") : tr("Pause"), button_x, button_y,
                  button_width, 32, 1, 0);
    if (ui_kit_clicked(button_x, button_y, button_width, 32)) {
        g_paused = !g_paused;
        sim_clock_pause(&g_clock, g_paused);
    }
    button_x += button_width + gap;
    ui_kit_button(tr("Restart"), button_x, button_y, button_width, 32, 1, 0);
    if (ui_kit_clicked(button_x, button_y, button_width, 32)) rebuild();
    button_x += button_width + gap;
    ui_kit_button(tr("Step"), button_x, button_y, button_width, 32, 1, 0);
    if (ui_kit_clicked(button_x, button_y, button_width, 32)) sim_clock_step(&g_clock);
    button_x += button_width + gap;
    ui_kit_button(tr("Aspect"), button_x, button_y, button_width, 32,
                  g_profile.alternate_width > 0, 0);
    if (g_profile.alternate_width > 0 &&
        ui_kit_clicked(button_x, button_y, button_width, 32)) {
        g_alternate_aspect = !g_alternate_aspect;
        target_create();
    }
    button_x += button_width + gap;
    ui_kit_button(g_overlay ? tr("Overlay") : tr("Show overlay"), button_x,
                  button_y, button_width, 32, 1, 0);
    if (ui_kit_clicked(button_x, button_y, button_width, 32)) g_overlay = !g_overlay;
    button_x += button_width + gap;
    ui_kit_button(tr("Stop"), button_x, button_y, button_width, 32, 1, 1);
    if (ui_kit_clicked(button_x, button_y, button_width, 32)) {
        simulation_runner_stop();
        return;
    }
    draw_overlay(width, height, view_x, view_y, view_width, view_height);
}
