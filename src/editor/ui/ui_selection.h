/*
 * ============================================================
 * ui_selection.h
 * ============================================================
 * Unified blue selection (selected) and hover highlight images for all lists,
 * trees and tabs. Created once at the required size then drawn with window_draw_texture
 * and freed with window_destroy_texture.
 * ============================================================
 */

#ifndef UI_SELECTION_H
#define UI_SELECTION_H

#include "window.h"

/* Light hover shading. Returns NULL on failure */
window_texture_t *ui_selection_make_hover(int w, int h, int corner_radius);

/* Stronger shading for the selected item. Returns NULL on failure */
window_texture_t *ui_selection_make_selected(int w, int h, int corner_radius);

#endif /* UI_SELECTION_H */
