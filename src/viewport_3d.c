/*
 * ============================================================
 * viewport_3d.c
 * ============================================================
 * كاميرا منظور حقيقية (Perspective) مبنية يدوياً بلا مكتبة رياضيات
 * خارجية: قاعدة نظر (lookAt) + إسقاط منظور فعلي (لا تخطيط وهمي
 * ولا خطوط متوازية بلا عمق). window.h ما فيه دالة رسم خط عامة -
 * فأي خط بزاوية اعتباطية (كل خطوط شبكة المنظور تقريباً) يُرسم
 * كمستطيل صلب رفيع بلون ثابت مُدوَّر حول مركزه بزاوية الخط بالضبط
 * (window_draw_texture_region_rotated) - نفس الأسلوب المستخدم أصلاً
 * لسهم فتح/إغلاق الشجرة، بس هنا بطول ديناميكي محسوب كل إطار.
 * ============================================================
 */

#include <math.h>
#include <stddef.h>

#include "viewport_3d.h"
#include "window.h"
#include "scene_tree_panel.h"
#include "node_editor_registry.h"

/* ------------------------------------------------------------
 * ألوان عالم التطوير 3D - نفس فلسفة عالم 2D بالضبط (خلفية داكنة
 * محايدة، شبكة رمادية خفيفة، محورا X أحمر وZ أخضر يتقاطعان عند
 * الأصل) - Y هو الارتفاع هنا (لأعلى)، فمو له خط أرضي خاص به
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
 * ثوابت الشبكة والكاميرا - بوحدات عالم مجردة (متر منطقي)
 * ------------------------------------------------------------ */
#define GRID_HALF_EXTENT   12    /* عدد خطوط الشبكة كل جهة عن المركز */
#define GRID_STEP          1.0f  /* المسافة بوحدات عالم بين كل خط والي يليه */

#define CAM_FOV_DEG   60.0f
#define CAM_NEAR       0.15f

#define GRID_LINE_THICKNESS  1
#define AXIS_LINE_THICKNESS  2

#define VP3D_PI  3.14159265358979323846f
#define PITCH_LIMIT  (89.0f * VP3D_PI / 180.0f) /* يمنع انقلاب الكاميرا عند النظر الرأسي التام */

/* موضع الكاميرا الافتراضي - نفس الزاوية القديمة تماماً (منظور مائل
 * يشوف الأرضية والمحاور الثلاثة معاً، يصوّر نقطة أصل العالم) - لكن
 * الآن نقطة بداية بس لكاميرا طيران حرة (موضع + Yaw/Pitch)، مو
 * قيماً ثابتة أبداً. ensure_camera_initialized تحسب Yaw/Pitch
 * الابتدائيين من نفس هذا الموضع/الهدف مرة وحدة بس عند أول استخدام */
static float g_cam_pos_x    = 9.0f;
static float g_cam_pos_y    = 7.0f;
static float g_cam_pos_z    = 9.0f;
static float g_cam_yaw      = 0.0f; /* راديان - حول محور Y (لأعلى) */
static float g_cam_pitch    = 0.0f; /* راديان - أعلى/أسفل، محدود بـPITCH_LIMIT */
static int   g_cam_ready    = 0;

/* حالة تفاعل الطيران - يُفعَّل بضغط مستمر على الزر الأيمن يبدأ من
 * داخل منطقة الكاميرا فعلياً (نفس فكرة سحب حد فاصل) */
static int   g_fly_active   = 0;
static float g_fly_speed    = 4.0f; /* وحدات عالم/ثانية - تتعدّل بعجلة الماوس أثناء الطيران */

#define FLY_SPEED_MIN       0.5f
#define FLY_SPEED_MAX      40.0f
#define FLY_SPEED_WHEEL_MULT 1.15f
#define MOUSE_LOOK_SENSITIVITY 0.0028f /* راديان لكل بكسل حركة نسبية */
#define SHIFT_BOOST_MULTIPLIER 3.0f

/* ------------------------------------------------------------
 * متجه ثلاثي أبعاد بسيط - عمليات محلية فقط، بلا هيدر رياضيات عام
 * (المحرك ما بنى مكتبة متجهات مشتركة بعد - هذا خاص بهذا الملف)
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

/* يحسب متجه النظر الأمامي (Forward) من Yaw/Pitch الحاليين - Yaw
 * حول محور Y العالمي (0 = يشاور +Z)، Pitch أعلى/أسفل. نفس الصيغة
 * القياسية بكاميرات الطيران بمحركات الألعاب */
static vp3d_vec3_t vp3d_forward_from_angles(float yaw, float pitch) {
    vp3d_vec3_t f = {
        cosf(pitch) * sinf(yaw),
        sinf(pitch),
        cosf(pitch) * cosf(yaw)
    };
    return f;
}

/* يهيّئ Yaw/Pitch الابتدائيين مرة وحدة بس - من نفس موضع/هدف
 * الكاميرا الثابتة القديمة بالضبط (9,7,9) تنظر لأصل العالم -
 * فالمظهر الأول عند فتح المحرك يطابق القديم تماماً، والفرق يبدأ
 * بس لما المطور يحرّك الكاميرا فعلياً */
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
 * قاعدة نظر الكاميرا (lookAt) - تُحسب من موضع/زوايا الكاميرا الحرة
 * كل إطار (رخيصة، بلا داعٍ لتخزينها) - zaxis يشاور للخلف (عكس
 * اتجاه النظر)، xaxis لليمين، yaxis لأعلى محلياً بالكاميرا
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
    cam.zaxis = vp3d_normalize((vp3d_vec3_t){ -forward.x, -forward.y, -forward.z }); /* للخلف */
    cam.xaxis = vp3d_normalize(vp3d_cross(world_up, cam.zaxis)); /* لليمين */
    cam.yaxis = vp3d_cross(cam.zaxis, cam.xaxis);             /* لأعلى محلياً (وحدة أصلاً) */
    return cam;
}

/* يحدّث كاميرا الطيران هذا الإطار - يُستدعى من viewport_3d_update
 * (المُصدَّرة بالهيدر)، منفصلة هنا فقط عشان تبقى قريبة من باقي
 * منطق الكاميرا بالملف */
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
        g_cam_pitch -= dy * MOUSE_LOOK_SENSITIVITY; /* الشاشة لأسفل موجبة -> تحريك الماوس لأسفل ينظر لأسفل */
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
        /* تأطير مبدئي: يوجّه النظر نحو أصل العالم من الموضع الحالي.
         * سينتقل لاحقاً لتأطير العقدة المحددة فعلياً بشجرة المشهد
         * لما يتوفر نظام مواضع العقد (خطوة قادمة منفصلة، غير جاهزة
         * بعد) - حالياً بلا موضع عقدة نقرأه، فأصل العالم أنسب افتراضي */
        vp3d_vec3_t to_origin = vp3d_normalize((vp3d_vec3_t){ -g_cam_pos_x, -g_cam_pos_y, -g_cam_pos_z });
        g_cam_pitch = asinf(to_origin.y);
        g_cam_yaw   = atan2f(to_origin.x, to_origin.z);
    }
}

void viewport_3d_update(int x, int y, int w, int h) {
    if (w <= 0 || h <= 0) return;
    vp3d_camera_update(x, y, w, h);
}

/* يحوّل نقطة عالمية لفضاء الكاميرا (view space) - المحور Z بهذا
 * الفضاء سالب أمام الكاميرا (نفس اصطلاح OpenGL)، موجب خلفها */
static vp3d_vec3_t vp3d_world_to_view(vp3d_vec3_t world_point, const vp3d_camera_basis_t *cam) {
    vp3d_vec3_t d = vp3d_sub(world_point, cam->eye);
    vp3d_vec3_t v = {
        vp3d_dot(d, cam->xaxis),
        vp3d_dot(d, cam->yaxis),
        vp3d_dot(d, cam->zaxis)
    };
    return v;
}

/* يسقط نقطة فضاء الكاميرا (view space، لازم تكون أمامها فعلاً -
 * z <= -CAM_NEAR) لإحداثيات شاشة عائمة داخل مستطيل الكاميرا */
static void vp3d_view_to_screen(vp3d_vec3_t v, int cam_x, int cam_y, int cam_w, int cam_h,
                                 float *out_sx, float *out_sy) {
    float aspect   = (float)cam_w / (float)cam_h;
    float fov_rad  = CAM_FOV_DEG * (VP3D_PI / 180.0f);
    float focal    = 1.0f / tanf(fov_rad * 0.5f);   /* بُعد بؤري - يتحكم بحقل الرؤية */
    float persp    = focal / (-v.z);                /* قسمة المنظور - أبعد = أصغر */

    float ndc_x = (v.x * persp) / aspect;
    float ndc_y = (v.y * persp);

    *out_sx = (float)cam_x + (float)cam_w * 0.5f + ndc_x * (float)cam_w * 0.5f;
    /* محور Y بالعالم لأعلى، بالشاشة لأسفل -> نعكس الإشارة */
    *out_sy = (float)cam_y + (float)cam_h * 0.5f - ndc_y * (float)cam_h * 0.5f;
}

/* يقص خط (بفضاء الكاميرا) على مستوى القرب (near plane) لو أحد
 * طرفيه خلفها - يرجع 1 لو أي جزء من الخط ظاهر فعلاً بعد القص،
 * 0 لو الخط بالكامل خلف الكاميرا (يُهمل تماماً) */
static int vp3d_clip_near(vp3d_vec3_t *a, vp3d_vec3_t *b) {
    int a_behind = (a->z > -CAM_NEAR);
    int b_behind = (b->z > -CAM_NEAR);

    if (a_behind && b_behind) return 0; /* الخط بالكامل خلف الكاميرا */
    if (!a_behind && !b_behind) return 1; /* بالكامل أمامها - بلا قص */

    /* طرف واحد بس خلف مستوى القرب - نقص عنده بالضبط */
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
 * خطوط بلون صلب مُدوَّرة - نسيج 4x4 بلون ثابت لكل لون مطلوب،
 * يُبنى مرة واحدة فقط (مثل نمط tab_chrome_t بـeditor_workspace.c)
 * ثم يُعاد استخدامه كل إطار بلا إعادة إنشاء
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

/* يرسم خط مستقيم حقيقي بزاوية اعتباطية بين نقطتي شاشة، كمستطيل
 * صلب رفيع مُدوَّر حول مركزه - thickness بالبكسل */
static void vp3d_draw_screen_line(vp3d_line_tex_t *slot, float x0, float y0, float x1, float y1,
                                   int thickness) {
    if (slot->tex == NULL) return;

    float dx = x1 - x0;
    float dy = y1 - y0;
    float length = sqrtf(dx * dx + dy * dy);
    if (length < 0.5f) return; /* خط بلا طول محسوس - نتجاهله */

    /* الزاوية الاتجاهية للخط بالشاشة (Y لأسفل) بالساعة من محور X
     * الموجب - نفس اصطلاح window_draw_texture_region_rotated بالضبط */
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

/* يقص ويسقط خط عالمي واحد (من a لـb) ويرسمه فعلياً لو ظهر أي جزء
 * منه أمام الكاميرا - الدالة المشتركة اللي كل خطوط الشبكة والمحاور
 * تمر عليها */
static void vp3d_draw_world_line(const vp3d_camera_basis_t *cam,
                                  vp3d_vec3_t world_a, vp3d_vec3_t world_b,
                                  int cam_x, int cam_y, int cam_w, int cam_h,
                                  vp3d_line_tex_t *slot, int thickness) {
    vp3d_vec3_t va = vp3d_world_to_view(world_a, cam);
    vp3d_vec3_t vb = vp3d_world_to_view(world_b, cam);

    if (!vp3d_clip_near(&va, &vb)) return; /* بالكامل خلف الكاميرا */

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

    /* شبكة الأرضية عند الارتفاع صفر (مستوى X/Z) - خط لكل قيمة صحيحة
     * من k، بمحاذاة تامة على نقطة أصل العالم. k == 0 يُهمل هنا لأنه
     * يُرسم لاحقاً كمحور ملوَّن (أحمر/أخضر) بدل رمادي عادي */
    for (int k = -GRID_HALF_EXTENT; k <= GRID_HALF_EXTENT; k++) {
        if (k == 0) continue;
        float kf = (float)k * GRID_STEP;

        /* خط موازٍ لمحور Z، عند X = kf */
        vp3d_vec3_t a1 = { kf, 0.0f, -extent };
        vp3d_vec3_t b1 = { kf, 0.0f,  extent };
        vp3d_draw_world_line(&cam, a1, b1, x, y, w, h, &g_tex_grid, GRID_LINE_THICKNESS);

        /* خط موازٍ لمحور X، عند Z = kf */
        vp3d_vec3_t a2 = { -extent, 0.0f, kf };
        vp3d_vec3_t b2 = {  extent, 0.0f, kf };
        vp3d_draw_world_line(&cam, a2, b2, x, y, w, h, &g_tex_grid, GRID_LINE_THICKNESS);
    }

    /* محورا X (أحمر) وZ (أخضر) - يتقاطعان بالضبط عند نقطة أصل
     * العالم (0,0,0)، أوضح من خطوط الشبكة (سماكة أكبر) */
    {
        vp3d_vec3_t xa = { -extent, 0.0f, 0.0f };
        vp3d_vec3_t xb = {  extent, 0.0f, 0.0f };
        vp3d_draw_world_line(&cam, xa, xb, x, y, w, h, &g_tex_axis_x, AXIS_LINE_THICKNESS);

        vp3d_vec3_t za = { 0.0f, 0.0f, -extent };
        vp3d_vec3_t zb = { 0.0f, 0.0f,  extent };
        vp3d_draw_world_line(&cam, za, zb, x, y, w, h, &g_tex_axis_z, AXIS_LINE_THICKNESS);
    }

    /* عقد المشهد - كل عقدة عندها تمثيل بصري 3D مسجَّل (node_editor_registry)
     * تُرسم بموضعها الحقيقي (لو أمام الكاميرا)؛ اللي بلا تمثيل
     * (draw_3d == NULL، مثل Element المجردة أو أي عقدة 2D) تُتجاهل
     * بأمان بلا أي فحص خاص هنا */
    {
        int selected = scene_tree_panel_get_selected_index();
        int node_count = scene_tree_panel_get_node_count();
        for (int i = 0; i < node_count; i++) {
            node_type_t type = scene_tree_panel_get_type(i);
            const node_editor_registry_entry_t *ed = node_editor_registry_get(type);
            if (ed == NULL || ed->draw_3d == NULL) continue;

            const node_registry_entry_t *schema = node_registry_get(type);
            node_property_value_t *values = scene_tree_panel_get_values(i);
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
        return 0; /* خلف الكاميرا - غير قابلة للرسم */
    }

    float sx, sy;
    vp3d_view_to_screen(v, cam_x, cam_y, cam_w, cam_h, &sx, &sy);
    *out_screen_x = (int)(sx + 0.5f);
    *out_screen_y = (int)(sy + 0.5f);
    return 1;
}
