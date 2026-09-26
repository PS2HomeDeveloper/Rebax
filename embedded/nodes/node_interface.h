/*
 * ============================================================
 * node_interface.h
 * ============================================================
 * هذا الهيدر يُترجم حصراً بمترجمات PS2 (EE: mips64r5900el-ps2-elf-gcc
 * أو g++) - ما له أي علاقة بأكواد المحرك نفسه (aarch64). يعرّف
 * الشكل الموحد اللي أي عقدة تشتغل وقت تشغيل اللعبة الفعلي على
 * PS2 لازم تلتزم فيه، عشان أي كود عام بالمحرك يتعامل مع أي نوع
 * عقدة بدون ما يعرف تفاصيلها الداخلية.
 *
 * كل عقدة جديدة تُنشأ لاحقاً (src/nodes/<اسم>.c) تعرّف متغيراً
 * من نوع node_interface_t معبّأ بدوالها الأربع، بنفس نمط element.c.
 * ============================================================
 */

#ifndef NODE_INTERFACE_H
#define NODE_INTERFACE_H

#include <stddef.h> /* size_t, offsetof - تحتاجها instance_size وproperty_offsets تحت */

/* نوع قيمة الخاصية - يحدد كيف يعرضها بانل الخصائص لاحقاً */
typedef enum {
    NODE_PROPERTY_TYPE_FLOAT,
    NODE_PROPERTY_TYPE_INT,
    NODE_PROPERTY_TYPE_STRING
} node_property_type_t;

/* خاصية واحدة تصرّح عنها العقدة - اسمها، نوعها، وقيمتها الافتراضية.
 * بانل الخصائص يقرأ هذي القائمة تلقائياً لأي عقدة، بدون ما يعرف
 * تفاصيلها - نفس فلسفة node_interface_t نفسها */
typedef struct {
    const char *name;
    node_property_type_t type;
    union {
        float f;
        int i;
        const char *s;
    } default_value;

    /* 1 فقط لو كانت الخاصية NODE_PROPERTY_TYPE_STRING وتمثّل مساراً
     * لملف داخل Assets/ الخاصة بالمشروع (صورة، صوت...) - properties_panel
     * تعرض وقتها زر "تصفح" (يفتح asset_browser) بدل خانة كتابة حرة.
     * صفر افتراضياً لكل خاصية ما تحدّده صراحة - آمن تماماً للعقد
     * الحالية (Element/Element2D/Element3D)، بلا أي تعديل عليها */
    int is_asset_path;
} node_property_t;

typedef struct {
    /* يُستدعى مرة واحدة عند إنشاء العقدة بالمشهد */
    void (*init)(void *self);

    /* يُستدعى كل إطار وقت تشغيل اللعبة - delta_time بالثانية منذ آخر إطار */
    void (*update)(void *self, float delta_time);

    /* يُستدعى كل إطار بعد update، لرسم العقدة (لو كان لها أي تمثيل مرئي) */
    void (*draw)(void *self);

    /* يُستدعى مرة واحدة عند إزالة العقدة أو إغلاق اللعبة */
    void (*destroy)(void *self);

    /* قائمة خصائص العقدة (لبانل الخصائص) - properties يمكن تكون
     * NULL وproperty_count صفر لو العقدة بلا خصائص خاصة بها */
    const node_property_t *properties;
    int property_count;

    /* حجم بنية بيانات العقدة الحقيقية بالبايت (sizeof نوعها الداخلي
     * الخاص - غير معروف هنا، كل عقدة تعرّفه بملفها). محمّل المشهد
     * وقت التصدير يستخدمه ليعرف كم بايت يخصص لكل نسخة من هذي
     * العقدة بالذاكرة وقت تشغيل اللعبة الفعلي - بدون هذا، self
     * بدوال init/update/draw/destroy مؤشر بلا حجم معروف إطلاقاً */
    size_t instance_size;

    /* موقع كل خاصية بالبايت داخل بنية بيانات العقدة (offsetof) -
     * مصفوفة موازية لproperties بنفس الترتيب وproperty_count بالضبط
     * (NULL لو property_count صفر، زي properties بالضبط). هذي هي
     * الجسر الحقيقي بين "القيمة اللي ضبطها المطور بالمحرر" و"موقعها
     * الفعلي بذاكرة العقدة" - بدونها ما فيه طريقة يوصل التعديل من
     * لوحة الخصائص لأي مكان حقيقي وقت تشغيل اللعبة على PS2 */
    const size_t *property_offsets;
} node_interface_t;

#endif /* NODE_INTERFACE_H */
