/*
 * ============================================================
 * export_dialog.h
 * ============================================================
 * Project export settings window for PlayStation 2 - executable name,
 * save path (manual entry or Browse button via file_manager), Debug/Release
 * selection, and an Export button that starts ps2_exporter.h. During export,
 * the same window switches to a live output panel (one line per actual background
 * operation) instead of the settings fields - same window, different content depending on state.
 * ============================================================
 */

#ifndef EXPORT_DIALOG_H
#define EXPORT_DIALOG_H

void export_dialog_open(void);
int  export_dialog_is_open(void);

void export_dialog_update(int window_w, int window_h);
void export_dialog_draw(int window_w, int window_h);
void export_dialog_shutdown(void);

#endif /* EXPORT_DIALOG_H */
