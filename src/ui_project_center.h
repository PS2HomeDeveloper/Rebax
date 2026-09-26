/*
 * ============================================================
 * ui_project_center.h
 * ============================================================
 * شاشة البداية (Main Menu) - قائمة المشاريع. تحسب مواقع وأبعاد
 * المناطق الثلاث بناءً على وصف التخطيط المتفق عليه:
 *   - شريط علوي طويل (شريط البحث)
 *   - مربع أسود باهت كبير في المنتصف/اليسار (قائمة المشاريع)
 *   - شريط رصاصي باهت ضيق على اليمين (الأزرار)
 *
 * لا يرسم أي شيء بنفسه - فقط يحسب الإحداثيات والألوان. الرسم
 * الفعلي مهمة مكتبة أخرى بعد اختيار مكتبة الرسم/النوافذ المناسبة.
 * ============================================================
 */

#ifndef UI_PROJECT_CENTER_H
#define UI_PROJECT_CENTER_H

/* لون واحد بصيغة RGBA */
typedef struct {
    unsigned char r, g, b, a;
} ui_color_t;

/* مستطيل: نقطة الزاوية العلوية اليسرى + العرض والارتفاع */
typedef struct {
    int x, y, w, h;
} ui_rect_t;

/* --- الألوان الثابتة، مقاسة مباشرة من المرجع اللوني المتفق عليه --- */
static const ui_color_t UI_COLOR_BLACK_MUTED  = {0x13, 0x13, 0x13, 0xFF}; /* #131313 */
static const ui_color_t UI_COLOR_GRAY_MUTED   = {0x42, 0x42, 0x42, 0xFF}; /* #424242 */
static const ui_color_t UI_COLOR_BUTTON_BLUE  = {0x3B, 0x5B, 0x74, 0xFF}; /* #3B5B74 */
static const ui_color_t UI_COLOR_VIEWPORT_BLACK = {0x00, 0x00, 0x00, 0xFF}; /* أسود خالص - كاميرا التطوير فقط */
static const ui_color_t UI_COLOR_PANEL_BG    = {0x38, 0x38, 0x38, 0xFF}; /* #383838 - خلفية النوافذ/البانلات الخمسة بواجهة التطوير */

/* --- ثوابت التخطيط، بالبكسل - عدّلها هنا فقط لتغيير الشكل العام --- */
#define UI_SEARCHBAR_HEIGHT   56  /* ارتفاع الشريط العلوي (شريط البحث) */
#define UI_RIGHT_PANEL_WIDTH  220 /* عرض الشريط الرصاصي الجانبي (الأزرار) */
#define UI_PANEL_GAP          16  /* الفراغ بين المربع الأسود والشريط الرصاصي */

/* يحسب مستطيلات المناطق الثلاث بناءً على أبعاد نافذة البرنامج
 * الحالية (window_w, window_h) - يُستدعى كل مرة يتغير فيها حجم
 * النافذة، عشان التخطيط يبقى متناسق مع أي مقاس شاشة */
void ui_project_center_layout(int window_w, int window_h,
                               ui_rect_t *out_searchbar,
                               ui_rect_t *out_project_panel,
                               ui_rect_t *out_buttons_panel);

/* يرسم كامل شاشة البداية (اللوحتين الملونتين + الأزرار الموجودة
 * عليها) بناءً على أبعاد النافذة الحالية - يُستدعى مرة كل إطار
 * من main.c بعد window_clear مباشرة */
void ui_project_center_draw(int window_w, int window_h);

/* يحرر أي موارد رسومية تم تحميلها داخلياً (صور الأزرار وغيرها) -
 * يُستدعى مرة واحدة فقط عند إنهاء البرنامج، قبل window_shutdown */
void ui_project_center_shutdown(void);

#endif /* UI_PROJECT_CENTER_H */
