/*
 * ============================================================
 * image_loader.h
 * ============================================================
 * وحدة تحميل الصور المشتركة الوحيدة لأي عقدة PS2 تحتاج تحميل صورة
 * وعرضها عبر gsKit - Sprite2D حالياً، وأي عقدة مستقبلية غيرها
 * (زر بصورة، خلفية متحركة...) تنادي نفس الدوال، بلا تكرار منطق
 * اكتشاف الصيغة بكل ملف عقدة على حدة.
 *
 * الصيغ المدعومة: PNG, JPEG/JPG, BMP, TGA, TIFF/TIF (عبر gsKit
 * الجاهزة)، RAW (بلا هيدر - الأبعاد/الصيغة من المتصل)، TIM2/TM2
 * وTIM (فك تشفير يدوي كامل، مبني على مرجعين حقيقيين: ps2_tim2_tool.py
 * وps1_tim_tool.py - نفس الأدوات اللي تُنتج هالملفات لمشروعك).
 * غير مدعومة إطلاقاً: GIF (فخ تسمية - لا علاقة له بمكتبات GS
 * الداخلية بمعالج PS2 نفسه).
 *
 * ------------------------------------------------------------
 * ليش دالة منفصلة لكل صيغة (image_loader_load_png، _jpeg، _tim2...)
 * بدل دالة واحدة متفرّعة بس؟ - سؤال تصدير مهم، مو تفصيل أسلوب:
 *
 * وقت التصدير الفعلي (خطوة قادمة منفصلة)، الهدف صراحة إنتاج ملف
 * تنفيذي PS2 ما فيه أي منطق تحميل صيغة صور المشروع ما يستخدمها
 * إطلاقاً - لو المطور استخدم PNG وTIM2 بس بمشروعه، منطق JPEG/BMP/
 * TGA/TIFF/RAW ما يظهر بالملف التنفيذي الناتج نهائياً.
 *
 * هذا يتحقق عبر: ترجمة src/nodes بفلاجات -ffunction-sections وقت
 * التصدير (كل دالة بقسم منفصل بالـ.o)، ثم كود التصدير المولَّد
 * (خطوة قادمة، يفحص امتداد "Image Path" الفعلي بخصائص كل عقدة
 * بالمشاهد المُصدَّرة) يستدعي الدالة المحدَّدة مباشرة (مثلاً
 * image_loader_load_png فقط لو الصورة .png)، **بدل** الدالة العامة
 * المتفرّعة image_loader_load تحتها. لو استدعينا الدالة العامة
 * دائماً، كودها المُترجَم يحتوي استدعاءات حقيقية لكل الدوال (كل
 * فروع if/else)، فيصير مرجعاً لكل قسم منها - فـ--gc-sections
 * (حذف الأقسام غير المُشار لها) ما يقدر يحذف شي، لأن الاستدعاء
 * موجود فعلياً بالكود المُترجَم بغض النظر عن أي فرع ينفَّذ وقت
 * التشغيل الفعلي. الدالة العامة تحت باقية للاستخدام غير التصديري
 * (اختبار محلي، أدوات تطوير...) - بس التصدير الفعلي لازم يتجاوزها.
 * ============================================================
 */

#ifndef IMAGE_LOADER_H
#define IMAGE_LOADER_H

#include <gsKit.h>

int image_loader_load_png(GSGLOBAL *gsGlobal, GSTEXTURE *texture, const char *path);
int image_loader_load_jpeg(GSGLOBAL *gsGlobal, GSTEXTURE *texture, const char *path);
int image_loader_load_bmp(GSGLOBAL *gsGlobal, GSTEXTURE *texture, const char *path);
int image_loader_load_tga(GSGLOBAL *gsGlobal, GSTEXTURE *texture, const char *path);
int image_loader_load_tiff(GSGLOBAL *gsGlobal, GSTEXTURE *texture, const char *path);

/* بلا هيدر إطلاقاً - width/height/psm لازم المتصل يعرفها مسبقاً من
 * مكان ثانٍ (خصائص العقدة مثلاً)، بالضبط زي مثال gsKit الرسمي
 * (examples/textures/textures.c) */
int image_loader_load_raw(GSGLOBAL *gsGlobal, GSTEXTURE *texture, const char *path,
                           int width, int height, int psm);

/* فك تشفير يدوي كامل - التخطيط الثنائي مأخوذ حرفياً من ps2_tim2_tool.py
 * (دالتا build_tim2_file وbuild_picture_block) - يدعم كل الصيغ
 * الخمس اللي الأداة تنتجها (32/24/16-bit مباشر، وindexed 8/4-bit)
 * تلقائياً، لأن PSM يُقرأ من GsTex0 المخزَّن بالملف نفسه، مو مخمَّناً */
int image_loader_load_tim2(GSGLOBAL *gsGlobal, GSTEXTURE *texture, const char *path);

/* فك تشفير يدوي كامل لصيغة TIM الخاصة بـPS1 - التخطيط مأخوذ حرفياً
 * من ps1_tim_tool.py (دالة parse_tim الرسمية بالأداة، ودوال البناء
 * الأربعة convert_4bit/8bit/16bit/24bit). الأربع صيغ (4/8/16/24-بت)
 * تنتقل لصيغ GS المقابلة (T4/T8/CT16/CT24) بنسخ مباشر للبايتات بلا
 * أي تحويل بتات - تحققت بمقارنة دوال الترميز الحقيقية بالأداتين
 * حرفياً: صيغة 5551 وترتيب nibble الفهرسة متطابقان تماماً بين
 * PS1 وPS2 GS، والصيغ المباشرة (16/24-بت) بايتات خام غير معقَّدة */
int image_loader_load_tim(GSGLOBAL *gsGlobal, GSTEXTURE *texture, const char *path);

/* الدالة العامة المتفرّعة - تكتشف الصيغة من امتداد الملف وتنادي
 * الدالة المناسبة فوق. **التصدير الفعلي ما يستخدمها** - راجع الشرح
 * فوق. تبقى مفيدة لأي استخدام غير تصديري (اختبار محلي، أدوات) */
int image_loader_load(GSGLOBAL *gsGlobal, GSTEXTURE *texture, const char *path,
                       int raw_width, int raw_height, int raw_psm);

#endif /* IMAGE_LOADER_H */
