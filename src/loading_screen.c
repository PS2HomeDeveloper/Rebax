/*
 * ============================================================
 * loading_screen.c
 * ============================================================
 * الفكرة: 8 نقاط موزعة بالتساوي على محيط دائرة (بحساب الزاوية
 * لكل نقطة عبر cos/sin - نفس رياضيات الدائرة القياسية). كل عدد
 * معين من الإطارات، النقطة "النشطة" تنتقل للي بعدها، فيبين إحساس
 * دوران - كل هذا بدون أي صورة أو مورد، مستطيلات صغيرة فقط.
 * ============================================================
 */

#include <math.h>

#include "loading_screen.h"
#include "window.h"
#include "ui_project_center.h" /* لأجل ألوان الثيم */

#define DOT_COUNT        8
#define DOT_SIZE         6   /* حجم النقطة العادية بالبكسل */
#define DOT_SIZE_ACTIVE  12  /* حجم النقطة النشطة (أكبر = تبين الحركة) */
#define ORBIT_RADIUS     34  /* نصف قطر الدائرة اللي تتوزع عليها النقاط */
#define FRAMES_PER_STEP  6   /* كل كم إطار تتحرك النقطة النشطة خطوة واحدة */
#define AUTO_HIDE_FRAMES 90  /* مدة ظهور الشاشة قبل الاختفاء التلقائي (~1.5 ثانية) -
                               * مؤقت فقط لحين ربطها بانتهاء عملية حقيقية لاحقاً */

static int g_visible = 0;
static int g_frame_counter = 0;
static int g_just_finished = 0;
static int g_auto_hide = 1; /* 0 أثناء انتظار عملية حقيقية (إعداد ريباكس أول تشغيل مثلاً) - main.c يتحكم بالإخفاء وقتها بنفسه */

void loading_screen_show(void) {
    g_visible = 1;
    g_frame_counter = 0;
    g_just_finished = 0;
    g_auto_hide = 1;
}

/* نفس loading_screen_show لكن بلا اختفاء تلقائي بعد مدة ثابتة -
 * تبقى ظاهرة لحد ما المتصل ينادي loading_screen_hide بنفسه (يستخدمها
 * main.c وقت انتظار rebax_paths_setup_* - مدة حقيقية متغيرة، مو
 * وقتاً وهمياً ثابتاً) */
void loading_screen_show_indefinite(void) {
    g_visible = 1;
    g_frame_counter = 0;
    g_just_finished = 0;
    g_auto_hide = 0;
}

void loading_screen_hide(void) {
    g_visible = 0;
}

int loading_screen_is_visible(void) {
    return g_visible;
}

/* علم "لمرة واحدة" - يرجع 1 مرة وحدة بس في الإطار اللي اختفت فيه
 * الشاشة تلقائياً، وبعدها يرجع 0 حتى لو استمريت تسأل. يقرأه main.c
 * ليعرف بالضبط متى ينتقل للشاشة التالية بدون تكرار الانتقال */
int loading_screen_just_finished(void) {
    if (g_just_finished) {
        g_just_finished = 0;
        return 1;
    }
    return 0;
}

void loading_screen_update(void) {
    if (!g_visible) {
        return;
    }
    g_frame_counter++;
    if (g_auto_hide && g_frame_counter >= AUTO_HIDE_FRAMES) {
        g_visible = 0;
        g_just_finished = 1;
    }
}

void loading_screen_draw(int window_w, int window_h) {
    if (!g_visible) {
        return;
    }

    /* الشاشة تدير خلفيتها بنفسها - استحواذ كامل على الشاشة أثناء الانتظار */
    window_clear(UI_COLOR_BLACK_MUTED.r, UI_COLOR_BLACK_MUTED.g, UI_COLOR_BLACK_MUTED.b);

    int cx = window_w / 2;
    int cy = window_h / 2;
    int active = (g_frame_counter / FRAMES_PER_STEP) % DOT_COUNT;

    for (int i = 0; i < DOT_COUNT; i++) {
        double angle = (2.0 * 3.14159265358979 * i) / DOT_COUNT;
        int dx = cx + (int)(cos(angle) * ORBIT_RADIUS);
        int dy = cy + (int)(sin(angle) * ORBIT_RADIUS);

        int is_active = (i == active);
        int size = is_active ? DOT_SIZE_ACTIVE : DOT_SIZE;

        window_fill_rect(dx - size / 2, dy - size / 2, size, size,
                          UI_COLOR_BUTTON_BLUE.r, UI_COLOR_BUTTON_BLUE.g, UI_COLOR_BUTTON_BLUE.b);
    }
}
