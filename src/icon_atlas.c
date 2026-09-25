/*
 * ============================================================
 * icon_atlas.c
 * ============================================================
 * ملاحظة: هذا الملف يُعرِّف STB_IMAGE_IMPLEMENTATION (التطبيق
 * الفعلي لمكتبة stb_image) - مرة واحدة بس بكل المشروع، هنا تحديداً
 * لأنه المستخدم الوحيد حالياً لفك ضغط PNG. أي ملف ثاني يحتاج
 * stb_image مستقبلاً يكتفي بـ #include "stb_image.h" بدون الماكرو،
 * وإلا يصير تعريف مكرر وخطأ ربط.
 * ============================================================
 */

#include <stddef.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "embedded_resources.h"
#include "icon_atlas.h"
#include "window.h"

#define ATLAS_COLS 16
#define CELL_SIZE  16

static window_texture_t *g_atlas_tex = NULL;
static int g_initialized = 0;

int icon_atlas_init(void) {
    if (g_initialized) {
        return 1;
    }

    int w, h, channels;
    unsigned char *pixels = stbi_load_from_memory(
        _binary_embedded_engine_images_icons_icons_png_start,
        (int)embedded_icon_atlas_size(),
        &w, &h, &channels, 4
    );
    if (pixels == NULL) {
        return 0;
    }

    g_atlas_tex = window_create_texture(pixels, w, h);
    stbi_image_free(pixels);

    g_initialized = (g_atlas_tex != NULL);
    return g_initialized;
}

/* يحسب موقع الخلية (بالبكسل) داخل الأطلس من رقمها التسلسلي */
static void cell_position(int icon_id, int *out_x, int *out_y) {
    *out_x = (icon_id % ATLAS_COLS) * CELL_SIZE;
    *out_y = (icon_id / ATLAS_COLS) * CELL_SIZE;
}

void icon_atlas_draw(int icon_id, int x, int y, int size) {
    if (!g_initialized) {
        return;
    }
    int sx, sy;
    cell_position(icon_id, &sx, &sy);
    window_draw_texture_region(g_atlas_tex, sx, sy, CELL_SIZE, CELL_SIZE, x, y, size, size);
}

void icon_atlas_draw_rotated(int icon_id, int x, int y, int size, double angle_degrees) {
    if (!g_initialized) {
        return;
    }
    int sx, sy;
    cell_position(icon_id, &sx, &sy);
    window_draw_texture_region_rotated(g_atlas_tex, sx, sy, CELL_SIZE, CELL_SIZE,
                                        x, y, size, size, angle_degrees);
}

void icon_atlas_draw_tinted(int icon_id, int x, int y, int size,
                             unsigned char r, unsigned char g, unsigned char b) {
    if (!g_initialized) {
        return;
    }
    int sx, sy;
    cell_position(icon_id, &sx, &sy);
    window_draw_texture_region_tinted(g_atlas_tex, sx, sy, CELL_SIZE, CELL_SIZE,
                                       x, y, size, size, r, g, b);
}

void icon_atlas_shutdown(void) {
    if (g_atlas_tex != NULL) {
        window_destroy_texture(g_atlas_tex);
        g_atlas_tex = NULL;
    }
    g_initialized = 0;
}
