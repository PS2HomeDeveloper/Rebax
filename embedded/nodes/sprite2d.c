/*
 * ============================================================
 * sprite2d.c
 * ============================================================
 * عقدة تعرض صورة ثنائية الأبعاد بموضع/دوران/حجم قابل للتعديل -
 * أول عقدة تحمّل وترسم محتوى حقيقي (بعكس Element2D/Element3D اللي
 * لسه هيكل أولي بلا رسم فعلي).
 *
 * التحميل عبر embedded_images.h المشتركة (يوفّرها مصدِّر التصدير
 * وقت كل تصدير، راجع ps2_exporter.c) - بكسلات خام (GS_PSM_CT32)
 * محوَّلة مسبقاً وقت التصدير نفسه (على جهاز التطوير، لا PS2)، لا أي
 * صيغة ضغط (PNG/JPEG/...) تصل PS2 إطلاقاً، ولا أي وصول لنظام ملفات
 * وقت التشغيل - بالضبط نفس أسلوب التطوير اليدوي الاحترافي (راجع
 * gsKit_texture_finish المستخدمة هنا، نفس تقنية تضمين الخطوط
 * بمشاريع PS2 يدوية حقيقية). هذا يبقي الملف التنفيذي خالياً من
 * libpng/libjpeg/libtiff بالكامل - لا حاجة لها إطلاقاً بعد اليوم.
 *
 * @PS2_EXPORT_LIBS: -L$(GSKIT)/lib -lgskit -lgskit_toolkit -ldmakit
 * @PS2_EXPORT_INCLUDES: -I$(GSKIT)/include
 * ============================================================
 */

#include <string.h>
#include <stdlib.h>
#include <malloc.h>

#include <gsKit.h>

#include "node_interface.h"
#include "engine_context.h"
#include "embedded_images.h"

/* gsKit_texture_finish غير معلنة بأي هيدر عام من gsKit (extern
 * داخلية فقط ضمن gsToolkit.c نفسه) - نفس التصريح اليدوي المستخدَم
 * بأي مشروع PS2 احترافي يرفع نسيجاً خاماً جاهزاً لـVRAM مباشرة بلا
 * فك أي صيغة ضغط (بدل الاعتماد على gsKit_texture_png/jpeg/... اللي
 * تجرّ مكتبات فك ضغط خارجية كاملة بلا داعٍ) */
extern int gsKit_texture_finish(GSGLOBAL *gsGlobal, GSTEXTURE *Texture);

/* بيانات Sprite2D الحقيقية - هذا الشكل اللي يُخصَّص له instance_size
 * بايت فعلياً وقت تحميل المشهد، وهو اللي offsetof() أسفل يشاور على
 * حقوله بالضبط. texture/texture_loaded حالة داخلية بس - ليست
 * خصائص قابلة للتعديل من لوحة الخصائص، فما لهم أي دخول بمصفوفة
 * sprite2d_properties ولا sprite2d_property_offsets تحت */
typedef struct {
    float position_x;
    float position_y;
    float rotation;   /* بالراديان - غير مطبَّق بالرسم بعد، راجع sprite2d_draw */
    float scale_x;
    float scale_y;
    const char *image_path; /* نفس النص بالضبط اللي بلوحة الخصائص - مفتاح البحث بـembedded_image_find */

    GSTEXTURE texture;
    int texture_loaded; /* 0 = لسه ما تحمّلت (أو الصورة غير مُضمَّنة أصلاً) */
} sprite2d_instance_t;

static void sprite2d_init(void *self) {
    sprite2d_instance_t *inst = (sprite2d_instance_t *)self;
    memset(&inst->texture, 0, sizeof(GSTEXTURE));
    inst->texture_loaded = 0;
}

/* يحمّل القوام من الصورة المُضمَّنة بالملف التنفيذي نفسها - كسول
 * (أول استدعاء رسم بس) عشان نضمن كل الخصائص مكتوبة فعلياً من قيم
 * المشهد المحمَّلة قبل أي محاولة تحميل */
static void sprite2d_load_texture(sprite2d_instance_t *inst) {
    if (inst->texture_loaded) return;
    if (inst->image_path == NULL || inst->image_path[0] == '\0') return;

    const embedded_image_t *img = embedded_image_find(inst->image_path);
    if (img == NULL) return; /* غير مُضمَّنة (فشل تحويلها وقت التصدير، أو مسار غير صالح) */

    GSGLOBAL *gsGlobal = engine_get_gs_global();

    inst->texture.Width  = img->width;
    inst->texture.Height = img->height;
    inst->texture.PSM    = GS_PSM_CT32;
    inst->texture.Filter = GS_FILTER_LINEAR;

    unsigned int need = gsKit_texture_size_ee(inst->texture.Width, inst->texture.Height, inst->texture.PSM);
    inst->texture.Mem = memalign(128, need);
    if (inst->texture.Mem == NULL) return;

    memcpy(inst->texture.Mem, img->data, need);

    inst->texture_loaded = (gsKit_texture_finish(gsGlobal, &inst->texture) == 0);
}

static void sprite2d_update(void *self, float delta_time) {
    (void)self;
    (void)delta_time;
}

static void sprite2d_draw(void *self) {
    sprite2d_instance_t *inst = (sprite2d_instance_t *)self;

    sprite2d_load_texture(inst);
    if (!inst->texture_loaded) return;

    GSGLOBAL *gsGlobal = engine_get_gs_global();

    float half_w = ((float)inst->texture.Width  * inst->scale_x) / 2.0f;
    float half_h = ((float)inst->texture.Height * inst->scale_y) / 2.0f;

    /* ملاحظة: rotation مخزَّنة كخاصية حقيقية لكن غير مطبَّقة بالرسم
     * هنا بعد - gsKit_prim_sprite_texture يرسم مستطيلاً محاذياً
     * للمحاور بس (بلا دوران). دوران فعلي يحتاج تحويل لأربع رؤوس
     * مستقلة عبر gsKit_prim_quad_texture بدل هذا الماكرو - خطوة
     * قادمة منفصلة، بلا ما تكسر أي شيء هنا لما نضيفها */
    gsKit_prim_sprite_texture(gsGlobal, &inst->texture,
        inst->position_x - half_w, inst->position_y - half_h, 0.0f, 0.0f,
        inst->position_x + half_w, inst->position_y + half_h,
        (float)inst->texture.Width, (float)inst->texture.Height,
        1, GS_SETREG_RGBAQ(0x80, 0x80, 0x80, 0x80, 0x00));
}

static void sprite2d_destroy(void *self) {
    sprite2d_instance_t *inst = (sprite2d_instance_t *)self;
    if (inst->texture.Mem != NULL) {
        free(inst->texture.Mem);
        inst->texture.Mem = NULL;
    }
    /* ملاحظة: تحرير ذاكرة VRAM نفسها (inst->texture.Vram) يحتاج
     * gsKit_vram_clear أو مدير VRAM على مستوى المحرك كامل (يعيد
     * استخدام المساحة لعقد ثانية) - خطوة قادمة منفصلة على مستوى
     * محمّل المشهد كامل، مو هذا الملف تحديداً */
}

static const node_property_t sprite2d_properties[] = {
    { "Position X",  NODE_PROPERTY_TYPE_FLOAT,  { .f = 0.0f }, 0 },
    { "Position Y",  NODE_PROPERTY_TYPE_FLOAT,  { .f = 0.0f }, 0 },
    { "Rotation",    NODE_PROPERTY_TYPE_FLOAT,  { .f = 0.0f }, 0 },
    { "Scale X",     NODE_PROPERTY_TYPE_FLOAT,  { .f = 1.0f }, 0 },
    { "Scale Y",     NODE_PROPERTY_TYPE_FLOAT,  { .f = 1.0f }, 0 },
    { "Image Path",  NODE_PROPERTY_TYPE_STRING, { .s = "" },   1 }  /* is_asset_path=1: زر تصفح بدل خانة كتابة - مكتبة التصدير تحوّلها تلقائياً لبكسلات خام مُضمَّنة، بلا أي إعداد يدوي إضافي */
};

/* موقع كل خاصية بالضبط بذاكرة sprite2d_instance_t - بنفس ترتيب
 * sprite2d_properties فوق حرفياً */
static const size_t sprite2d_property_offsets[] = {
    offsetof(sprite2d_instance_t, position_x),
    offsetof(sprite2d_instance_t, position_y),
    offsetof(sprite2d_instance_t, rotation),
    offsetof(sprite2d_instance_t, scale_x),
    offsetof(sprite2d_instance_t, scale_y),
    offsetof(sprite2d_instance_t, image_path)
};

const node_interface_t sprite2d_interface = {
    sprite2d_init,
    sprite2d_update,
    sprite2d_draw,
    sprite2d_destroy,
    sprite2d_properties,
    6,
    sizeof(sprite2d_instance_t),
    sprite2d_property_offsets
};

/* @NODE type=NODE_TYPE_SPRITE_2D name=Sprite2D icon=element_2d properties=sprite2d_properties */
/* التصريح فوق هو المصدر الوحيد اللي يعرّف Sprite2D بمحرر التطوير -
 * لازم يبقى آخر سطر بالملف. أيقونة مؤقتة (element_2d) لحد ما تُضاف
 * أيقونة مخصصة لها بـicons_src/ */
