/*
 * ============================================================
 * viewport_2d.c
 * ============================================================
 */

#include <stdlib.h>

#include "viewport_2d.h"
#include "window.h"
#include "scene_tree_panel.h"
#include "node_editor_registry.h"

/* ألوان عالم التطوير 2D - خلفية محايدة داكنة، شبكة رمادية خفيفة،
 * ومحوران بنفس الاصطلاح الشائع ببرامج التطوير ثلاثية/ثنائية
 * الأبعاد: أحمر لمحور X، أخضر لمحور Y */
#define BG_R  0x2B
#define BG_G  0x2B
#define BG_B  0x2E

#define GRID_R  0x3F
#define GRID_G  0x3F
#define GRID_B  0x42

#define AXIS_X_R  220
#define AXIS_X_G   60
#define AXIS_X_B   60

#define AXIS_Y_R   60
#define AXIS_Y_G  200
#define AXIS_Y_B   90

#define GRID_SPACING  32  /* شبكة كل 32 بكسل عالمية */
#define PS2_WIDTH     640
#define PS2_HEIGHT    448

/* ------------------------------------------------------------
 * حالة الكاميرا الحرة - إزاحة Pan (بكسل شاشة، عن منتصف منطقة
 * الكاميرا) + مستوى تكبير Zoom (مضاعف على GRID_SPACING). يُحدَّثان
 * بالزر الأوسط للسحب وعجلة الماوس للتكبير عبر viewport_2d_update،
 * ويُقرآن هنا بالرسم والإسقاط
 * ------------------------------------------------------------ */
static float g_pan_offset_x = 0.0f;
static float g_pan_offset_y = 0.0f;
static float g_zoom         = 1.0f;
static int   g_pan_active   = 0;
static int   g_drag_node    = -1;

#define ZOOM_MIN         0.1f
#define ZOOM_MAX         8.0f
#define ZOOM_WHEEL_MULT  1.15f

void viewport_2d_update(int x, int y, int w, int h) {
    if (w <= 0 || h <= 0) return;

    int mx = window_mouse_x();
    int my = window_mouse_y();
    int inside = (mx >= x && mx < x + w && my >= y && my < y + h);

    if (!g_pan_active) {
        if (inside && window_mouse_middle_just_pressed()) {
            g_pan_active = 1;
        }
    } else if (!window_mouse_middle_down()) {
        g_pan_active = 0;
    }

    if (g_pan_active) {
        g_pan_offset_x += (float)window_mouse_delta_x();
        g_pan_offset_y += (float)window_mouse_delta_y();
    }

    int wheel = window_mouse_wheel_delta();
    if (wheel != 0 && inside) {
        /* نحسب أي نقطة عالمية تقع تحت المؤشر بالتكبير الحالي، ثم
         * نعدّل الإزاحة بعد تغيير التكبير عشان نفس النقطة تبقى تحت
         * المؤشر بالضبط - نفس سلوك Figma/Photoshop عند التكبير */
        float old_spacing = g_zoom;
        float origin_x = (float)(x + w / 2) + g_pan_offset_x;
        float origin_y = (float)(y + h / 2) + g_pan_offset_y;

        float world_under_x = ((float)mx - origin_x) / old_spacing;
        float world_under_y = ((float)my - origin_y) / old_spacing;

        if (wheel > 0) g_zoom *= ZOOM_WHEEL_MULT; else g_zoom /= ZOOM_WHEEL_MULT;
        if (g_zoom < ZOOM_MIN) g_zoom = ZOOM_MIN;
        if (g_zoom > ZOOM_MAX) g_zoom = ZOOM_MAX;

        float new_spacing = g_zoom;
        float new_origin_x = (float)mx - world_under_x * new_spacing;
        float new_origin_y = (float)my - world_under_y * new_spacing;

        g_pan_offset_x = new_origin_x - (float)(x + w / 2);
        g_pan_offset_y = new_origin_y - (float)(y + h / 2);
    }

    /* تحديد/سحب عقد 2D: إحداثيات المشهد بكسلات عالمية، وأصل العالم
     * هو مركز إطار PS2. نكتب القيم في نفس مصفوفة الخصائص التي تستخدمها
     * لوحة الخصائص، لذلك يظهر التغيير فوراً ويمكن حفظه كأي تعديل آخر. */
    if (inside && window_mouse_left_just_pressed()) {
        int best = -1;
        int best_dist = 18;
        int count = scene_tree_panel_get_node_count();
        for (int i = 0; i < count; i++) {
            node_property_value_t *values = scene_tree_panel_get_values(i);
            const node_registry_entry_t *schema = node_registry_get(scene_tree_panel_get_type(i));
            if (values == NULL || schema == NULL || schema->property_count < 2) continue;
            int sx, sy;
            viewport_2d_project(values[0].f, values[1].f, x, y, w, h, &sx, &sy);
            int dx = mx - sx, dy = my - sy;
            int dist = abs(dx) + abs(dy);
            if (dist < best_dist) { best_dist = dist; best = i; }
        }
        if (best >= 0) {
            scene_tree_panel_select_index(best);
            g_drag_node = best;
        }
    }
    if (g_drag_node >= 0) {
        if (!window_mouse_left_down()) g_drag_node = -1;
        else {
            float world_x, world_y;
            viewport_2d_unproject(mx, my, x, y, w, h, &world_x, &world_y);
            scene_tree_panel_set_float_property(g_drag_node, 0, world_x);
            scene_tree_panel_set_float_property(g_drag_node, 1, world_y);
        }
    }
}

void viewport_2d_draw(int x, int y, int w, int h) {
    if (w <= 0 || h <= 0) return;

    window_set_clip_rect(x, y, w, h);

    /* الخلفية */
    window_fill_rect(x, y, w, h, BG_R, BG_G, BG_B);

    /* نقطة أصل العالم (0,0) - منتصف منطقة العرض + إزاحة Pan الحالية،
     * والتباعد الفعلي بين خطوط الشبكة يتحرك مع مستوى Zoom الحالي */
    int origin_x = x + w / 2 + (int)g_pan_offset_x;
    int origin_y = y + h / 2 + (int)g_pan_offset_y;
    int spacing = (int)((float)GRID_SPACING * g_zoom + 0.5f);
    if (spacing < 1) spacing = 1;

    /* الشبكة - خطوط عمودية وأفقية رفيعة، محاذاة بالضبط على نقطة
     * الأصل (تمر من عندها، مو من حافة الشاشة) وتمتد لكل منطقة العرض */
    int start_kx = (x - origin_x) / spacing - 1;
    for (int k = start_kx; ; k++) {
        int gx = origin_x + k * spacing;
        if (gx > x + w) break;
        if (gx >= x) window_fill_rect(gx, y, 1, h, GRID_R, GRID_G, GRID_B);
    }
    int start_ky = (y - origin_y) / spacing - 1;
    for (int k = start_ky; ; k++) {
        int gy = origin_y + k * spacing;
        if (gy > y + h) break;
        if (gy >= y) window_fill_rect(x, gy, w, 1, GRID_R, GRID_G, GRID_B);
    }

    /* محورا X وY - يتقاطعان بالضبط عند نقطة الأصل، أوضح من خطوط
     * الشبكة (سماكة 2 بكسل بدل 1) */
    window_fill_rect(x, origin_y - 1, w, 2, AXIS_X_R, AXIS_X_G, AXIS_X_B);
    window_fill_rect(origin_x - 1, y, 2, h, AXIS_Y_R, AXIS_Y_G, AXIS_Y_B);

    /* منطقة الإخراج الفعلية للـPS2: مركزها هو (0,0) في عالم Rebax،
     * وحجمها ثابت 640x448 بوحدات العالم. */
    int left = origin_x - (int)(PS2_WIDTH * g_zoom / 2.0f);
    int top = origin_y - (int)(PS2_HEIGHT * g_zoom / 2.0f);
    int frame_w = (int)(PS2_WIDTH * g_zoom);
    int frame_h = (int)(PS2_HEIGHT * g_zoom);
    window_fill_rect(left, top, frame_w, 2, 245, 190, 55);
    window_fill_rect(left, top + frame_h - 2, frame_w, 2, 245, 190, 55);
    window_fill_rect(left, top, 2, frame_h, 245, 190, 55);
    window_fill_rect(left + frame_w - 2, top, 2, frame_h, 245, 190, 55);

    /* عقد المشهد - كل عقدة عندها تمثيل بصري 2D مسجَّل (node_editor_registry)
     * تُرسم بموضعها الحقيقي؛ اللي بلا تمثيل (draw_2d == NULL، مثل
     * Element المجردة أو أي عقدة 3D) تُتجاهل بأمان بلا أي فحص خاص هنا */
    {
        int selected = scene_tree_panel_get_selected_index();
        int node_count = scene_tree_panel_get_node_count();
        for (int i = 0; i < node_count; i++) {
            node_type_t type = scene_tree_panel_get_type(i);
            const node_editor_registry_entry_t *ed = node_editor_registry_get(type);
            if (ed == NULL || ed->draw_2d == NULL) continue;

            const node_registry_entry_t *schema = node_registry_get(type);
            node_property_value_t *values = scene_tree_panel_get_values(i);
            if (schema == NULL || values == NULL) continue;

            ed->draw_2d(values, schema->property_count, x, y, w, h, (i == selected));
        }
    }

    window_clear_clip_rect();
}

void viewport_2d_project(float world_x, float world_y,
                          int cam_x, int cam_y, int cam_w, int cam_h,
                          int *out_screen_x, int *out_screen_y) {
    int origin_x = cam_x + cam_w / 2 + (int)g_pan_offset_x;
    int origin_y = cam_y + cam_h / 2 + (int)g_pan_offset_y;
    float spacing = g_zoom;

    /* نفس تحويل الشبكة بالضبط: وحدة عالم واحدة = spacing بكسل.
     * Rebax يطابق إحداثيات PS2 هنا: Y الموجب يتجه إلى أسفل الشاشة. */
    *out_screen_x = origin_x + (int)(world_x * spacing);
    *out_screen_y = origin_y + (int)(world_y * spacing);
}

void viewport_2d_unproject(int screen_x, int screen_y,
                            int cam_x, int cam_y, int cam_w, int cam_h,
                            float *out_world_x, float *out_world_y) {
    int origin_x = cam_x + cam_w / 2 + (int)g_pan_offset_x;
    int origin_y = cam_y + cam_h / 2 + (int)g_pan_offset_y;
    if (out_world_x) *out_world_x = ((float)screen_x - origin_x) / g_zoom;
    if (out_world_y) *out_world_y = ((float)screen_y - origin_y) / g_zoom;
}
