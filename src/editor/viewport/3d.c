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

#include <math.h>
#include <stddef.h>

#include "editor/viewport/3d.h"
#include "platform/common/window.h"
#include "scene/scene_data.h"
#include "dev_systems/nodes/editor/node_editor_registry.h"

#define BG_R  0x1A
#define BG_G  0x1A
#define BG_B  0x1D

#define GRID_R  0x3F
#define GRID_G  0x3F
#define GRID_B  0x42

#define AXIS_X_R  220
#define AXIS_X_G   60
#define AXIS_X_B   60

#define AXIS_Z_R   60
#define AXIS_Z_G  200
#define AXIS_Z_B   90

#define GRID_HALF_EXTENT   12
#define GRID_STEP          1.0f

#define CAM_FOV_DEG   60.0f
#define CAM_NEAR       0.15f

#define GRID_LINE_THICKNESS  1
#define AXIS_LINE_THICKNESS  2

#define VP3D_PI  3.14159265358979323846f
#define PITCH_LIMIT  (89.0f * VP3D_PI / 180.0f)

static float g_cam_pos_x    = 9.0f;
static float g_cam_pos_y    = 7.0f;
static float g_cam_pos_z    = 9.0f;
static float g_cam_yaw      = 0.0f;
static float g_cam_pitch    = 0.0f;
static int   g_cam_ready    = 0;

static int   g_fly_active   = 0;
static float g_fly_speed    = 4.0f;

#define FLY_SPEED_MIN       0.5f
#define FLY_SPEED_MAX      40.0f
#define FLY_SPEED_WHEEL_MULT 1.15f
#define MOUSE_LOOK_SENSITIVITY 0.0028f
#define SHIFT_BOOST_MULTIPLIER 3.0f

typedef struct { float x, y, z; } vp3d_vec3_t;

static vp3d_vec3_t vp3d_sub(vp3d_vec3_t a, vp3d_vec3_t b) {
    vp3d_vec3_t r = { a.x - b.x, a.y - b.y, a.z - b.z };
    return r;
}

static vp3d_vec3_t vp3d_cross(vp3d_vec3_t a, vp3d_vec3_t b) {
    vp3d_vec3_t r = {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
    return r;
}

static float vp3d_dot(vp3d_vec3_t a, vp3d_vec3_t b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

static vp3d_vec3_t vp3d_normalize(vp3d_vec3_t a) {
    float len = sqrtf(vp3d_dot(a, a));
    if (len < 0.00001f) {
        vp3d_vec3_t zero = { 0.0f, 0.0f, 0.0f };
        return zero;
    }
    vp3d_vec3_t r = { a.x / len, a.y / len, a.z / len };
    return r;
}

static vp3d_vec3_t vp3d_forward_from_angles(float yaw, float pitch) {
    vp3d_vec3_t f = {
        cosf(pitch) * sinf(yaw),
        sinf(pitch),
        cosf(pitch) * cosf(yaw)
    };
    return f;
}

static void ensure_camera_initialized(void) {
    if (g_cam_ready) return;
    g_cam_ready = 1;

    vp3d_vec3_t eye    = { g_cam_pos_x, g_cam_pos_y, g_cam_pos_z };
    vp3d_vec3_t target = { 0.0f, 0.0f, 0.0f };
    vp3d_vec3_t fwd = vp3d_normalize(vp3d_sub(target, eye));

    g_cam_pitch = asinf(fwd.y);
    g_cam_yaw   = atan2f(fwd.x, fwd.z);
}

typedef struct {
    vp3d_vec3_t eye;
    vp3d_vec3_t xaxis, yaxis, zaxis;
} vp3d_camera_basis_t;

static vp3d_camera_basis_t vp3d_build_camera_basis(void) {
    ensure_camera_initialized();

    vp3d_camera_basis_t cam;
    vp3d_vec3_t eye = { g_cam_pos_x, g_cam_pos_y, g_cam_pos_z };
    vp3d_vec3_t forward = vp3d_forward_from_angles(g_cam_yaw, g_cam_pitch);
    vp3d_vec3_t world_up = { 0.0f, 1.0f, 0.0f };

    cam.eye = eye;
    cam.zaxis = vp3d_normalize((vp3d_vec3_t){ -forward.x, -forward.y, -forward.z });
    cam.xaxis = vp3d_normalize(vp3d_cross(world_up, cam.zaxis));
    cam.yaxis = vp3d_cross(cam.zaxis, cam.xaxis);
    return cam;
}

static void vp3d_camera_update(int x, int y, int w, int h) {
    ensure_camera_initialized();

    int mx = window_mouse_x();
    int my = window_mouse_y();
    float dt = window_get_delta_time();

    if (!g_fly_active) {
        int inside = (mx >= x && mx < x + w && my >= y && my < y + h);
        if (inside && window_mouse_right_just_pressed()) {
            g_fly_active = 1;
            window_set_relative_mouse_mode(1);
        }
    } else if (!window_mouse_right_down()) {
        g_fly_active = 0;
        window_set_relative_mouse_mode(0);
    }

    if (g_fly_active) {
        float dx = (float)window_mouse_delta_x();
        float dy = (float)window_mouse_delta_y();

        g_cam_yaw   += dx * MOUSE_LOOK_SENSITIVITY;
        g_cam_pitch -= dy * MOUSE_LOOK_SENSITIVITY;
        if (g_cam_pitch >  PITCH_LIMIT) g_cam_pitch =  PITCH_LIMIT;
        if (g_cam_pitch < -PITCH_LIMIT) g_cam_pitch = -PITCH_LIMIT;

        int wheel = window_mouse_wheel_delta();
        if (wheel > 0) {
            g_fly_speed *= FLY_SPEED_WHEEL_MULT;
        } else if (wheel < 0) {
            g_fly_speed /= FLY_SPEED_WHEEL_MULT;
        }
        if (g_fly_speed < FLY_SPEED_MIN) g_fly_speed = FLY_SPEED_MIN;
        if (g_fly_speed > FLY_SPEED_MAX) g_fly_speed = FLY_SPEED_MAX;

        vp3d_vec3_t forward = vp3d_forward_from_angles(g_cam_yaw, g_cam_pitch);
        vp3d_vec3_t world_up = { 0.0f, 1.0f, 0.0f };
        vp3d_vec3_t right = vp3d_normalize(vp3d_cross(forward, world_up));

        float speed = g_fly_speed * dt * (window_key_down_shift() ? SHIFT_BOOST_MULTIPLIER : 1.0f);

        if (window_key_down_w()) {
            g_cam_pos_x += forward.x * speed; g_cam_pos_y += forward.y * speed; g_cam_pos_z += forward.z * speed;
        }
        if (window_key_down_s()) {
            g_cam_pos_x -= forward.x * speed; g_cam_pos_y -= forward.y * speed; g_cam_pos_z -= forward.z * speed;
        }
        if (window_key_down_d()) {
            g_cam_pos_x += right.x * speed; g_cam_pos_y += right.y * speed; g_cam_pos_z += right.z * speed;
        }
        if (window_key_down_a()) {
            g_cam_pos_x -= right.x * speed; g_cam_pos_y -= right.y * speed; g_cam_pos_z -= right.z * speed;
        }
        if (window_key_down_e()) {
            g_cam_pos_y += speed;
        }
        if (window_key_down_q()) {
            g_cam_pos_y -= speed;
        }
    }

    if (window_key_just_pressed_f()) {
        vp3d_vec3_t to_origin = vp3d_normalize((vp3d_vec3_t){ -g_cam_pos_x, -g_cam_pos_y, -g_cam_pos_z });
        g_cam_pitch = asinf(to_origin.y);
        g_cam_yaw   = atan2f(to_origin.x, to_origin.z);
    }
}

void viewport_3d_update(int x, int y, int w, int h) {
    if (w <= 0 || h <= 0) return;
    vp3d_camera_update(x, y, w, h);
}

static vp3d_vec3_t vp3d_world_to_view(vp3d_vec3_t world_point, const vp3d_camera_basis_t *cam) {
    vp3d_vec3_t d = vp3d_sub(world_point, cam->eye);
    vp3d_vec3_t v = {
        vp3d_dot(d, cam->xaxis),
        vp3d_dot(d, cam->yaxis),
        vp3d_dot(d, cam->zaxis)
    };
    return v;
}

static void vp3d_view_to_screen(vp3d_vec3_t v, int cam_x, int cam_y, int cam_w, int cam_h,
                                 float *out_sx, float *out_sy) {
    float aspect   = (float)cam_w / (float)cam_h;
    float fov_rad  = CAM_FOV_DEG * (VP3D_PI / 180.0f);
    float focal    = 1.0f / tanf(fov_rad * 0.5f);
    float persp    = focal / (-v.z);

    float ndc_x = (v.x * persp) / aspect;
    float ndc_y = (v.y * persp);

    *out_sx = (float)cam_x + (float)cam_w * 0.5f + ndc_x * (float)cam_w * 0.5f;
    *out_sy = (float)cam_y + (float)cam_h * 0.5f - ndc_y * (float)cam_h * 0.5f;
}

static int vp3d_clip_near(vp3d_vec3_t *a, vp3d_vec3_t *b) {
    int a_behind = (a->z > -CAM_NEAR);
    int b_behind = (b->z > -CAM_NEAR);

    if (a_behind && b_behind) return 0;
    if (!a_behind && !b_behind) return 1;

    float t = (-CAM_NEAR - a->z) / (b->z - a->z);
    vp3d_vec3_t clipped = {
        a->x + (b->x - a->x) * t,
        a->y + (b->y - a->y) * t,
        -CAM_NEAR
    };
    if (a_behind) *a = clipped; else *b = clipped;
    return 1;
}

#define LINE_TEX_SIZE  4

typedef struct {
    window_texture_t *tex;
    int ready;
} vp3d_line_tex_t;

static vp3d_line_tex_t g_tex_grid = {0};
static vp3d_line_tex_t g_tex_axis_x = {0};
static vp3d_line_tex_t g_tex_axis_z = {0};

static void vp3d_ensure_line_tex(vp3d_line_tex_t *slot, unsigned char r, unsigned char g, unsigned char b) {
    if (slot->ready) return;
    slot->ready = 1;

    unsigned char pixels[LINE_TEX_SIZE * LINE_TEX_SIZE * 4];
    for (int i = 0; i < LINE_TEX_SIZE * LINE_TEX_SIZE; i++) {
        pixels[i * 4 + 0] = r;
        pixels[i * 4 + 1] = g;
        pixels[i * 4 + 2] = b;
        pixels[i * 4 + 3] = 0xFF;
    }
    slot->tex = window_create_texture(pixels, LINE_TEX_SIZE, LINE_TEX_SIZE);
}

static void vp3d_draw_screen_line(vp3d_line_tex_t *slot, float x0, float y0, float x1, float y1,
                                   int thickness) {
    if (slot->tex == NULL) return;

    float dx = x1 - x0;
    float dy = y1 - y0;
    float length = sqrtf(dx * dx + dy * dy);
    if (length < 0.5f) return;

    float angle_deg = atan2f(dy, dx) * (180.0f / VP3D_PI);

    float cx = (x0 + x1) * 0.5f;
    float cy = (y0 + y1) * 0.5f;

    int dst_w = (int)(length + 0.5f);
    int dst_h = thickness;
    if (dst_w < 1) dst_w = 1;
    int dst_x = (int)(cx - (float)dst_w * 0.5f);
    int dst_y = (int)(cy - (float)dst_h * 0.5f);

    window_draw_texture_region_rotated(slot->tex, 0, 0, LINE_TEX_SIZE, LINE_TEX_SIZE,
                                        dst_x, dst_y, dst_w, dst_h, (double)angle_deg);
}

static void vp3d_draw_world_line(const vp3d_camera_basis_t *cam,
                                  vp3d_vec3_t world_a, vp3d_vec3_t world_b,
                                  int cam_x, int cam_y, int cam_w, int cam_h,
                                  vp3d_line_tex_t *slot, int thickness) {
    vp3d_vec3_t va = vp3d_world_to_view(world_a, cam);
    vp3d_vec3_t vb = vp3d_world_to_view(world_b, cam);

    if (!vp3d_clip_near(&va, &vb)) return;

    float sx0, sy0, sx1, sy1;
    vp3d_view_to_screen(va, cam_x, cam_y, cam_w, cam_h, &sx0, &sy0);
    vp3d_view_to_screen(vb, cam_x, cam_y, cam_w, cam_h, &sx1, &sy1);

    vp3d_draw_screen_line(slot, sx0, sy0, sx1, sy1, thickness);
}

void viewport_3d_draw(int x, int y, int w, int h) {
    if (w <= 0 || h <= 0) return;

    vp3d_ensure_line_tex(&g_tex_grid, GRID_R, GRID_G, GRID_B);
    vp3d_ensure_line_tex(&g_tex_axis_x, AXIS_X_R, AXIS_X_G, AXIS_X_B);
    vp3d_ensure_line_tex(&g_tex_axis_z, AXIS_Z_R, AXIS_Z_G, AXIS_Z_B);

    window_set_clip_rect(x, y, w, h);
    window_fill_rect(x, y, w, h, BG_R, BG_G, BG_B);

    vp3d_camera_basis_t cam = vp3d_build_camera_basis();

    float extent = (float)GRID_HALF_EXTENT * GRID_STEP;

    for (int k = -GRID_HALF_EXTENT; k <= GRID_HALF_EXTENT; k++) {
        if (k == 0) continue;
        float kf = (float)k * GRID_STEP;

        vp3d_vec3_t a1 = { kf, 0.0f, -extent };
        vp3d_vec3_t b1 = { kf, 0.0f,  extent };
        vp3d_draw_world_line(&cam, a1, b1, x, y, w, h, &g_tex_grid, GRID_LINE_THICKNESS);

        vp3d_vec3_t a2 = { -extent, 0.0f, kf };
        vp3d_vec3_t b2 = {  extent, 0.0f, kf };
        vp3d_draw_world_line(&cam, a2, b2, x, y, w, h, &g_tex_grid, GRID_LINE_THICKNESS);
    }

    {
        vp3d_vec3_t xa = { -extent, 0.0f, 0.0f };
        vp3d_vec3_t xb = {  extent, 0.0f, 0.0f };
        vp3d_draw_world_line(&cam, xa, xb, x, y, w, h, &g_tex_axis_x, AXIS_LINE_THICKNESS);

        vp3d_vec3_t za = { 0.0f, 0.0f, -extent };
        vp3d_vec3_t zb = { 0.0f, 0.0f,  extent };
        vp3d_draw_world_line(&cam, za, zb, x, y, w, h, &g_tex_axis_z, AXIS_LINE_THICKNESS);
    }

    {
        int selected = scene_data_get_selected_index();
        int node_count = scene_data_get_node_count();
        for (int i = 0; i < node_count; i++) {
            node_type_t type = scene_data_get_type(i);
            const node_editor_registry_entry_t *ed = node_editor_registry_get(type);
            if (ed == NULL || ed->draw_3d == NULL) continue;

            const node_registry_entry_t *schema = node_registry_get(type);
            node_property_value_t *values = scene_data_get_values(i);
            if (schema == NULL || values == NULL) continue;

            ed->draw_3d(values, schema->property_count, x, y, w, h, (i == selected));
        }
    }

    window_clear_clip_rect();
}

int viewport_3d_project(float world_x, float world_y, float world_z,
                         int cam_x, int cam_y, int cam_w, int cam_h,
                         int *out_screen_x, int *out_screen_y) {
    vp3d_camera_basis_t cam = vp3d_build_camera_basis();
    vp3d_vec3_t world_point = { world_x, world_y, world_z };
    vp3d_vec3_t v = vp3d_world_to_view(world_point, &cam);

    if (v.z > -CAM_NEAR) {
        return 0;
    }

    float sx, sy;
    vp3d_view_to_screen(v, cam_x, cam_y, cam_w, cam_h, &sx, &sy);
    *out_screen_x = (int)(sx + 0.5f);
    *out_screen_y = (int)(sy + 0.5f);
    return 1;
}
