/*
 * ============================================================
 * viewport_2d.h
 * ============================================================
 * Renders the virtual development world for a 2D scene: background, a grid of
 * gray lines, and the X (red) and Y (green) axes intersecting at the world origin (0,0).
 * Free camera: pan with the middle mouse button, and zoom with the mouse wheel
 * centered on the cursor position (same behavior as Figma/Photoshop/Godot 2D - the
 * element under the cursor stays in place when zooming, not the screen center).
 * ============================================================
 */

#ifndef VIEWPORT_2D_H
#define VIEWPORT_2D_H

/* Updates the free 2D camera state (Pan offset + Zoom level)
 * based on this frame's mouse input - receives the current camera area rectangle
 * (x, y, w, h) in window coordinates because middle-button panning must start inside it.
 * Called once per frame from editor_workspace_update when the 2D camera is active, before drawing */
void viewport_2d_update(int x, int y, int w, int h);
void viewport_2d_focus_scene(void);

/* Draws the entire 2D development world inside the camera area rectangle
 * (x, y, w, h) in window coordinates - called every frame from
 * editor_workspace_draw when the 2D camera is selected */
void viewport_2d_draw(int x, int y, int w, int h);

/* Projects a point in the 2D scene world space (units = pixels, origin at the
 * center of the camera area) to actual screen coordinates inside the current camera
 * rectangle (cam_x, cam_y, cam_w, cam_h) - the clean entry point for any node
 * (Element2D or any future subtype) that needs to draw itself at the correct
 * position in the camera area instead of redoing projection calculations. */
void viewport_2d_project(float world_x, float world_y,
                          int cam_x, int cam_y, int cam_w, int cam_h,
                          int *out_screen_x, int *out_screen_y);

void viewport_2d_unproject(int screen_x, int screen_y,
                            int cam_x, int cam_y, int cam_w, int cam_h,
                            float *out_world_x, float *out_world_y);
float viewport_2d_get_zoom(void);

#endif /* VIEWPORT_2D_H */
