/*
 * ============================================================
 * export_dialog.h
 * ============================================================
 * نافذة إعدادات تصدير المشروع لبلاي ستيشن 2 - اسم الملف التنفيذي،
 * مسار الحفظ (كتابة يدوية أو زر تصفح عبر asset_browser)، اختيار
 * Debug/Release، وزر تصدير يبدأ ps2_exporter.h. أثناء التصدير،
 * تتحوّل نفس النافذة للوحة مخرجات حية (سطر بكل عملية فعلية تشتغل
 * بالخلفية) بدل حقول الإعدادات - نفس النافذة، محتوى مختلف حسب الحالة.
 * ============================================================
 */

#ifndef EXPORT_DIALOG_H
#define EXPORT_DIALOG_H

void export_dialog_open(void);
int  export_dialog_is_open(void);

void export_dialog_update(int window_w, int window_h);
void export_dialog_draw(int window_w, int window_h);
void export_dialog_shutdown(void);

#endif /* EXPORT_DIALOG_H */
