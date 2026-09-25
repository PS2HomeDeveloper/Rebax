/*
 * ============================================================
 * rebax_fs.h
 * ============================================================
 * عمليات ملفات/مجلدات أساسية مكتوبة من الصفر (POSIX + Win32) - بلا
 * أي اعتماد على أوامر نظام خارجية (mkdir/rm/cp/find/basename كأوامر
 * shell). هذا يزيل ثغرة الاعتماد على أدوات Unix قد لا تكون متوفرة
 * إطلاقاً على أندرويد/ويندوز الحقيقيين وقت تشغيل المحرك عند المستخدم
 * النهائي - كل استدعاء نظام هنا عبر API نظام التشغيل مباشرة
 * (CreateDirectoryA/RemoveDirectoryA على ويندوز، mkdir()/unlink()
 * POSIX بغيره)، بلا fork/exec لأي عملية خارجية إطلاقاً.
 *
 * صيغة .tar.xz (فك الأرشيفات المُضمَّنة) وتشغيل مترجم/رابط PS2 نفسه
 * ملفان منفصلان (rebax_archive.h وتعديل ps2_exporter.c ليشغّل
 * المترجم مباشرة بدل Makefile/make) - خارج نطاق هذا الملف بالذات.
 * ============================================================
 */

#ifndef REBAX_FS_H
#define REBAX_FS_H

/* ينشئ مجلداً وكل المجلدات الأصل الناقصة (نفس mkdir -p) - يرجع 1
 * لو نجح أو كان موجوداً أصلاً، 0 لو فشل فعلياً */
int rebax_fs_mkdir_p(const char *path);

/* يحذف ملفاً أو مجلداً (وكل محتواه، بأي عمق) - نفس rm -rf. يرجع 1
 * لو نجح أو كان المسار غير موجود أصلاً (لا خطأ بهذي الحالة)، 0 لو
 * فشل حذف عنصر فعلي موجود */
int rebax_fs_remove_recursive(const char *path);

/* ينسخ ملفاً واحداً (محتوى خام، بايت ببايت) - ينشئ/يستبدل الوجهة.
 * يرجع 1 لو نجح، 0 لو فشل (مصدر غير موجود، أو تعذّرت الكتابة) */
int rebax_fs_copy_file(const char *src_path, const char *dst_path);
int rebax_fs_move(const char *src_path, const char *dst_path);

/* هل المسار موجود فعلاً (ملف أو مجلد)؟ */
int rebax_fs_exists(const char *path);

/* هل المسار مجلد فعلاً؟ (0 لو غير موجود أو ملف عادي) */
int rebax_fs_is_dir(const char *path);

/* يمرّ على كل ملف داخل dir_path (بأي عمق - يدخل المجلدات الفرعية
 * تلقائياً)، وينادي callback بمساره الكامل لكل ملف (نفس find). لا
 * ينادي callback على المجلدات نفسها، فقط الملفات */
typedef void (*rebax_fs_walk_callback_t)(const char *file_path, void *user_data);
void rebax_fs_walk_files(const char *dir_path, rebax_fs_walk_callback_t callback, void *user_data);

/* يستخرج اسم الملف الأخير من مسار كامل (نفس basename) - out يجب
 * يكون سعته كافية (استخدم نفس سعة path على الأقل) */
void rebax_fs_basename(const char *path, char *out, int out_size);

/* نفس rebax_fs_basename لكن يشيل الامتداد كمان (".png" مثلاً) -
 * يشيل بس آخر نقطة فأي شيء بعدها */
void rebax_fs_basename_no_ext(const char *path, char *out, int out_size);

/* يرجع مؤشر لبداية الامتداد داخل path نفسه (بعد آخر نقطة، بلا
 * النقطة) - أو NULL لو ما فيه امتداد. لا ينسخ، يشاور جوا path نفسه */
const char *rebax_fs_extension(const char *path);

/* ------------------------------------------------------------
 * فك أرشيف .tar.xz كامل لمجلد وجهة - الدالة العامة الوحيدة اللي أي
 * ملف ثاني بالمشروع يحتاجها (نفس نتيجة "tar -xJf" حرفياً، بلا أي
 * فرق بالمخرجات). داخلياً: فك ضغط xz عبر مكتبة xz_embedded المُتبنَّاة
 * (نفس فاك ضغط xz الحقيقي المستخدَم بنواة لينكس نفسها - جودة/صحة
 * مطابقة تماماً، بلا أي تنازل)، ثم تحليل تنسيق tar (USTAR + امتداد
 * GNU longname للمسارات الطويلة) مكتوب من الصفر هنا (تنسيق tar
 * نفسه بسيط - كتل رأس ثابتة 512 بايت، لا شيء معقد يستدعي مكتبة
 * خارجية له).
 *
 * يرجع 1 لو نجح الفك بالكامل، 0 لو فشل (أرشيف تالف، مساحة قرص،
 * صيغة رأس tar غير مدعومة - PAX extended headers مو مدعومة حالياً،
 * ustar + GNU longname بس) */
int rebax_fs_extract_tar_xz(const char *xz_path, const char *dest_dir);

#endif /* REBAX_FS_H */
