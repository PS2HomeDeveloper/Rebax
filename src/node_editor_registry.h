/*
 * ============================================================
 * node_editor_registry.h
 * ============================================================
 * يربط كل نوع عقدة بدالة رسمها البصري بمحرر التطوير (لو وُجدت) -
 * بياناته **مولَّدة تلقائياً وقت البناء** (node_editor_registry_generated.h)
 * من سطر @NODE_EDITOR بكل ملف src/nodes_editor (ملفات .c)، بنفس
 * فلسفة node_registry.h تماماً. لا تعدّله يدوياً ولا node_editor_registry.c.
 * ============================================================
 */

#ifndef NODE_EDITOR_REGISTRY_H
#define NODE_EDITOR_REGISTRY_H

#include "node_types.h"
#include "node_editor_interface.h"

typedef struct {
    node_type_t type;
    node_editor_draw_2d_fn draw_2d; /* NULL لو النوع مو 2D أو بلا تمثيل بصري بعد */
    node_editor_draw_3d_fn draw_3d; /* NULL لو النوع مو 3D أو بلا تمثيل بصري بعد */
} node_editor_registry_entry_t;

/* يرجع دوال الرسم لنوع عقدة معين. NULL لو النوع غير مسجَّل هنا
 * إطلاقاً (عادي - يعني بلا أي تمثيل بصري مخصَّص بعد، الفيوبورت
 * يتجاهله بأمان) */
const node_editor_registry_entry_t *node_editor_registry_get(node_type_t type);

#endif /* NODE_EDITOR_REGISTRY_H */
