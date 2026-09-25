/*
 * ============================================================
 * node_editor_interface.h
 * ============================================================
 * "النسخة المعاكسة" لـ nodes/node_interface.h - تلك تعرّف كيف تشتغل
 * العقدة فعلياً وقت تشغيل اللعبة على PS2 (كود يترجم بمترجم PS2
 * منفصل تماماً). هذي تعرّف كيف تُرسم/تُمثَّل نفس نوع العقدة بمحرر
 * التطوير نفسه (aarch64) - تمثيل تحرير بصري بس، بلا أي علاقة
 * بمنطق اللعبة الحقيقي أو أدائها.
 *
 * كل ملف src/nodes_editor/<اسم>.c يعرّف دالة رسم واحدة (لعالم 2D أو
 * 3D حسب نوعه، مو الاثنين)، ويصرّح عنها بسطر @NODE_EDITOR - يُكتشف
 * تلقائياً وقت البناء (node_editor_registry_generated.h)، بلا أي
 * قائمة يدوية.
 * ============================================================
 */

#ifndef NODE_EDITOR_INTERFACE_H
#define NODE_EDITOR_INTERFACE_H

#include "node_registry.h" /* node_property_value_t */

/* يرسم تمثيل نوع عقدة بعالم الـ2D بمحرر التطوير.
 * values/property_count: القيم الحقيقية الحية لهذي النسخة بالضبط،
 *   بنفس ترتيب properties[] بجدول node_registry لنفس النوع - كل
 *   ملف يفسّرها حسب ترتيبه الخاص (هو نفسه مصدر ذاك الترتيب أصلاً).
 * cam_x/y/w/h: مستطيل منطقة كاميرا الـ2D الحالي - يمرَّر مباشرة
 *   لـviewport_2d_project لتحويل أي إحداثي عالمي لموضع شاشة فعلي.
 * selected: 1 لو هذي العقدة محددة حالياً بشجرة المشهد (لرسم علامة
 *   تحديد إضافية) */
typedef void (*node_editor_draw_2d_fn)(const node_property_value_t *values, int property_count,
                                        int cam_x, int cam_y, int cam_w, int cam_h, int selected);

/* نفس الفكرة بالضبط لعالم الـ3D - عبر viewport_3d_project */
typedef void (*node_editor_draw_3d_fn)(const node_property_value_t *values, int property_count,
                                        int cam_x, int cam_y, int cam_w, int cam_h, int selected);

#endif /* NODE_EDITOR_INTERFACE_H */
