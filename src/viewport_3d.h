/*
 * ============================================================
 * viewport_3d.h
 * ============================================================
 * Renders the editor's default 3D scene: a real perspective camera
 * (Perspective) - an actual matrix projection, not a fake layout - depicting
 * a ground grid at height zero (X/Z plane) indicating the world's floor,
 * and X (red) and Z (green) axes intersecting exactly at the world origin
 * (0,0,0). A free 'fly' camera - same philosophy as Unity/Unreal:
 * holding the right mouse button enables mouse-look + WASD/QE movement,
 * Shift to boost, the mouse wheel during that adjusts the flight speed itself,
 * and F to quickly frame the world origin.
 * ============================================================
 */

#ifndef VIEWPORT_3D_H
#define VIEWPORT_3D_H

/* Updates the fly camera state (position/view angles/speed) based on mouse and
 * keyboard input this frame - receives the current camera rectangle (x, y, w, h)
 * in window coordinates because starting mouse-look with the right button must
 * begin from inside it (afterwards it continues even if the cursor leaves temporarily,
 * same behavior as dragging splitters). Called once per frame from editor_workspace_update
 * when the 3D camera is active, before drawing */
void viewport_3d_update(int x, int y, int w, int h);

/* Draws the entire 3D editor world inside the camera rectangle
 * (x, y, w, h) in window coordinates - called each frame from
 * editor_workspace_draw when the 3D camera is selected */
void viewport_3d_draw(int x, int y, int w, int h);

/* Projects a point in the 3D scene world space to actual screen coordinates inside the current
 * camera rectangle (cam_x, cam_y, cam_w, cam_h), using the same perspective camera
 * that draws the grid - a clean entry point for any node (Element3D or any future subtype)
 * that needs to draw itself at the correct place in the camera area, instead of duplicating
 * projection math itself. Returns 1 if the point is in front of the camera (actually drawable),
 * and 0 if behind (not drawn - the projected coordinates are undefined in that case) */
int viewport_3d_project(float world_x, float world_y, float world_z,
                         int cam_x, int cam_y, int cam_w, int cam_h,
                         int *out_screen_x, int *out_screen_y);

#endif /* VIEWPORT_3D_H */
