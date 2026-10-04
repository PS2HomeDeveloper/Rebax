/* 2D editor camera, expandable virtual-world scrollbars, and transform gizmos. */
#include <stdlib.h>
#include <math.h>
#include <float.h>

#include "viewport_2d.h"
#include "window.h"
#include "scene_data.h"
#include "node_editor_registry.h"
#include "node_registry.h"
#include "node_types.h"
#include "properties_panel.h"
#include "nodes_editor/sprite2d.h"

#define BG_R 0x2B
#define BG_G 0x2B
#define BG_B 0x2E
#define GRID_R 0x3F
#define GRID_G 0x3F
#define GRID_B 0x42
#define AXIS_X_R 220
#define AXIS_X_G 60
#define AXIS_X_B 60
#define AXIS_Y_R 60
#define AXIS_Y_G 200
#define AXIS_Y_B 90
#define GRID_SPACING 32
#define PS2_WIDTH 640
#define PS2_HEIGHT 448
#define ZOOM_MIN 0.1f
#define ZOOM_MAX 8.0f
#define ZOOM_WHEEL_MULT 1.15f
#define GIZMO_LENGTH 54
#define VIEW_SCROLLBAR_SIZE 10
#define VIEW_SCROLLBAR_MIN_THUMB 24
#define INITIAL_WORLD_HALF_RANGE 1024.0f
#define WORLD_RANGE_MARGIN_VIEWS 2.0f

typedef enum { DRAG_NONE, DRAG_MOVE, DRAG_AXIS_X, DRAG_AXIS_Y, DRAG_MARQUEE } drag_mode_t;
typedef struct {
    int dragging;
    int start_mouse;
    int start_thumb;
    float start_camera;
} axis_scroll_state_t;

static float g_pan_offset_x, g_pan_offset_y, g_zoom = 1.0f;
static int g_pan_active;
static int g_focus_scene_pending;
static drag_mode_t g_drag_mode;
static float g_last_world_x, g_last_world_y;
static int g_marquee_start_x, g_marquee_start_y, g_marquee_x, g_marquee_y;
static float g_world_min_x = -INITIAL_WORLD_HALF_RANGE;
static float g_world_max_x = INITIAL_WORLD_HALF_RANGE;
static float g_world_min_y = -INITIAL_WORLD_HALF_RANGE;
static float g_world_max_y = INITIAL_WORLD_HALF_RANGE;
static axis_scroll_state_t g_horizontal_scroll, g_vertical_scroll;

void viewport_2d_focus_scene(void) { g_focus_scene_pending = 1; }

static float camera_world_x(void) { return -g_pan_offset_x / g_zoom; }
static float camera_world_y(void) { return -g_pan_offset_y / g_zoom; }

static int is_visible_2d_node(int index, const node_registry_entry_t **out_schema,
                              node_property_value_t **out_values) {
    node_type_t type = scene_data_get_type(index);
    const node_editor_registry_entry_t *editor = node_editor_registry_get(type);
    const node_registry_entry_t *schema = node_registry_get(type);
    node_property_value_t *values = scene_data_get_values(index);
    if (editor == NULL || editor->draw_2d == NULL || schema == NULL || values == NULL ||
        schema->property_count < 2 || schema->properties[0].type != NODE_PROPERTY_TYPE_FLOAT ||
        schema->properties[1].type != NODE_PROPERTY_TYPE_FLOAT) return 0;
    if (out_schema) *out_schema = schema;
    if (out_values) *out_values = values;
    return 1;
}

static void expand_world_range(int view_w, int view_h) {
    float visible_w = (float)view_w / g_zoom;
    float visible_h = (float)view_h / g_zoom;
    float margin_x = fmaxf(visible_w * WORLD_RANGE_MARGIN_VIEWS, 256.0f);
    float margin_y = fmaxf(visible_h * WORLD_RANGE_MARGIN_VIEWS, 256.0f);
    float cx = camera_world_x(), cy = camera_world_y();
    if (cx < g_world_min_x + visible_w * 0.25f) g_world_min_x = cx - margin_x;
    if (cx > g_world_max_x - visible_w * 0.25f) g_world_max_x = cx + margin_x;
    if (cy < g_world_min_y + visible_h * 0.25f) g_world_min_y = cy - margin_y;
    if (cy > g_world_max_y - visible_h * 0.25f) g_world_max_y = cy + margin_y;

    for (int i = 0; i < scene_data_get_node_count(); i++) {
        const node_registry_entry_t *schema;
        node_property_value_t *values;
        if (!is_visible_2d_node(i, &schema, &values)) continue;
        float px = values[0].f, py = values[1].f;
        float node_margin_x = margin_x, node_margin_y = margin_y;
        if (scene_data_get_type(i) == NODE_TYPE_SPRITE_2D && schema->property_count >= 9) {
            float half_w = (values[6].i > 0 ? values[6].i : 32) * fabsf(values[3].f) * 0.5f;
            float half_h = (values[7].i > 0 ? values[7].i : 32) * fabsf(values[4].f) * 0.5f;
            node_margin_x = fmaxf(node_margin_x, half_w + visible_w);
            node_margin_y = fmaxf(node_margin_y, half_h + visible_h);
        }
        if (px < g_world_min_x) g_world_min_x = px - node_margin_x;
        if (px > g_world_max_x) g_world_max_x = px + node_margin_x;
        if (py < g_world_min_y) g_world_min_y = py - node_margin_y;
        if (py > g_world_max_y) g_world_max_y = py + node_margin_y;
    }
}

static void move_selected(float dx, float dy) {
    for (int i = 0; i < scene_data_get_node_count(); i++) {
        if (!scene_data_is_selected(i) || !is_visible_2d_node(i, NULL, NULL)) continue;
        node_property_value_t *values = scene_data_get_values(i);
        scene_data_set_float_property(i, 0, values[0].f + dx);
        scene_data_set_float_property(i, 1, values[1].f + dy);
    }
}

static int point_near(int px, int py, int x, int y, int radius) {
    return abs(px - x) <= radius && abs(py - y) <= radius;
}

static int hit_node(int mx, int my, int x, int y, int w, int h) {
    for (int i = scene_data_get_node_count() - 1; i >= 0; i--) {
        const node_registry_entry_t *schema;
        node_property_value_t *values;
        if (!is_visible_2d_node(i, &schema, &values)) continue;
        int sx, sy;
        viewport_2d_project(values[0].f, values[1].f, x, y, w, h, &sx, &sy);
        if (scene_data_get_type(i) == NODE_TYPE_SPRITE_2D &&
            sprite2d_editor_hit_test(values, schema->property_count, mx, my, sx, sy, g_zoom)) return i;
        if (point_near(mx, my, sx, sy, 12)) return i;
    }
    return -1;
}

static void finish_marquee(int x, int y, int w, int h) {
    int left = g_marquee_start_x < g_marquee_x ? g_marquee_start_x : g_marquee_x;
    int right = g_marquee_start_x > g_marquee_x ? g_marquee_start_x : g_marquee_x;
    int top = g_marquee_start_y < g_marquee_y ? g_marquee_start_y : g_marquee_y;
    int bottom = g_marquee_start_y > g_marquee_y ? g_marquee_start_y : g_marquee_y;
    if (right - left < 4 && bottom - top < 4) return;
    scene_data_clear_selection();
    int last = -1;
    for (int i = 0; i < scene_data_get_node_count(); i++) {
        if (!is_visible_2d_node(i, NULL, NULL)) continue;
        node_property_value_t *values = scene_data_get_values(i);
        int sx, sy;
        viewport_2d_project(values[0].f, values[1].f, x, y, w, h, &sx, &sy);
        if (sx >= left && sx <= right && sy >= top && sy <= bottom) {
            scene_data_add_to_selection(i);
            last = i;
        }
    }
    if (last >= 0) properties_panel_set_selected(last);
    else properties_panel_clear_selection();
}

static void axis_thumb(float camera, float range_min, float range_max, float zoom,
                       int track_start, int track_len, int view_px,
                       int *out_thumb_start, int *out_thumb_len) {
    float span = range_max - range_min;
    if (span < 1.0f) span = 1.0f;
    float content_px = span * zoom + (float)view_px;
    int thumb = (int)((float)track_len * (float)view_px / content_px);
    if (thumb < VIEW_SCROLLBAR_MIN_THUMB) thumb = VIEW_SCROLLBAR_MIN_THUMB;
    if (thumb > track_len) thumb = track_len;
    float max_scroll = span * zoom;
    float progress = max_scroll > 0.0f ? (camera - range_min) * zoom / max_scroll : 0.0f;
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    int travel = track_len - thumb;
    *out_thumb_start = track_start + (int)(progress * (float)travel + 0.5f);
    *out_thumb_len = thumb;
}

static void update_axis_scroll(axis_scroll_state_t *state, int horizontal,
                               int track_start, int track_len, int track_cross_start,
                               int track_cross_len, int view_px,
                               float range_min, float range_max, float *camera) {
    int mouse = horizontal ? window_mouse_x() : window_mouse_y();
    int cross = horizontal ? window_mouse_y() : window_mouse_x();
    int in_track = mouse >= track_start && mouse < track_start + track_len &&
                   cross >= track_cross_start && cross < track_cross_start + track_cross_len;
    int thumb_start, thumb_len;
    axis_thumb(*camera, range_min, range_max, g_zoom, track_start, track_len, view_px,
               &thumb_start, &thumb_len);
    if (window_mouse_left_just_pressed() && in_track) {
        if (mouse >= thumb_start && mouse < thumb_start + thumb_len) {
            state->dragging = 1;
            state->start_mouse = mouse;
            state->start_thumb = thumb_start;
            state->start_camera = *camera;
        } else {
            int target_thumb = mouse - thumb_len / 2;
            int travel = track_len - thumb_len;
            float progress = travel > 0 ? (float)(target_thumb - track_start) / (float)travel : 0.5f;
            if (progress < 0.0f) progress = 0.0f;
            if (progress > 1.0f) progress = 1.0f;
            *camera = range_min + progress * (range_max - range_min);
            state->dragging = 1;
            state->start_mouse = mouse;
            state->start_thumb = target_thumb;
            state->start_camera = *camera;
        }
    }
    if (!window_mouse_left_down()) state->dragging = 0;
    if (state->dragging && window_mouse_left_down()) {
        int travel = track_len - thumb_len;
        float progress = travel > 0 ? (float)(state->start_thumb - track_start + mouse - state->start_mouse) /
                                     (float)travel : 0.5f;
        if (progress < 0.0f) progress = 0.0f;
        if (progress > 1.0f) progress = 1.0f;
        *camera = range_min + progress * (range_max - range_min);
    }
}

static void draw_axis_scrollbar(int horizontal, int track_start, int track_len,
                                int cross_start, int cross_len, int view_px,
                                float range_min, float range_max, float camera) {
    int thumb_start, thumb_len;
    axis_thumb(camera, range_min, range_max, g_zoom, track_start, track_len, view_px,
               &thumb_start, &thumb_len);
    unsigned char thumb_color = 115;
    int mx = window_mouse_x(), my = window_mouse_y();
    int hover = horizontal
        ? (mx >= thumb_start && mx < thumb_start + thumb_len && my >= cross_start && my < cross_start + cross_len)
        : (my >= thumb_start && my < thumb_start + thumb_len && mx >= cross_start && mx < cross_start + cross_len);
    if (hover) thumb_color = 155;
    if (horizontal) {
        window_fill_rect(track_start, cross_start, track_len, cross_len, 40, 43, 48);
        window_fill_rect(thumb_start, cross_start + 1, thumb_len, cross_len - 2, thumb_color, thumb_color, thumb_color);
    } else {
        window_fill_rect(cross_start, track_start, cross_len, track_len, 40, 43, 48);
        window_fill_rect(cross_start + 1, thumb_start, cross_len - 2, thumb_len, thumb_color, thumb_color, thumb_color);
    }
}

void viewport_2d_update(int x, int y, int w, int h) {
    if (w <= VIEW_SCROLLBAR_SIZE || h <= VIEW_SCROLLBAR_SIZE) return;
    int view_w = w - VIEW_SCROLLBAR_SIZE;
    int view_h = h - VIEW_SCROLLBAR_SIZE;
    int mx = window_mouse_x(), my = window_mouse_y();
    int inside_view = mx >= x && mx < x + view_w && my >= y && my < y + view_h;

    if (g_focus_scene_pending) {
        float min_x = FLT_MAX, min_y = FLT_MAX, max_x = -FLT_MAX, max_y = -FLT_MAX;
        for (int i = 0; i < scene_data_get_node_count(); i++) {
            if (!is_visible_2d_node(i, NULL, NULL)) continue;
            node_property_value_t *values = scene_data_get_values(i);
            if (values[0].f < min_x) min_x = values[0].f;
            if (values[0].f > max_x) max_x = values[0].f;
            if (values[1].f < min_y) min_y = values[1].f;
            if (values[1].f > max_y) max_y = values[1].f;
        }
        if (min_x != FLT_MAX) {
            g_pan_offset_x = -((min_x + max_x) * 0.5f) * g_zoom;
            g_pan_offset_y = -((min_y + max_y) * 0.5f) * g_zoom;
        }
        g_focus_scene_pending = 0;
    }

    float camera_x = camera_world_x(), camera_y = camera_world_y();
    expand_world_range(view_w, view_h);
    update_axis_scroll(&g_horizontal_scroll, 1, x, view_w, y + view_h,
                       VIEW_SCROLLBAR_SIZE, view_w, g_world_min_x, g_world_max_x, &camera_x);
    update_axis_scroll(&g_vertical_scroll, 0, y, view_h, x + view_w,
                       VIEW_SCROLLBAR_SIZE, view_h, g_world_min_y, g_world_max_y, &camera_y);
    g_pan_offset_x = -camera_x * g_zoom;
    g_pan_offset_y = -camera_y * g_zoom;

    if (!g_pan_active && inside_view && window_mouse_middle_just_pressed()) g_pan_active = 1;
    else if (g_pan_active && !window_mouse_middle_down()) g_pan_active = 0;
    if (g_pan_active) {
        g_pan_offset_x += (float)window_mouse_delta_x();
        g_pan_offset_y += (float)window_mouse_delta_y();
    }

    int wheel = window_mouse_wheel_delta();
    if (wheel != 0 && inside_view) {
        float old_zoom = g_zoom;
        float world_x = ((float)mx - ((float)(x + view_w / 2) + g_pan_offset_x)) / old_zoom;
        float world_y = ((float)my - ((float)(y + view_h / 2) + g_pan_offset_y)) / old_zoom;
        g_zoom *= (wheel > 0) ? ZOOM_WHEEL_MULT : (1.0f / ZOOM_WHEEL_MULT);
        if (g_zoom < ZOOM_MIN) g_zoom = ZOOM_MIN;
        if (g_zoom > ZOOM_MAX) g_zoom = ZOOM_MAX;
        g_pan_offset_x = (float)mx - world_x * g_zoom - (float)(x + view_w / 2);
        g_pan_offset_y = (float)my - world_y * g_zoom - (float)(y + view_h / 2);
    }

    if (inside_view && window_mouse_left_just_pressed() &&
        !g_horizontal_scroll.dragging && !g_vertical_scroll.dragging) {
        int selected = scene_data_get_selected_index();
        int sx = 0, sy = 0;
        if (selected >= 0 && scene_data_is_selected(selected) && is_visible_2d_node(selected, NULL, NULL)) {
            node_property_value_t *v = scene_data_get_values(selected);
            viewport_2d_project(v[0].f, v[1].f, x, y, view_w, view_h, &sx, &sy);
        }
        if (selected >= 0 && point_near(mx, my, sx + GIZMO_LENGTH, sy, 9)) {
            g_drag_mode = DRAG_AXIS_X;
        } else if (selected >= 0 && point_near(mx, my, sx, sy - GIZMO_LENGTH, 9)) {
            g_drag_mode = DRAG_AXIS_Y;
        } else {
            int hit = hit_node(mx, my, x, y, view_w, view_h);
            if (hit >= 0) {
                if (!scene_data_is_selected(hit)) scene_data_select_index(hit);
                else scene_data_add_to_selection(hit);
                properties_panel_set_selected(hit);
                g_drag_mode = DRAG_MOVE;
            } else {
                scene_data_clear_selection();
                properties_panel_clear_selection();
                g_marquee_start_x = g_marquee_x = mx;
                g_marquee_start_y = g_marquee_y = my;
                g_drag_mode = DRAG_MARQUEE;
            }
        }
        viewport_2d_unproject(mx, my, x, y, view_w, view_h, &g_last_world_x, &g_last_world_y);
    }

    if (g_drag_mode == DRAG_MARQUEE && window_mouse_left_down()) {
        g_marquee_x = mx; g_marquee_y = my;
    } else if ((g_drag_mode == DRAG_MOVE || g_drag_mode == DRAG_AXIS_X || g_drag_mode == DRAG_AXIS_Y)
               && window_mouse_left_down()) {
        float world_x, world_y;
        viewport_2d_unproject(mx, my, x, y, view_w, view_h, &world_x, &world_y);
        float dx = world_x - g_last_world_x;
        float dy = world_y - g_last_world_y;
        if (g_drag_mode == DRAG_AXIS_X) dy = 0.0f;
        if (g_drag_mode == DRAG_AXIS_Y) dx = 0.0f;
        move_selected(dx, dy);
        g_last_world_x = world_x;
        g_last_world_y = world_y;
    }
    if (g_drag_mode != DRAG_NONE && !window_mouse_left_down()) {
        if (g_drag_mode == DRAG_MARQUEE) finish_marquee(x, y, view_w, view_h);
        g_drag_mode = DRAG_NONE;
    }

    if (inside_view && g_drag_mode == DRAG_NONE) {
        float step = window_key_down_shift() ? 10.0f : 1.0f;
        float dx = 0.0f, dy = 0.0f;
        if (window_key_just_pressed_left()) dx -= step;
        if (window_key_just_pressed_right()) dx += step;
        if (window_key_just_pressed_up()) dy -= step;
        if (window_key_just_pressed_down()) dy += step;
        if (dx != 0.0f || dy != 0.0f) move_selected(dx, dy);
    }

    expand_world_range(view_w, view_h);
}

static void draw_gizmo(int x, int y) {
    window_fill_rect(x, y - 1, GIZMO_LENGTH, 3, AXIS_X_R, AXIS_X_G, AXIS_X_B);
    window_fill_rect(x, y - GIZMO_LENGTH, 3, GIZMO_LENGTH, AXIS_Y_R, AXIS_Y_G, AXIS_Y_B);
    for (int offset = -5; offset <= 5; offset++) {
        int width = 5 - abs(offset);
        window_fill_rect(x + GIZMO_LENGTH - width, y + offset, width + 1, 1,
                         AXIS_X_R, AXIS_X_G, AXIS_X_B);
        int height = 5 - abs(offset);
        window_fill_rect(x + offset, y - GIZMO_LENGTH, 1, height + 1,
                         AXIS_Y_R, AXIS_Y_G, AXIS_Y_B);
    }
    window_fill_rect(x - 4, y - 4, 8, 8, 235, 235, 235);
}

static void draw_marquee(void) {
    int left = g_marquee_start_x < g_marquee_x ? g_marquee_start_x : g_marquee_x;
    int right = g_marquee_start_x > g_marquee_x ? g_marquee_start_x : g_marquee_x;
    int top = g_marquee_start_y < g_marquee_y ? g_marquee_start_y : g_marquee_y;
    int bottom = g_marquee_start_y > g_marquee_y ? g_marquee_start_y : g_marquee_y;
    if (right <= left || bottom <= top) return;
    window_fill_rect(left, top, right - left, 1, 75, 155, 255);
    window_fill_rect(left, bottom, right - left, 1, 75, 155, 255);
    window_fill_rect(left, top, 1, bottom - top, 75, 155, 255);
    window_fill_rect(right, top, 1, bottom - top, 75, 155, 255);
}

void viewport_2d_draw(int x, int y, int w, int h) {
    if (w <= VIEW_SCROLLBAR_SIZE || h <= VIEW_SCROLLBAR_SIZE) return;
    int view_w = w - VIEW_SCROLLBAR_SIZE;
    int view_h = h - VIEW_SCROLLBAR_SIZE;
    window_set_clip_rect(x, y, view_w, view_h);
    window_fill_rect(x, y, view_w, view_h, BG_R, BG_G, BG_B);
    int origin_x = x + view_w / 2 + (int)g_pan_offset_x;
    int origin_y = y + view_h / 2 + (int)g_pan_offset_y;
    int spacing = (int)((float)GRID_SPACING * g_zoom + 0.5f);
    if (spacing < 1) spacing = 1;
    int start_kx = (int)floorf((float)(x - origin_x) / (float)spacing) - 1;
    for (int k = start_kx; ; k++) {
        int gx = origin_x + k * spacing;
        if (gx > x + view_w) break;
        if (gx >= x) window_fill_rect(gx, y, 1, view_h, GRID_R, GRID_G, GRID_B);
    }
    int start_ky = (int)floorf((float)(y - origin_y) / (float)spacing) - 1;
    for (int k = start_ky; ; k++) {
        int gy = origin_y + k * spacing;
        if (gy > y + view_h) break;
        if (gy >= y) window_fill_rect(x, gy, view_w, 1, GRID_R, GRID_G, GRID_B);
    }
    window_fill_rect(x, origin_y - 1, view_w, 2, AXIS_X_R, AXIS_X_G, AXIS_X_B);
    window_fill_rect(origin_x - 1, y, 2, view_h, AXIS_Y_R, AXIS_Y_G, AXIS_Y_B);

    int left = origin_x - (int)(PS2_WIDTH * g_zoom / 2.0f);
    int top = origin_y - (int)(PS2_HEIGHT * g_zoom / 2.0f);
    int frame_w = (int)(PS2_WIDTH * g_zoom), frame_h = (int)(PS2_HEIGHT * g_zoom);
    window_fill_rect(left, top, frame_w, 2, 245, 190, 55);
    window_fill_rect(left, top + frame_h - 2, frame_w, 2, 245, 190, 55);
    window_fill_rect(left, top, 2, frame_h, 245, 190, 55);
    window_fill_rect(left + frame_w - 2, top, 2, frame_h, 245, 190, 55);

    for (int i = 0; i < scene_data_get_node_count(); i++) {
        node_type_t type = scene_data_get_type(i);
        const node_editor_registry_entry_t *ed = node_editor_registry_get(type);
        if (!ed || !ed->draw_2d) continue;
        const node_registry_entry_t *schema = node_registry_get(type);
        node_property_value_t *values = scene_data_get_values(i);
        if (!schema || !values) continue;
        ed->draw_2d(values, schema->property_count, x, y, view_w, view_h, scene_data_is_selected(i));
    }
    int selected = scene_data_get_selected_index();
    if (selected >= 0 && scene_data_is_selected(selected) && is_visible_2d_node(selected, NULL, NULL)) {
        node_property_value_t *v = scene_data_get_values(selected);
        int sx, sy;
        viewport_2d_project(v[0].f, v[1].f, x, y, view_w, view_h, &sx, &sy);
        draw_gizmo(sx, sy);
    }
    if (g_drag_mode == DRAG_MARQUEE && window_mouse_left_down()) draw_marquee();
    window_clear_clip_rect();

    float cx = camera_world_x(), cy = camera_world_y();
    draw_axis_scrollbar(1, x, view_w, y + view_h, VIEW_SCROLLBAR_SIZE, view_w,
                        g_world_min_x, g_world_max_x, cx);
    draw_axis_scrollbar(0, y, view_h, x + view_w, VIEW_SCROLLBAR_SIZE, view_h,
                        g_world_min_y, g_world_max_y, cy);
}

void viewport_2d_project(float world_x, float world_y,
                          int cam_x, int cam_y, int cam_w, int cam_h,
                          int *out_screen_x, int *out_screen_y) {
    int origin_x = cam_x + cam_w / 2 + (int)g_pan_offset_x;
    int origin_y = cam_y + cam_h / 2 + (int)g_pan_offset_y;
    *out_screen_x = origin_x + (int)(world_x * g_zoom);
    *out_screen_y = origin_y + (int)(world_y * g_zoom);
}

void viewport_2d_unproject(int screen_x, int screen_y,
                            int cam_x, int cam_y, int cam_w, int cam_h,
                            float *out_world_x, float *out_world_y) {
    int origin_x = cam_x + cam_w / 2 + (int)g_pan_offset_x;
    int origin_y = cam_y + cam_h / 2 + (int)g_pan_offset_y;
    if (out_world_x) *out_world_x = ((float)screen_x - origin_x) / g_zoom;
    if (out_world_y) *out_world_y = ((float)screen_y - origin_y) / g_zoom;
}

float viewport_2d_get_zoom(void) { return g_zoom; }
