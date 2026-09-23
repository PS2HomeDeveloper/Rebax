/*
 * ============================================================
 * element.c
 * ============================================================
 * تطبيق عقدة Element - العقدة الأساسية المجردة، أب لكل عقد
 * المحرك. وظيفتها تنظيمية بحتة (تجميع عقد ثانية تحتها بالشجرة)،
 * بلا أي معنى مكاني وبلا أي سلوك وقت التشغيل - فدوالها الأربع
 * فاضية عمداً.
 * ============================================================
 */

#include "node_interface.h"

/* بلا بيانات فعلية (عقدة تنظيمية بحتة) - حقل وهمي واحد بس عشان
 * struct ما يكون فاضياً تماماً (غير مسموح بـC خالص) */
typedef struct {
    char _unused;
} element_instance_t;

static void element_init(void *self) {
    (void)self;
    /* عقدة تنظيمية بحتة - لا تحتاج أي تهيئة */
}

static void element_update(void *self, float delta_time) {
    (void)self;
    (void)delta_time;
    /* لا سلوك وقت التشغيل */
}

static void element_draw(void *self) {
    (void)self;
    /* لا تمثيل مرئي */
}

static void element_destroy(void *self) {
    (void)self;
    /* لا موارد لتحريرها */
}

/* الواجهة الموحّدة لعقدة Element - أي كود عام بالمحرك يستخدم هذا
 * المتغيّر للتعامل مع عقد Element بدون معرفة تفاصيلها.
 * بلا خصائص خاصة بها (properties = NULL) - عقدة تنظيمية مجردة */
const node_interface_t element_interface = {
    element_init,
    element_update,
    element_draw,
    element_destroy,
    NULL,
    0,
    sizeof(element_instance_t),
    NULL
};

/* @NODE type=NODE_TYPE_ELEMENT name=Element icon=Folder properties=NULL */
/* التصريح فوق هو المصدر الوحيد اللي يعرّف Element بمحرر التطوير -
 * ملف البناء يقرأه تلقائياً كل بناء ويولّد نسخة العرض بنفسه، بلا
 * أي تعديل يدوي بمكان ثانٍ. لازم يبقى آخر سطر بالملف (بعد تعريف
 * الخصائص والواجهة) عشان يقدر يقرأهم قبل ما يوصله */
