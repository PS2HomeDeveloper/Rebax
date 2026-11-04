/*
 * ============================================================
 * ui_project_center.h
 * ============================================================
 * The start screen (Main Menu) - the projects list. Computes positions and sizes
 * of the three areas based on the agreed layout description:
 *   - a long top bar (search bar)
 *   - a large faded black box in the center/left (projects list)
 *   - a narrow faded gray bar on the right (buttons)
 *
 * Does not draw anything itself - it only computes coordinates and colors. Actual
 * rendering is the job of another library after choosing the rendering/windowing
 * backend.
 * ============================================================
 */

#ifndef UI_PROJECT_CENTER_H
#define UI_PROJECT_CENTER_H

#include "ui_theme.h"

/* Rect: top-left corner point + width and height */
typedef struct {
    int x, y, w, h;
} ui_rect_t;

/* --- Layout constants, in pixels - change them here only to modify the overall look --- */
#define UI_SEARCHBAR_HEIGHT   56  /* Top bar height (search bar) */
#define UI_RIGHT_PANEL_WIDTH  220 /* Width of the gray side bar (buttons) */
#define UI_PANEL_GAP          16  /* Gap between the black box and the gray bar */

/* Computes the rectangles of the three areas based on the current
 * application window size (window_w, window_h) - called whenever the
 * window is resized so the layout stays consistent with any screen size */
void ui_project_center_layout(int window_w, int window_h,
                               ui_rect_t *out_searchbar,
                               ui_rect_t *out_project_panel,
                               ui_rect_t *out_buttons_panel);

/* Draws the entire start screen (the two colored panels + the buttons on them)
 * based on the current window size - called once per frame from main.c
 * immediately after window_clear */
void ui_project_center_draw(int window_w, int window_h);

/* Frees any graphical resources loaded internally (button images, etc.) -
 * called only once on program exit, before window_shutdown */
void ui_project_center_shutdown(void);

#endif /* UI_PROJECT_CENTER_H */
