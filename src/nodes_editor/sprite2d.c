/*
 * ============================================================
 * sprite2d.c (nodes_editor)
 * ============================================================
 * التمثيل البصري لـSprite2D بمحرر التطوير - يحمّل الصورة الحقيقية
 * عبر image_loader.h الموحَّدة (نسخة المحرر) اللي تدعم كل الصيغ
 * اللي نسخة PS2 الحقيقية تدعمها بالضبط (PNG/JPEG/BMP/TGA/TIFF عبر
 * stb_image داخلياً، وRAW/TIM2/TIM عبر فك تشفيرنا الخاص) - فيرسمها
 * بموضعها وحجمها الحقيقيين، مع علامة تحديد (نقطة بيضاء + زائد أخضر
 * لو محددة) بنفس أسلوب باقي عقد nodes_editor.
 *
 * الترتيب [0]=Position X, [1]=Position Y, [2]=Rotation, [3]=Scale X,
 * [4]=Scale Y, [5]=Image Path, [6]=Raw Width, [7]=Raw Height,
 * [8]=Raw Format - ثابت هنا لأن هذا الملف نفسه مصدر ترتيب خصائص
 * Sprite2D (src/nodes/sprite2d.c).
 *
 * تخزين مؤقت (Cache) بالمسار: أول مرة تُطلب صورة بمسار معين تُحمَّل
 * وتُخزَّن، أي طلب لاحق بنفس المسار (حتى من عقدة ثانية) يعيد
 * استخدام نفس القوام مباشرة - بلا إعادة قراءة القرص كل إطار. حد
 * أقصى 32 صورة مختلفة مفتوحة بآن واحد (سعة ثابتة معقولة، بلا أي
 * إخلاء تلقائي بعد - أول تبسيط معروف، كافٍ حالياً).
 * ============================================================
 */

#include <string.h>
#include <stdlib.h>

#include "node_editor_interface.h"
#include "viewport_2d.h"
#include "window.h"
#include "image_loader.h" /* المحلل الموحَّد لنسخة المحرر - يدعم PNG/JPEG/BMP/TGA/TIFF (عبر stb_image داخلياً) + RAW/TIM2/TIM (فك تشفيرنا الخاص) */

#define MARKER_HALF        3
#define SELECT_CROSS_HALF  6
#define SPRITE_CACHE_MAX   32

typedef struct {
    char path[256];
    window_texture_t *tex;
    int width, height;
    int valid;
    int load_failed; /* 1 لو حاولنا التحميل وفشل - نتجنب إعادة المحاولة كل إطار */
} sprite_cache_entry_t;

static sprite_cache_entry_t g_cache[SPRITE_CACHE_MAX];
static int g_cache_count = 0;

static sprite_cache_entry_t *sprite_cache_find(const char *path) {
    for (int i = 0; i < g_cache_count; i++) {
        if (g_cache[i].valid && strcmp(g_cache[i].path, path) == 0) return &g_cache[i];
    }
    return NULL;
}

/* يحمّل صورة جديدة للكاش (أو يرجّع مدخلاً بحالة فشل سابق) - NULL
 * لو الكاش ممتلئ تماماً ولا فيه مكان لصورة جديدة. raw_width/height/psm
 * تُمرَّر لمحلل RAW بس (تُهمَل لأي صيغة ثانية - راجع تعليق
 * image_loader.h) */
static sprite_cache_entry_t *sprite_cache_load(const char *path, int raw_width, int raw_height, int raw_psm) {
    if (g_cache_count >= SPRITE_CACHE_MAX) return NULL;

    sprite_cache_entry_t *entry = &g_cache[g_cache_count];
    strncpy(entry->path, path, sizeof(entry->path) - 1);
    entry->path[sizeof(entry->path) - 1] = '\0';

    int w, h;
    unsigned char *pixels = image_loader_editor_load(path, &w, &h, raw_width, raw_height, raw_psm);

    if (pixels == NULL) {
        entry->valid = 1;
        entry->load_failed = 1;
        entry->tex = NULL;
        g_cache_count++;
        return entry;
    }

    entry->tex = window_create_texture(pixels, w, h);
    entry->width = w;
    entry->height = h;
    entry->valid = 1;
    entry->load_failed = 0;
    free(pixels); /* مخصَّصة بـmalloc من image_loader_editor_load - free عادي، مو stbi_image_free */

    g_cache_count++;
    return entry;
}

void sprite2d_editor_draw(const node_property_value_t *values, int property_count,
                           int cam_x, int cam_y, int cam_w, int cam_h, int selected) {
    if (property_count < 9) return; /* الحد الأدنى: كل خصائص Sprite2D التسع (راجع src/nodes/sprite2d.c) */

    float pos_x = values[0].f;
    float pos_y = values[1].f;
    float scale_x = values[3].f;
    float scale_y = values[4].f;
    const char *path = values[5].s;
    int raw_width = values[6].i;
    int raw_height = values[7].i;
    int raw_format = values[8].i;

    int sx, sy;
    viewport_2d_project(pos_x, pos_y, cam_x, cam_y, cam_w, cam_h, &sx, &sy);

    if (path != NULL && path[0] != '\0') {
        sprite_cache_entry_t *entry = sprite_cache_find(path);
        if (entry == NULL) entry = sprite_cache_load(path, raw_width, raw_height, raw_format);

        if (entry != NULL && !entry->load_failed && entry->tex != NULL) {
            int draw_w = (int)((float)entry->width  * scale_x);
            int draw_h = (int)((float)entry->height * scale_y);
            /* ملاحظة: rotation (values[2].f) غير مطبَّق بالرسم هنا
             * بعد - window_draw_texture ترسم مستطيلاً محاذياً
             * للمحاور بس، بنفس القيد المذكور بنسخة PS2 بالضبط */
            window_draw_texture(entry->tex, sx - draw_w / 2, sy - draw_h / 2, draw_w, draw_h);
        }
    }

    /* نقطة بيضاء خفيفة تدل على موضع العقدة (محورها) دائماً - حتى
     * لو الصورة لسه ما تحمّلت أو فشلت - نفس أسلوب Element2D بالضبط */
    window_fill_rect(sx - MARKER_HALF, sy - MARKER_HALF, MARKER_HALF * 2, MARKER_HALF * 2, 225, 225, 225);

    if (selected) {
        window_fill_rect(sx - SELECT_CROSS_HALF, sy - 1, SELECT_CROSS_HALF * 2, 2, 70, 220, 120);
        window_fill_rect(sx - 1, sy - SELECT_CROSS_HALF, 2, SELECT_CROSS_HALF * 2, 70, 220, 120);
    }
}

/* @NODE_EDITOR type=NODE_TYPE_SPRITE_2D draw_2d=sprite2d_editor_draw */
