/*
 * ============================================================
 * element_3d.c
 * ============================================================
 * نفس فكرة Element2D بس بمعنى مكاني ثلاثي الأبعاد - موقع/دوران/
 * حجم بمحاور X وY وZ. حالياً هيكل أولي للاختبار (منطق التصدير) -
 * بلا رسم فعلي بعد.
 * ============================================================
 */

#include "node_interface.h"

/* بيانات Element3D الحقيقية - نفس فكرة element2d_instance_t */
typedef struct {
    float position_x;
    float position_y;
    float position_z;
    float rotation_x;
    float rotation_y;
    float rotation_z;
    float scale_x;
    float scale_y;
    float scale_z;
} element3d_instance_t;

static void element3d_init(void *self) {
    (void)self;
}

static void element3d_update(void *self, float delta_time) {
    (void)self;
    (void)delta_time;
}

static void element3d_draw(void *self) {
    (void)self;
    /* لا رسم فعلي بعد - هيكل أولي للاختبار */
}

static void element3d_destroy(void *self) {
    (void)self;
}

static const node_property_t element3d_properties[] = {
    { "Position X",  NODE_PROPERTY_TYPE_FLOAT, { .f = 0.0f }, 0 },
    { "Position Y",  NODE_PROPERTY_TYPE_FLOAT, { .f = 0.0f }, 0 },
    { "Position Z",  NODE_PROPERTY_TYPE_FLOAT, { .f = 0.0f }, 0 },
    { "Rotation X",  NODE_PROPERTY_TYPE_FLOAT, { .f = 0.0f }, 0 },
    { "Rotation Y",  NODE_PROPERTY_TYPE_FLOAT, { .f = 0.0f }, 0 },
    { "Rotation Z",  NODE_PROPERTY_TYPE_FLOAT, { .f = 0.0f }, 0 },
    { "Scale X",     NODE_PROPERTY_TYPE_FLOAT, { .f = 1.0f }, 0 },
    { "Scale Y",     NODE_PROPERTY_TYPE_FLOAT, { .f = 1.0f }, 0 },
    { "Scale Z",     NODE_PROPERTY_TYPE_FLOAT, { .f = 1.0f }, 0 }
};

/* مواقع الخصائص فوق داخل element3d_instance_t - بنفس الترتيب بالضبط،
 * منفصلة عن element3d_properties تماماً (نفس مبدأ Element2D) */
static const size_t element3d_offsets[] = {
    offsetof(element3d_instance_t, position_x),
    offsetof(element3d_instance_t, position_y),
    offsetof(element3d_instance_t, position_z),
    offsetof(element3d_instance_t, rotation_x),
    offsetof(element3d_instance_t, rotation_y),
    offsetof(element3d_instance_t, rotation_z),
    offsetof(element3d_instance_t, scale_x),
    offsetof(element3d_instance_t, scale_y),
    offsetof(element3d_instance_t, scale_z)
};

const node_interface_t element3d_interface = {
    element3d_init,
    element3d_update,
    element3d_draw,
    element3d_destroy,
    element3d_properties,
    9,
    sizeof(element3d_instance_t),
    element3d_offsets
};

/* @NODE type=NODE_TYPE_ELEMENT_3D name=Element3D icon=element_3d properties=element3d_properties */
/* التصريح فوق هو المصدر الوحيد اللي يعرّف Element3D بمحرر التطوير -
 * لازم يبقى آخر سطر بالملف */
