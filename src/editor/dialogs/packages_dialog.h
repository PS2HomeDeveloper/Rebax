#ifndef PACKAGES_DIALOG_H
#define PACKAGES_DIALOG_H

void packages_dialog_open(void);
int packages_dialog_is_open(void);
void packages_dialog_update(int window_w, int window_h);
void packages_dialog_draw(int window_w, int window_h);
void packages_dialog_shutdown(void);

#endif
