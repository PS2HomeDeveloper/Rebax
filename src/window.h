/*
 * ============================================================
 * window.h
 * ============================================================
 * مكتبة النافذة: تغلّف تفاصيل SDL2 وتوفر دوال بسيطة لفتح نافذة،
 * الرسم فيها، والتقاط أحداث الإغلاق. أي مكتبة أخرى تحتاج ترسم
 * على الشاشة تستخدم هذه الدوال، بدون ما تتعامل مع SDL2 مباشرة.
 * ============================================================
 */

#ifndef WINDOW_H
#define WINDOW_H

/* يفتح نافذة بعنوان وأبعاد محددة، ويهيئ محرك الرسم الداخلي.
 * يرجع 1 عند النجاح و0 عند الفشل (مثلاً: تعذر الاتصال بنظام
 * العرض - يطبع سبب الخطأ في هذي الحالة) */
int window_init(const char *title, int width, int height);

/* يرجع 1 إذا طلب المستخدم إغلاق النافذة (مثلاً بالضغط على زر
 * الإغلاق)، يُستخدم كشرط توقف حلقة الرسم الرئيسية */
int window_should_close(void);

/* يتحقق من أحداث النظام الجديدة (إغلاق، ضغط ماوس لاحقاً، إلخ)
 * ويحدّث الحالة الداخلية للنافذة - يُستدعى مرة كل إطار (frame) */
void window_poll_events(void);

/* يمسح كامل النافذة بلون واحد - أول خطوة في كل إطار رسم جديد */
void window_clear(unsigned char r, unsigned char g, unsigned char b);

/* يرسم مستطيل مملوء بلون واحد صلب، بإحداثيات وأبعاد محددة */
void window_fill_rect(int x, int y, int w, int h,
                       unsigned char r, unsigned char g, unsigned char b);

/* نوع مبهم (opaque) يمثّل صورة محمّلة داخل الذاكرة الرسومية (GPU) -
 * لا يحتاج أي كود خارج window.c يعرف تفاصيله */
typedef struct window_texture window_texture_t;

/* ينشئ صورة قابلة للرسم من بيانات بكسلات RGBA خام (مثل الناتج من
 * button_render). يُستدعى مرة واحدة فقط لكل صورة - النتيجة تُخزَّن
 * وتُعاد رسمها كل إطار بدون إعادة إنشائها */
window_texture_t *window_create_texture(const unsigned char *rgba_pixels,
                                         int width, int height);

/* يرسم صورة تم إنشاؤها مسبقاً بإحداثيات وأبعاد محددة */
void window_draw_texture(window_texture_t *tex, int x, int y, int w, int h);

/* نفس window_draw_texture لكن يضرب لون النسيج بلون إضافي (تعديل
 * لون - color modulation). مفيد لإعادة استخدام نفس نسيج نص أبيض
 * (من font.c) بأي لون آخر (أحمر لخطأ، أخضر لنجاح...) بدون توليد
 * نسيج جديد بلون مختلف من الصفر */
void window_draw_texture_tinted(window_texture_t *tex, int x, int y, int w, int h,
                                 unsigned char r, unsigned char g, unsigned char b);

/* يرسم جزءاً (مستطيل فرعي) بس من نسيج أكبر - أساسي لأطلسات الأيقونات
 * (نسيج واحد كبير فيه عدة أيقونات، نسحب وحدة بس كل مرة). src_* تحدد
 * منطقة القص من النسيج المصدر، dst_* أين وبأي حجم تُرسم بالنافذة */
void window_draw_texture_region(window_texture_t *tex,
                                 int src_x, int src_y, int src_w, int src_h,
                                 int dst_x, int dst_y, int dst_w, int dst_h);

/* نفس window_draw_texture_region لكن مع تلوين إضافي */
void window_draw_texture_region_tinted(window_texture_t *tex,
                                        int src_x, int src_y, int src_w, int src_h,
                                        int dst_x, int dst_y, int dst_w, int dst_h,
                                        unsigned char r, unsigned char g, unsigned char b);

/* نفس window_draw_texture_region لكن مع تدوير حول مركز الصورة -
 * angle_degrees بالساعة (0 = بدون تدوير، 90 = ربع دورة مع اتجاه
 * عقارب الساعة). أساسي لسهم فتح/إغلاق شجرة الملفات والعقد */
void window_draw_texture_region_rotated(window_texture_t *tex,
                                         int src_x, int src_y, int src_w, int src_h,
                                         int dst_x, int dst_y, int dst_w, int dst_h,
                                         double angle_degrees);

/* يحرر صورة تم إنشاؤها بـ window_create_texture */
void window_destroy_texture(window_texture_t *tex);

/* يعرض كل ما رُسم في هذا الإطار فعلياً على الشاشة - آخر خطوة
 * في كل إطار رسم (بدونها الرسم يبقى غير مرئي) */
void window_present(void);

/* العرض والارتفاع الحاليين للنافذة (يتغيران لو المستخدم غيّر
 * حجمها) - تُستخدم لحساب التخطيط في ui_project_center */
int window_get_width(void);
int window_get_height(void);

/* ============================================================
 * الماوس
 * ============================================================ */

/* الموقع الحالي للمؤشر داخل النافذة */
int window_mouse_x(void);
int window_mouse_y(void);

/* 1 فقط في الإطار اللي حصل فيه الضغط (مو طول مدة الضغط) - مناسب
 * تماماً لاكتشاف "ضغط زر" بدون تكرار التنفيذ كل إطار */
int window_mouse_left_just_pressed(void);

/* 1 طوال مدة بقاء الزر مضغوطاً (من لحظة الضغط لحد الإفلات) -
 * مناسب للسحب المستمر (زي تحريك حد فاصل بين نافذتين) */
int window_mouse_left_down(void);

/* نفس المنطق (just_pressed / down) لكن للزرين الأيمن والأوسط -
 * أضيفا لأجل تحكم كاميرا التطوير (نظر/طيران بالزر الأيمن، تحريك
 * جانبي Pan بالزر الأوسط) - غير مستخدمين بأي مكان ثانٍ بالواجهة */
int window_mouse_right_just_pressed(void);
int window_mouse_right_down(void);
int window_mouse_middle_just_pressed(void);
int window_mouse_middle_down(void);

/* حركة عجلة الماوس بهذا الإطار بس (موجب = بعيد عن المستخدم/تكبير
 * حسب السياق، سالب = العكس) - تصفر كل إطار جديد تلقائياً */
int window_mouse_wheel_delta(void);

/* الحركة النسبية للماوس بهذا الإطار بس (فرق x/y الفعلي، بغض النظر
 * عن حدود الشاشة) - مفيدة لنظر الكاميرا الحر (mouse-look) بدل
 * الاعتماد على الموقع المطلق اللي يتوقف عند حافة الشاشة */
int window_mouse_delta_x(void);
int window_mouse_delta_y(void);

/* وضع الحركة النسبية (Relative Mouse Mode) - يخفي المؤشر ويقفله
 * بمنتصف النافذة، فيرجع window_mouse_delta_x/y فرقاً حراً بلا حد
 * أقصى (بدل ما يتوقف عند حافة الشاشة). يُفعَّل فقط طول مدة نظر
 * الكاميرا الفعلي (مثلاً وقت الضغط المستمر على الزر الأيمن) */
void window_set_relative_mouse_mode(int enabled);

/* ============================================================
 * الوقت
 * ============================================================ */

/* الوقت بالثواني منذ آخر إطار (Delta Time) - أساسي لأي حركة
 * مستقلة عن معدل الإطارات (كاميرا التطوير الحرة مثلاً) */
float window_get_delta_time(void);

/* ============================================================
 * لوحة المفاتيح ونظام الكتابة النصية
 * ============================================================
 * لازم تستدعي window_start_text_input قبل ما تقدر تستقبل أي حرف
 * مكتوب، ووwindow_stop_text_input لما تنتهي (مثلاً عند إغلاق خانة
 * كتابة) - هذا يطابق نظام SDL2 نفسه، ويمنع النظام يفتح لوحة مفاتيح
 * افتراضية على الأجهزة اللمسية إلا وقت الحاجة الفعلية.
 */
void window_start_text_input(void);
void window_stop_text_input(void);

/* أي نص كتبه المستخدم هذا الإطار بالذات (فارغ "" لو ما كتب شيء) -
 * يدعم UTF-8، صالح فقط لحدود هذا الإطار الواحد */
const char *window_text_input_this_frame(void);

/* مفاتيح تحكم شائعة - تُرجع 1 فقط في إطار الضغطة الأولى (مو تكرار
 * كل إطار طول مدة الضغط) */
int window_key_just_pressed_backspace(void);
int window_key_just_pressed_delete(void);
int window_key_just_pressed_left(void);
int window_key_just_pressed_right(void);
int window_key_just_pressed_home(void);
int window_key_just_pressed_end(void);

/* اختصارات النسخ/اللصق/القص (Ctrl+C / Ctrl+V / Ctrl+X) - كل واحد
 * 1 فقط في إطار الضغطة الأولى */
int window_key_just_pressed_copy(void);
int window_key_just_pressed_paste(void);
int window_key_just_pressed_cut(void);

/* ============================================================
 * مفاتيح تحكم كاميرا التطوير (Fly Camera) - W/A/S/D حركة أفقية،
 * Q/E حركة رأسية، Shift تسريع مؤقت. كلها "down" مستمر (مو
 * just_pressed) لأن الحركة تحتاج تستمر طول مدة الضغط، بعكس
 * اختصارات الكتابة أعلاه. F تأطير العقدة المحددة - ضغطة واحدة بس
 * فيُكتفى بـjust_pressed مثل بقية مفاتيح الاختصار
 * ============================================================ */
int window_key_down_w(void);
int window_key_down_a(void);
int window_key_down_s(void);
int window_key_down_d(void);
int window_key_down_q(void);
int window_key_down_e(void);
int window_key_down_shift(void);
int window_key_just_pressed_f(void);

/* ============================================================
 * الحافظة (Clipboard)
 * ============================================================ */

/* يرجع نص الحافظة الحالي - مؤشر يجب تحريره بـ free() بعد الاستخدام،
 * أو NULL لو الحافظة فارغة أو حصل خطأ */
char *window_get_clipboard_text(void);

/* يضع نصاً في الحافظة */
void window_set_clipboard_text(const char *text);

/* ============================================================
 * قص الرسم (Clipping) - لإخفاء أي جزء من نص يتجاوز حدود خانة
 * الكتابة بدل ما يبين طافياً خارجها
 * ============================================================ */
void window_set_clip_rect(int x, int y, int w, int h);
void window_clear_clip_rect(void);

/* يغلق النافذة ويحرر كل موارد SDL2 - يُستدعى مرة واحدة عند
 * إنهاء البرنامج */
void window_shutdown(void);

#endif /* WINDOW_H */
