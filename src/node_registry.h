/*
 * ============================================================
 * node_registry.h
 * ============================================================
 * نسخة "عرض" من بيانات كل نوع عقدة، جانب المحرك نفسه (aarch64) -
 * منفصلة عمداً عن node_interface_t الحقيقية بمجلد src/nodes/
 * (تلك تترجم بمترجم PS2 فقط، وما تصل لكود المحرك إطلاقاً).
 *
 * تُستخدم من أي واجهة تحتاج تعرض معلومات عن نوع عقدة: اسمه،
 * أيقونته، وقائمة خصائصه (لبانل الخصائص).
 *
 * بياناتها **مولَّدة تلقائياً وقت البناء** (node_registry_generated.h)
 * من سطر @NODE بكل ملف src/nodes (ملفات .c) - بلا أي تعديل يدوي هنا
 * أو بـnode_registry.c. عقدة جديدة = ملف جديد بمجلد src/nodes فيه
 * سطر @NODE واحد، تظهر تلقائياً بأول بناء.
 * ============================================================
 */

#ifndef NODE_REGISTRY_H
#define NODE_REGISTRY_H

#include "node_types.h"
#include "nodes/node_interface.h" /* node_property_t وnode_property_type_t فقط - تعريفات C خام محايدة، بلا أي كود مترجم PS2 */

typedef struct {
    node_type_t type;
    const char *name;
    int icon_id; /* أحد ثوابت ICON_* من icon_names.h */
    const node_property_t *properties;
    int property_count;
} node_registry_entry_t;

/* قيمة خاصية حيّة فعلية لنسخة عقدة معينة بالمشهد - نفس شكل
 * default_value بـnode_property_t بالضبط، لكن منفصلة عنه عمداً:
 * default_value قراءة فقط (مخطط النوع)، وهذي تتغيّر فعلياً بالتعديل
 * من لوحة الخصائص. مصفوفة منها (بطول property_count) تُخزَّن لكل
 * عقدة بالمشهد - عامة تماماً، فخاصية أكثر أو أقل بنوع عقدة تشتغل
 * بنفس الكود، بلا أي تعديل هنا */
typedef union {
    float f;
    int i;
    char *s; /* غير const (بعكس default_value.s) - مملوكة بالكامل
              * لكل نسخة عقدة، تُخصَّص وتُحرَّر بشكل مستقل */
} node_property_value_t;

/* يرجع بيانات العرض لنوع عقدة معين. NULL لو النوع غير مسجَّل */
const node_registry_entry_t *node_registry_get(node_type_t type);

/* عدد كل أنواع العقد المسجَّلة - لأي واجهة تحتاج تسرد كلها (نافذة
 * إضافة عقدة مثلاً) */
int node_registry_count(void);

/* يرجع بيانات العرض حسب الترتيب (0 لـ count-1) - بدل الحاجة لمعرفة
 * node_type_t مسبقاً */
const node_registry_entry_t *node_registry_get_by_index(int index);

#endif /* NODE_REGISTRY_H */
