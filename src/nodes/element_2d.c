/*
 * ============================================================
 * element_2d.c
 * ============================================================
 * أول عقدة فيها معنى مكاني فعلي - موقع/دوران/حجم ثنائي الأبعاد.
 * حالياً هيكل أولي للاختبار (منطق التصدير) - بلا رسم فعلي بعد.
 * ============================================================
 */

#include "node_interface.h"

/* بيانات Element2D الحقيقية - هذا الشكل اللي يُخصَّص له instance_size
 * بايت فعلياً وقت تحميل المشهد على PS2، وهو اللي offsetof() أسفل
 * يشاور على حقوله بالضبط */
typedef struct {
    float position_x;
    float position_y;
    float rotation;
    float scale_x;
    float scale_y;
} element2d_instance_t;

static void element2d_init(void *self) {
    (void)self;
}

static void element2d_update(void *self, float delta_time) {
    (void)self;
    (void)delta_time;
}

static void element2d_draw(void *self) {
    (void)self;
    /* لا رسم فعلي بعد - هيكل أولي للاختبار */
}

static void element2d_destroy(void *self) {
    (void)self;
}

static const node_property_t element2d_properties[] = {
    { "Position X", NODE_PROPERTY_TYPE_FLOAT, { .f = 0.0f }, 0 },
    { "Position Y", NODE_PROPERTY_TYPE_FLOAT, { .f = 0.0f }, 0 },
    { "Rotation",   NODE_PROPERTY_TYPE_FLOAT, { .f = 0.0f }, 0 },
    { "Scale X",    NODE_PROPERTY_TYPE_FLOAT, { .f = 1.0f }, 0 },
    { "Scale Y",    NODE_PROPERTY_TYPE_FLOAT, { .f = 1.0f }, 0 }
};

/* مواقع الخصائص فوق داخل element2d_instance_t - بنفس الترتيب بالضبط
 * (فهرس مقابل فهرس). محمّل المشهد وقت التصدير يستخدمها ليكتب قيمة
 * كل خاصية بموقعها الصحيح فعلياً بالذاكرة - محرر التطوير (aarch64)
 * ما يقرأ ولا يحتاج هذي المصفوفة إطلاقاً (لهذا هي منفصلة تماماً عن
 * element2d_properties فوق، مو حقلاً إضافياً فيها) */
static const size_t element2d_offsets[] = {
    offsetof(element2d_instance_t, position_x),
    offsetof(element2d_instance_t, position_y),
    offsetof(element2d_instance_t, rotation),
    offsetof(element2d_instance_t, scale_x),
    offsetof(element2d_instance_t, scale_y)
};

const node_interface_t element2d_interface = {
    element2d_init,
    element2d_update,
    element2d_draw,
    element2d_destroy,
    element2d_properties,
    5,
    sizeof(element2d_instance_t),
    element2d_offsets
};

/* @NODE type=NODE_TYPE_ELEMENT_2D name=Element2D icon=element_2d properties=element2d_properties */
/* التصريح فوق هو المصدر الوحيد اللي يعرّف Element2D بمحرر التطوير -
 * ملف البناء يقرأه تلقائياً كل بناء (وبالذات مصفوفة الخصائص
 * element2d_properties فوق - تُنسخ حرفياً بدون أي تكرار يدوي بمكان
 * ثانٍ). لازم يبقى آخر سطر بالملف */
