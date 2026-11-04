/*
 * ============================================================
 * viewport_3d.c
 * ============================================================
 * A real perspective camera built manually without an external math library:
 * a lookAt basis + an actual perspective projection (no orthographic trick
 * or depthless parallel lines). window.h has no generic line drawing function -
 * so any arbitrarily angled line (almost all perspective grid lines) is drawn as
 * a thin solid rectangle with a constant color rotated around its center by the
 * exact line angle (window_draw_texture_region_rotated) - the same technique
 * originally used for the tree open/close arrow, but here with a dynamically
 * computed length each frame.
 * ============================================================
 */

#include <math.h>
#include <stddef.h>

#include "viewport_3d.h"
#include "window.h"
#include "scene_data.h"
#include "node_editor_registry.h"

/* ------------------------------------------------------------
 * Colors for the 3D development world - the exact same philosophy as 2D
 * (neutral dark background, light gray grid, X axis red and Z green intersecting
 * at the origin) - Y is the vertical up axis here, so it does not have its own ground line
 * ------------------------------------------------------------ */
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

/* ------------------------------------------------------------
 * Grid and camera constants - in abstract world units (logical meters)
 * ------------------------------------------------------------ */
#define GRID_HALF_EXTENT   12    /* Number of grid lines on each side of the center */
#define GRID_STEP          1.0f  /* Distance in world units between consecutive lines */

#define CAM_FOV_DEG   60.0f
#define CAM_NEAR       0.15f

#define GRID_LINE_THICKNESS  1
#define AXIS_LINE_THICKNESS  2

#define VP3D_PI  3.14159265358979323846f
#define PITCH_LIMIT  (89.0f * VP3D_PI / 180.0f) /* Prevents camera flipping when looking straight up/down */

/* Default camera position - exactly the same old angle (an oblique perspective
 * that sees the ground and all three axes, framing the world origin) - but
 * now only a starting point for a free-flying camera (position + Yaw/Pitch), not
 * fixed values. ensure_camera_initialized computes the initial Yaw/Pitch from
 * this position/target once on first use */
static float g_cam_pos_x    = 9.0f;
static float g_cam_pos_y    = 7.0f;
static float g_cam_pos_z    = 9.0f;
static float g_cam_yaw      = 0.0f; /* Radians - around the Y axis (up) */
static float g_cam_pitch    = 0.0f; /* Radians - up/down, clamped by PITCH_LIMIT */
static int   g_cam_ready    = 0;

/* Flight interaction state - enabled by holding the right mouse button started
 * from inside the camera area (same idea as dragging a splitter) */
static int   g_fly_active   = 0;
static float g_fly_speed    = 4.0f; /* World units per second - adjusted with the mouse wheel while flying */

#define FLY_SPEED_MIN       0.5f
#define FLY_SPEED_MAX      40.0f
#define FLY_SPEED_WHEEL_MULT 1.15f
#define MOUSE_LOOK_SENSITIVITY 0.0028f /* Radians per pixel of relative movement */
#define SHIFT_BOOST_MULTIPLIER 3.0f

/* ------------------------------------------------------------
 * Simple 3D vector - local operations only, no general math header
 * (the engine hasn't built a shared vector library yet - this is specific to this file)
 * ------------------------------------------------------------ */
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

/* Computes the forward view vector from the current Yaw/Pitch - Yaw
 * around the global Y axis (0 = points +Z), Pitch up/down. Same formula
 * used by flight cameras in game engines */
static vp3d_vec3_t vp3d_forward_from_angles(float yaw, float pitch) {
    vp3d_vec3_t f = {
        cosf(pitch) * sinf(yaw),
        sinf(pitch),
        cosf(pitch) * cosf(yaw)
    };
    return f;
}

/* Initializes the initial Yaw/Pitch only once - from the exact same position/target
 * of the old fixed camera ((9,7,9) looking at the world origin) -
 * so the first appearance when opening the engine matches the old one exactly,
 * and differences only appear when the developer actually moves the camera */
static void ensure_camera_initialized(void) {
    if (g_cam_ready) return;
    g_cam_ready = 1;

    vp3d_vec3_t eye    = { g_cam_pos_x, g_cam_pos_y, g_cam_pos_z };
    vp3d_vec3_t target = { 0.0f, 0.0f, 0.0f };
    vp3d_vec3_t fwd = vp3d_normalize(vp3d_sub(target, eye));

    g_cam_pitch = asinf(fwd.y);
    g_cam_yaw   = atan2f(fwd.x, fwd.z);
}

/* ------------------------------------------------------------
 * Camera lookAt basis - computed from the free camera position/angles
 * each frame (cheap, no need to store) - zaxis points backward (opposite
 * viewing direction), xaxis to the right, yaxis up in the camera's local space
 * ------------------------------------------------------------ */
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
    cam.zaxis = vp3d_normalize((vp3d_vec3_t){ -forward.x, -forward.y, -forward.z }); /* backward */
    cam.xaxis = vp3d_normalize(vp3d_cross(world_up, cam.zaxis)); /* right */
    cam.yaxis = vp3d_cross(cam.zaxis, cam.xaxis);             /* up locally (unit length) */
    return cam;
}

/* Updates the fly camera this frame - called from viewport_3d_update
 * (exported in the header), separated here just to stay close to the rest
 * of the camera logic in the file */
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
        g_cam_pitch -= dy * MOUSE_LOOK_SENSITIVITY; /* Screen Y is down-positive -> moving the mouse down looks down */
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
        /* Initial framing: points the view at the world origin from the current position.
         * It will later switch to framing the actually selected node in the scene tree
         * when a node transform system becomes available (a separate upcoming step, not ready
         * yet) - currently there is no node transform to read, so the world origin is the most suitable default */
        vp3d_vec3_t to_origin = vp3d_normalize((vp3d_vec3_t){ -g_cam_pos_x, -g_cam_pos_y, -g_cam_pos_z });
        g_cam_pitch = asinf(to_origin.y);
        g_cam_yaw   = atan2f(to_origin.x, to_origin.z);
    }
}

void viewport_3d_update(int x, int y, int w, int h) {
    if (w <= 0 || h <= 0) return;
    vp3d_camera_update(x, y, w, h);
}

/* Transforms a world point to camera (view) space - the Z axis in this
 * space is negative in front of the camera (same convention as OpenGL), positive behind it */
static vp3d_vec3_t vp3d_world_to_view(vp3d_vec3_t world_point, const vp3d_camera_basis_t *cam) {
    vp3d_vec3_t d = vp3d_sub(world_point, cam->eye);
    vp3d_vec3_t v = {
        vp3d_dot(d, cam->xaxis),
        vp3d_dot(d, cam->yaxis),
        vp3d_dot(d, cam->zaxis)
    };
    return v;
}

/* Projects a camera-space point (must actually be in front -
 * z <= -CAM_NEAR) to floating screen coordinates inside the camera rectangle */
static void vp3d_view_to_screen(vp3d_vec3_t v, int cam_x, int cam_y, int cam_w, int cam_h,
                                 float *out_sx, float *out_sy) {
    float aspect   = (float)cam_w / (float)cam_h;
    float fov_rad  = CAM_FOV_DEG * (VP3D_PI / 180.0f);
    float focal    = 1.0f / tanf(fov_rad * 0.5f);   /* Focal length - controls the field of view */
    float persp    = focal / (-v.z);                /* Perspective divide - farther = smaller */

    float ndc_x = (v.x * persp) / aspect;
    float ndc_y = (v.y * persp);

    *out_sx = (float)cam_x + (float)cam_w * 0.5f + ndc_x * (float)cam_w * 0.5f;
    /* World Y is up, screen Y is down -> flip the sign */
    *out_sy = (float)cam_y + (float)cam_h * 0.5f - ndc_y * (float)cam_h * 0.5f;
}

/* Clips a line (in camera space) against the near plane if one
 * of its endpoints is behind it - returns 1 if any part of the line is actually
 * visible after clipping, 0 if the entire line is behind the camera (completely discarded) */
static int vp3d_clip_near(vp3d_vec3_t *a, vp3d_vec3_t *b) {
    int a_behind = (a->z > -CAM_NEAR);
    int b_behind = (b->z > -CAM_NEAR);

    if (a_behind && b_behind) return 0; /* entire line behind the camera */
    if (!a_behind && !b_behind) return 1; /* entirely in front - no clipping */

    /* exactly one endpoint behind the near plane - clip it precisely */
    float t = (-CAM_NEAR - a->z) / (b->z - a->z);
    vp3d_vec3_t clipped = {
        a->x + (b->x - a->x) * t,
        a->y + (b->y - a->y) * t,
        -CAM_NEAR
    };
    if (a_behind) *a = clipped; else *b = clipped;
    return 1;
}

/* ------------------------------------------------------------
 * Solid-colored rotated lines - a 4x4 single-color texture for each required color,
 * built only once (like tab_chrome_t pattern in editor_workspace.c)
 * then reused every frame without recreating
 * ------------------------------------------------------------ */
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

/* Draws a true straight line at an arbitrary angle between two screen points, as a thin
 * filled rectangle rotated about its center - thickness in pixels */
static void vp3d_draw_screen_line(vp3d_line_tex_t *slot, float x0, float y0, float x1, float y1,
                                   int thickness) {
    if (slot->tex == NULL) return;

    float dx = x1 - x0;
    float dy = y1 - y0;
    float length = sqrtf(dx * dx + dy * dy);
    if (length < 0.5f) return; /* zero-length line - ignore */

    /* Directional angle of the line on screen (Y down) clockwise from the positive X axis
     * - exactly the same convention as window_draw_texture_region_rotated */
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

/* Clips and projects a single world line (from a to b) and actually draws it if any part
 * of it is in front of the camera - the common function used by all grid and axis lines */
static void vp3d_draw_world_line(const vp3d_camera_basis_t *cam,
                                  vp3d_vec3_t world_a, vp3d_vec3_t world_b,
                                  int cam_x, int cam_y, int cam_w, int cam_h,
                                  vp3d_line_tex_t *slot, int thickness) {
    vp3d_vec3_t va = vp3d_world_to_view(world_a, cam);
    vp3d_vec3_t vb = vp3d_world_to_view(world_b, cam);

    if (!vp3d_clip_near(&va, &vb)) return; /* completely behind the camera */

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

    /* Ground grid at height zero (X/Z plane) - a line for each integer
     * value k, perfectly aligned with the world origin. k == 0 is skipped here because
     * it is drawn later as colored axes (red/green) instead of plain gray */
    for (int k = -GRID_HALF_EXTENT; k <= GRID_HALF_EXTENT; k++) {
        if (k == 0) continue;
        float kf = (float)k * GRID_STEP;

        /* Line parallel to Z axis, at X = kf */
        vp3d_vec3_t a1 = { kf, 0.0f, -extent };
        vp3d_vec3_t b1 = { kf, 0.0f,  extent };
        vp3d_draw_world_line(&cam, a1, b1, x, y, w, h, &g_tex_grid, GRID_LINE_THICKNESS);

        /* Line parallel to X axis, at Z = kf */
        vp3d_vec3_t a2 = { -extent, 0.0f, kf };
        vp3d_vec3_t b2 = {  extent, 0.0f, kf };
        vp3d_draw_world_line(&cam, a2, b2, x, y, w, h, &g_tex_grid, GRID_LINE_THICKNESS);
    }

    /* X (red) and Z (green) axes - intersect exactly at the world origin
     * (0,0,0), more prominent than the grid lines (thicker) */
    {
        vp3d_vec3_t xa = { -extent, 0.0f, 0.0f };
        vp3d_vec3_t xb = {  extent, 0.0f, 0.0f };
        vp3d_draw_world_line(&cam, xa, xb, x, y, w, h, &g_tex_axis_x, AXIS_LINE_THICKNESS);

        vp3d_vec3_t za = { 0.0f, 0.0f, -extent };
        vp3d_vec3_t zb = { 0.0f, 0.0f,  extent };
        vp3d_draw_world_line(&cam, za, zb, x, y, w, h, &g_tex_axis_z, AXIS_LINE_THICKNESS);
    }

    /* Scene nodes - each node has a registered 3D visual (node_editor_registry)
     * drawn at its actual position (if in front of the camera); those without a visual
     * (draw_3d == NULL, like an abstract Element or any 2D-only node) are safely ignored
     * with no special checks here */
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
        return 0; /* behind the camera - not drawable */
    }

    float sx, sy;
    vp3d_view_to_screen(v, cam_x, cam_y, cam_w, cam_h, &sx, &sy);
    *out_screen_x = (int)(sx + 0.5f);
    *out_screen_y = (int)(sy + 0.5f);
    return 1;
}
