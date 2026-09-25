/*
 * ============================================================
 * Rebax_Engine - main.c
 * ============================================================
 * هذا الملف هو المنظّم فقط (orchestrator).
 * لا يحتوي منطق فعلي بحد ذاته - وظيفته الوحيدة هي:
 *   1. تضمين هيدرات المكتبات (كل مكتبة = ملف .c + .h منفصل)
 *   2. استدعاء كل مكتبة في مكانها الصحيح (تهيئة / حلقة الرسم / تصدير)
 *
 * قاعدة المشروع: التعليقات بالعربية فقط، أي طباعة نتائج بالإنجليزية
 * فقط وعند الحاجة فقط.
 * ============================================================
 */

#include <stdio.h>

#include "window.h"
#include "ui_project_center.h"
#include "project_dialog.h"
#include "loading_screen.h"
#include "editor_workspace.h"
#include "app_state.h"
#include "rebax_paths.h" /* مجلد عمل ريباكس الثابت + إعداد أول تشغيل (استخراج ps2dev/node_sources) */
#include "asset_browser.h" /* مدير الملفات المدمج - يُفتح من أي مكان بالمحرك، يُرسم فوق كل شيء */

/* ------------------------------------------------------------
 * مستقبلاً: هنا تُضاف تضمينات مكتبات إضافية، مثلاً:
 * #include "export.h"
 * #include "project_manager.h"
 * ------------------------------------------------------------ */

int main(void) {
    /* مقاس أصغر من قبل، ليتناسب مع شاشات الجوال داخل Termux:X11 -
     * النافذة قابلة لتغيير الحجم لاحقاً بأي حال */
    if (!window_init("Rebax_Engine", 900, 560)) {
        return 1;
    }

    printf("Rebax_Engine started successfully.\n");

    /* أول تشغيل فقط (على سطح المكتب) - استخراج بيئة ps2dev وسورس
     * العقد لمجلد ريباكس الثابت (راجع rebax_paths.h). شاشة تحميل
     * بلا اختفاء تلقائي لحين اكتمال العملية الحقيقية - قد تأخذ وقتاً
     * ملموساً (أرشيف ps2dev ضخم)، ما نجمّد الواجهة أثناءها (غير
     * حاجزة بالكامل) لكن ما فيه شيء نعرضه غير هذي الشاشة لحد ما تخلص */
    int rebax_setup_needed = rebax_paths_is_setup_needed();
    if (rebax_setup_needed) {
        loading_screen_show_indefinite();
        rebax_paths_setup_start();
    }

    while (!window_should_close()) {
        window_poll_events();

        if (rebax_setup_needed && !rebax_paths_setup_done()) {
            loading_screen_update();
            /* اعرض الإطار أولاً؛ فك أرشيف ps2dev قد يستغرق وقتاً طويلاً،
             * وإذا بدأ قبل window_present() تظهر النافذة فارغة. */
            loading_screen_draw(window_get_width(), window_get_height());
            window_present();
            rebax_paths_setup_update();
            if (rebax_paths_setup_done()) {
                if (rebax_paths_setup_failed()) {
                    fprintf(stderr, "[rebax] WARNING: first-run setup failed - "
                                    "export won't work until this is resolved.\n");
                }
                loading_screen_hide();
            }
            continue;
        }

        loading_screen_update();

        /* لما شاشة التحميل تختفي تلقائياً، ننتقل لواجهة التطوير -
         * هذا الربط مؤقت (كل تحميل حالياً ينتهي بواجهة التطوير)،
         * لاحقاً يختلف حسب سبب التحميل (إنشاء/فتح/استيراد مشروع) */
        if (loading_screen_just_finished()) {
            app_state_set_screen(APP_SCREEN_EDITOR);
        }

        if (loading_screen_is_visible()) {
            /* شاشة التحميل تستحوذ على الشاشة بالكامل - لا نرسم
             * أي شاشة أخرى تحتها أثناء ظهورها */
            loading_screen_draw(window_get_width(), window_get_height());
        } else if (app_state_get_screen() == APP_SCREEN_EDITOR) {
            /* لا نحدّث واجهة التطوير (نقر/سحب/كاميرا) وقت فتح متصفح
             * الملفات فوقها - يبقى المستخدم يتفاعل مع نافذة واحدة
             * فقط بكل لحظة، لكنها تبقى مرسومة تحته (مظلَّلة) */
            if (!asset_browser_is_open()) {
                editor_workspace_update(window_get_width(), window_get_height());
            }
            editor_workspace_draw(window_get_width(), window_get_height());
        } else {
            window_clear(UI_COLOR_BLACK_MUTED.r, UI_COLOR_BLACK_MUTED.g, UI_COLOR_BLACK_MUTED.b);
            ui_project_center_draw(window_get_width(), window_get_height());

            /* نافذة إنشاء المشروع - تُحدَّث وتُرسم فوق كل شيء، فقط لو
             * مفتوحة (ونفس منطق تعطيل التحديث وقت فتح متصفح الملفات
             * فوقها - مثال: زر Browse بداخلها يفتح المتصفح) */
            if (project_dialog_is_open()) {
                if (!asset_browser_is_open()) {
                    project_dialog_update(window_get_width(), window_get_height());
                }
                project_dialog_draw(window_get_width(), window_get_height());
            }
        }

        /* متصفح الملفات المدمج (asset_browser) - فوق كل شيء آخر
         * دائماً، بغض النظر عن الشاشة المفتوحة حالياً، لأنه يُفتح من
         * أي مكان بالمحرك يحتاج اختيار ملف أو مجلد (خاصية عقدة، زر
         * Browse بنافذة إنشاء مشروع، إلخ) */
        if (asset_browser_is_open()) {
            asset_browser_update(window_get_width(), window_get_height());
            asset_browser_draw(window_get_width(), window_get_height());
        }

        window_present();
    }

    editor_workspace_shutdown();
    project_dialog_shutdown();
    ui_project_center_shutdown();
    asset_browser_shutdown();
    window_shutdown();
    return 0;
}
