/*
 * ============================================================
 * image_loader_internal.h
 * ============================================================
 * دوال مساعدة مشتركة بين ملفات image_loader_*.c بعضها فقط - مو
 * جزء من الواجهة العامة (image_loader.h)، ومو شيء أي عقدة تحتاج
 * تعرفه. فصلها هنا (بدل تكرارها بكل ملف صيغة) لأن TIM2 وTIM كلاهما
 * يحتاجانها بالضبط: قراءة أعداد صحيحة little-endian من بايتات خام،
 * وإكمال رفع قوام مُفكَّك تشفيره يدوياً لذاكرة GS.
 * ============================================================
 */

#ifndef IMAGE_LOADER_INTERNAL_H
#define IMAGE_LOADER_INTERNAL_H

#include <gsKit.h>

/* يقرأ عدد صحيح little-endian من بايتات خام - نفس ترتيب البايتات
 * المستخدم بملفات TIM2/TIM (وأي صيغة PS2/PS1 أصلية ثانية مستقبلاً) */
unsigned int read_u32le(const unsigned char *p);
unsigned short read_u16le(const unsigned char *p);
unsigned long long read_u64le(const unsigned char *p);

/* يكمل رفع قوام مفكوك تشفيره يدوياً (TIM2/TIM) لذاكرة GS فعلياً -
 * نفس التسلسل الحرفي اللي gsKit تسويه داخلياً (gsKit_texture_finish
 * بملف gsToolkit.c الأصلي)، لكن مكتوب هنا بأنفسنا بدلالة الدوال
 * العامة المؤكدة فقط (gsKit_vram_alloc, gsKit_texture_size,
 * gsKit_texture_upload - كلها مُصرَّح عنها بـgsTexture.h/gsCore.h
 * العامين). عمداً ما نستدعي gsKit_texture_finish نفسها: هذي دالة
 * داخلية بمكتبة gsKit (extern معرَّفة داخل gsToolkit.c نفسه، بلا أي
 * تصريح بأي هيدر عام) - رغم إنها موجودة فعلياً بالمكتبة المبنية
 * (تحققت من CMakeLists.txt)، الاعتماد عليها مباشرة يعني ثبات كودنا
 * مرهون بتفصيل تنفيذي داخلي مو جزء من واجهة gsKit الرسمية المستقرة */
int finish_texture_upload(GSGLOBAL *gsGlobal, GSTEXTURE *texture);

#endif /* IMAGE_LOADER_INTERNAL_H */
