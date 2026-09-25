/*
 * ============================================================
 * element_2d.c (nodes_editor)
 * ============================================================
 * التمثيل البصري لـElement2D بمحرر التطوير - نقطة صغيرة بموضعها
 * الحقيقي (Position X/Y)، وعلامة تحديد إضافية (زائد أخضر) لو كانت
 * محددة حالياً بشجرة المشهد.
 *
 * الترتيب [0]=Position X، [1]=Position Y ثابت هنا لأن هذا الملف هو
 * نفسه المكان اللي يفسّر ترتيب خصائص Element2D - لا علاقة له بأي
 * تعديل مستقبلي على خصائص عقد أخرى.
 * ============================================================
 */

#include "node_editor_interface.h"
#include "viewport_2d.h"
#include "window.h"

#define MARKER_HALF        3  /* نصف عرض المربع الصغير اللي يمثّل موضع العقدة */
#define SELECT_CROSS_HALF  6  /* نصف طول ذراع علامة التحديد (الزائد) */

void element2d_editor_draw(const node_property_value_t *values, int property_count,
                            int cam_x, int cam_y, int cam_w, int cam_h, int selected) {
    if (property_count < 2) return; /* الحد الأدنى: Position X وY */

    int sx, sy;
    viewport_2d_project(values[0].f, values[1].f, cam_x, cam_y, cam_w, cam_h, &sx, &sy);

    /* نقطة بيضاء خفيفة تدل على مكان العقدة دائماً - بغض النظر عن التحديد */
    window_fill_rect(sx - MARKER_HALF, sy - MARKER_HALF, MARKER_HALF * 2, MARKER_HALF * 2, 225, 225, 225);

    if (selected) {
        window_fill_rect(sx - SELECT_CROSS_HALF, sy - 1, SELECT_CROSS_HALF * 2, 2, 70, 220, 120);
        window_fill_rect(sx - 1, sy - SELECT_CROSS_HALF, 2, SELECT_CROSS_HALF * 2, 70, 220, 120);
    }
}

/* @NODE_EDITOR type=NODE_TYPE_ELEMENT_2D draw_2d=element2d_editor_draw */
