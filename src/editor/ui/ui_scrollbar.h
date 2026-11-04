/*
 * ============================================================
 * ui_scrollbar.h
 * ============================================================
 * Unified scrollbar for all engine panels (properties, node tree, file tree...).
 * Each panel keeps two variables of its own (current scroll offset and whether the
 * scrollbar is being dragged) and passes them to these functions instead of
 * reimplementing scrolling logic from scratch.
 *   - Mouse wheel over the panel: scroll by wheel_step pixels per tick.
 *   - Drag the scrollbar with the left mouse button.
 * ============================================================
 */

#ifndef UI_SCROLLBAR_H
#define UI_SCROLLBAR_H

#define UI_SCROLLBAR_WIDTH 8

/* Called every frame from the panel's update function.
 *   scroll_offset: current scroll offset in pixels (modified)
 *   dragging:      1 if the scrollbar is currently being dragged (modified)
 *   x, w:          panel x position and width (scrollbar aligned to its right edge)
 *   viewport_y/h:  start and height of the visible list area
 *   content_h:     total height of all rows
 *   wheel_step:    pixels per mouse wheel tick (usually the row height) */
void ui_scrollbar_update(int *scroll_offset, int *dragging,
                          int x, int w, int viewport_y, int viewport_h,
                          int content_h, int wheel_step);

/* Draws the scrollbar (background + handle) - called from the panel's draw
 * function after all rows have been rendered */
void ui_scrollbar_draw(int scroll_offset, int x, int w,
                        int viewport_y, int viewport_h, int content_h);

#endif /* UI_SCROLLBAR_H */
