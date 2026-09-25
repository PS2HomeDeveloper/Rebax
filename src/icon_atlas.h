/*
 * ============================================================
 * icon_atlas.h
 * ============================================================
 * يحمّل أطلس أيقونات المحرك المضمّن (صورة واحدة فيها كل أيقونة
 * بالمحرك، 256x256، شبكة 16x16 خانة)، ويوفر لأي ملف رسم أي أيقونة
 * بمجرد معرفة رقمها - والرقم نفسه يجيك جاهزاً من icon_names.h
 * المولَّد تلقائياً وقت البناء (مثال: ICON_Folder, ICON_add).
 *
 * لا حاجة لتعديل هذا الملف أبداً عند إضافة أيقونة جديدة - فقط
 * حط ملف PNG جديد بمجلد resources/images/icons/icons_src/، وابنِ.
 * ============================================================
 */

#ifndef ICON_ATLAS_H
#define ICON_ATLAS_H

#include "icon_names.h" /* مولَّد تلقائياً - يعرّف ICON_<اسم الملف> لكل أيقونة */

/* يحمّل الأطلس المضمّن (مرة واحدة فقط). يرجع 1 عند النجاح و0 عند
 * الفشل. يجب استدعاؤه قبل أي استدعاء لدوال الرسم أدناه */
int icon_atlas_init(void);

/* يرسم أيقونة وحدة (استخدم ثوابت ICON_* من icon_names.h)، بحجم
 * مطلوب (يُمدَّد تلقائياً لو الحجم يخالف 16x16 الأصلي) */
void icon_atlas_draw(int icon_id, int x, int y, int size);

/* نفس icon_atlas_draw لكن يدور الأيقونة حول مركزها - يستخدم لسهم
 * فتح/إغلاق شجرة الملفات والعقد (0 مغلق، 90 مفتوح) */
void icon_atlas_draw_rotated(int icon_id, int x, int y, int size, double angle_degrees);

/* نفس icon_atlas_draw لكن بتلوين إضافي (زي window_draw_texture_tinted) */
void icon_atlas_draw_tinted(int icon_id, int x, int y, int size,
                             unsigned char r, unsigned char g, unsigned char b);

/* يحرر الأطلس المحمَّل - يُستدعى مرة واحدة عند إغلاق البرنامج */
void icon_atlas_shutdown(void);

#endif /* ICON_ATLAS_H */
