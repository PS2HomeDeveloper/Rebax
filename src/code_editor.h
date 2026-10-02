/* code_editor.h - محرر الأكواد المدمج لملفات C/C++ والهيدرات. */
#ifndef CODE_EDITOR_H
#define CODE_EDITOR_H

/* يُستدعى فقط عندما يكون تبويب Script هو التبويب النشط. */
void code_editor_update(int x, int y, int w, int h);
void code_editor_draw(int x, int y, int w, int h);
void code_editor_set_active(int active);
void code_editor_shutdown(void);

#endif /* CODE_EDITOR_H */
