/*
 * ============================================================
 * close_scene_dialog.h
 * ============================================================
 * نافذة صغيرة منبثقة (Save / Don't Save / Cancel) تظهر عند محاولة
 * إغلاق تبويب مشهد فيه تعديلات غير محفوظة. لا تلمس scene_tabs ولا
 * current_scene بنفسها إطلاقاً - ترجّع بس قرار المستخدم، والمستدعي
 * (editor_workspace.c) يتصرف بناءً عليه (يفتح نافذة حفظ الملف لو
 * لازم، وينادي scene_tabs_close في النهاية).
 * ============================================================
 */

#ifndef CLOSE_SCENE_DIALOG_H
#define CLOSE_SCENE_DIALOG_H

typedef enum {
    CLOSE_SCENE_RESULT_NONE,       /* لسه المستخدم ما اختار شيء */
    CLOSE_SCENE_RESULT_SAVE,
    CLOSE_SCENE_RESULT_DONT_SAVE,
    CLOSE_SCENE_RESULT_CANCEL
} close_scene_result_t;

/* يفتح النافذة لتبويب معين (بالفهرس) - المستدعي مسؤول عن نقل
 * التبويب النشط لهذا الفهرس أولاً (scene_tabs_switch_to) قبل ما
 * ينادي هذي الدالة، عشان current_scene تعكس محتوى نفس هذا التبويب
 * فعلياً لو المستخدم اختار "Save" */
void close_scene_dialog_open(int tab_index);

int close_scene_dialog_is_open(void);

/* فهرس التبويب المرتبط بالنافذة المفتوحة حالياً - بلا معنى لو
 * close_scene_dialog_is_open() == 0 */
int close_scene_dialog_get_tab_index(void);

void close_scene_dialog_update(int window_w, int window_h);
void close_scene_dialog_draw(int window_w, int window_h);

/* يرجّع قرار المستخدم لو ضغط أي زر هذا الإطار (تُقرأ مرة وحدة ثم
 * تُصفَّر تلقائياً لداخل)، وإلا CLOSE_SCENE_RESULT_NONE. الضغط على
 * "Save" أو "Don't Save" أو "Cancel" يغلق النافذة نفسها فوراً
 * (is_open يرجع 0 بعدها) بغض النظر شو المستدعي سوّى بالنتيجة */
close_scene_result_t close_scene_dialog_consume_result(void);

void close_scene_dialog_shutdown(void);

#endif /* CLOSE_SCENE_DIALOG_H */
