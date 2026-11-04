#ifndef UI_DIALOG_H
#define UI_DIALOG_H

#include "window.h"

void ui_dialog_draw_backdrop(int window_w, int window_h);
void ui_dialog_draw_frame(int x, int y, int w, int h);
void ui_dialog_draw_title_left(window_texture_t *title, int title_w, int title_h,
                               int x, int y, int titlebar_height, int padding);
void ui_dialog_draw_title_centered(window_texture_t *title, int title_w, int title_h,
                                   int x, int y, int w, int titlebar_height);
void ui_dialog_draw_close_button(int x, int y, int size);
int ui_dialog_close_button_hit(int mouse_x, int mouse_y, int x, int y, int size);
void ui_dialog_shutdown(void);

#endif /* UI_DIALOG_H */
