/* code_editor.h - embedded code editor for C/C++ files and headers. */
#ifndef CODE_EDITOR_H
#define CODE_EDITOR_H

/* Called only when the Script tab is the active tab. */
void code_editor_update(int x, int y, int w, int h);
void code_editor_draw(int x, int y, int w, int h);
void code_editor_set_active(int active);
int code_editor_open_path(const char *path);
void code_editor_shutdown(void);

#endif /* CODE_EDITOR_H */
