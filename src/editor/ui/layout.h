#ifndef UI_LAYOUT_H
#define UI_LAYOUT_H

typedef enum {
    UI_SPLITTER_IDLE,
    UI_SPLITTER_HOVER,
    UI_SPLITTER_ACTIVE
} ui_splitter_visual_t;

void ui_layout_draw_panel(int x, int y, int w, int h, int tab_height);
void ui_layout_draw_splitter(int x, int y, int w, int h, ui_splitter_visual_t state);

#endif /* UI_LAYOUT_H */
