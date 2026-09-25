/*
 * ============================================================
 * rebax_paths.h
 * ============================================================
 * مسار عمل ريباكس الثابت خارج مجلد المشروع نهائياً - يحل مشكلة
 * صلاحيات الكتابة غير الموثوقة اللي تحصل لما نكتب بجانب الملف
 * التنفيذي نفسه أو داخل مجلد المشروع (خصوصاً بيئات زي Termux:X11 -
 * الصلاحيات هناك تعتمد على طريقة تشغيل الجلسة بالضبط، مو شيء
 * نتحكم فيه من كودنا). البديل: مسار قياسي معروف لكل نظام تشغيل،
 * بنفس المكان اللي تستخدمه أي برامج أخرى على نفس الجهاز - مضمون
 * الكتابة عليه دائماً:
 *
 *   Windows:  %LOCALAPPDATA%\Rebax\
 *   Linux:    ~/.local/share/Rebax/
 *   Android:  مسار تخزين التطبيق الداخلي (عبر SDL، بلا JNI يدوي)
 *   macOS:    مستقبلاً - بنفس الأسلوب بالضبط (مسار جذر واحد،
 *             نفس الشجرة تحته)
 *
 * الشجرة تحت الجذر (سطح المكتب - Windows/Linux/macOS):
 *   Rebax/
 *   ├── Engine/
 *   │   ├── toolchains/ps2dev/              ← يُستخرج من embedded_resources
 *   │   │                                     (ps2dev.tar.xz) أول مرة بس
 *   │   └── export_resources/node_sources/  ← يُستخرج من node_sources.tar.xz
 *   │                                         (كان اسمه "nodes" داخل
 *   │                                         الأرشيف - يُعاد تسميته هنا)
 *   ├── Temp/export/     ← مساحة عمل مؤقتة لكل عملية تصدير - تُنظَّف
 *   │                       بالكامل فور انتهاء كل تصدير (نجح أو فشل)
 *   └── Settings/         ← إعدادات المحرر (مستقبلاً)
 *
 * على أندرويد (APK): Engine/toolchains وEngine/export_resources
 * غير موجودتين إطلاقاً هنا - أرشيفا ps2dev/node_sources ينضمّان
 * لملف الـAPK نفسه بخطوة تغليف منفصلة (راجع Makefile)، وملف
 * التصدير يقرأهما من هناك مباشرة وقتها - خارج نطاق هذا الملف.
 * هنا فقط Temp/export وSettings.
 * ============================================================
 */

#ifndef REBAX_PATHS_H
#define REBAX_PATHS_H

/* المسار الجذر (Rebax/) على هذا الجهاز بالذات - يُنشئه لو غير موجود
 * (بما فيها كل الشجرة تحته المناسبة للمنصة الحالية). محسوب ومخزَّن
 * مرة وحدة بأول استدعاء - كل الاستدعاءات بعدها ترجع نفس القيمة
 * فوراً بلا أي عملية قرص إضافية */
const char *rebax_root_dir(void);

/* اختصارات مباشرة لمجلدات الشجرة الفرعية - كل وحدة ترجع مساراً
 * كاملاً جاهزاً (rebax_root_dir() + الجزء الخاص بها) */
const char *rebax_toolchains_dir(void);
const char *rebax_toolchain_dir(void);     /* .../Engine/toolchains/ps2dev - فاضٍ على أندرويد */
const char *rebax_make_path(void);
const char *rebax_node_sources_dir(void);  /* .../Engine/resources/nodes/node_sources - فاضٍ على أندرويد */
const char *rebax_export_resources_dir(void); /* .../Engine/export_resources */
const char *rebax_temp_export_dir(void);   /* .../Temp/export */
const char *rebax_settings_dir(void);      /* .../Settings */

/* هل لسه محتاجين نستخرج ps2dev/node_sources (سطح المكتب فقط - على
 * أندرويد ترجع دائماً 0، ما فيه شيء نستخرجه هنا) */
int rebax_paths_is_setup_needed(void);

/* يبدأ استخراج ps2dev.tar.xz + node_sources.tar.xz المُضمَّنين
 * بالملف التنفيذي نفسه (أول تشغيل فقط) - غير حاجز، حالة تتقدّم
 * بـrebax_paths_setup_update() كل إطار (نفس أسلوب shell_step
 * بـps2_exporter.c - عمليتا فك ضغط طويلتان، ما نجمّد الواجهة لهما) */
void rebax_paths_setup_start(void);
void rebax_paths_setup_update(void);
int  rebax_paths_setup_done(void);      /* نجح أو فشل - خلص بأي الحالتين */
int  rebax_paths_setup_failed(void);    /* فشل تحديداً - لعرض رسالة خطأ بدل المتابعة بصمت */

int rebax_export_template_is_required(void);
int rebax_export_template_is_installed(void);
int rebax_export_template_install(const char *archive_path);
int rebax_export_template_open_download_url(void);

#endif /* REBAX_PATHS_H */
