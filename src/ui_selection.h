/*
 * ============================================================
 * ui_selection.h
 * ============================================================
 * صور التحديد الأزرق (المحدد) والتمرير فوق العنصر (hover) الموحدة
 * لكل القوائم والأشجار والتبويبات. تُنشأ مرة واحدة بالحجم المطلوب
 * ثم تُرسم بـwindow_draw_texture، وتُحرَّر بـwindow_destroy_texture.
 * ============================================================
 */

#ifndef UI_SELECTION_H
#define UI_SELECTION_H

#include "window.h"

/* تظليل خفيف عند مرور الماوس. يرجع NULL عند الفشل */
window_texture_t *ui_selection_make_hover(int w, int h, int corner_radius);

/* تظليل أقوى للعنصر المحدد. يرجع NULL عند الفشل */
window_texture_t *ui_selection_make_selected(int w, int h, int corner_radius);

#endif /* UI_SELECTION_H */
