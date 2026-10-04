/*
 * ============================================================
 * editor_workspace.h
 * ============================================================
 * Overall structure of the development interface: five dock areas (top-right, bottom-right,
 * top-left, bottom-left, bottom-center) around a central camera area, separated
 * by draggable borders to resize. Contains no actual panels yet -
 * just the layout, colors, and interaction (border dragging).
 * ============================================================
 */

#ifndef EDITOR_WORKSPACE_H
#define EDITOR_WORKSPACE_H

/* The type of development camera currently shown in the central camera area -
 * 2D (flat world, viewport_2d) or 3D (true perspective world, viewport_3d). In the future this will be determined
 * automatically based on the type of scene actually opened in the project - currently defaulted to
 * 3D on engine startup, manually switchable via
 * editor_workspace_set_camera_mode until an actual selection UI exists */
typedef enum {
    EDITOR_CAMERA_MODE_2D,
    EDITOR_CAMERA_MODE_3D
} editor_camera_mode_t;

/* Toggles the type of camera shown in the camera area - will be called in the future from the scene-type selection/opening logic (not present yet) */
void editor_workspace_set_camera_mode(editor_camera_mode_t mode);
void editor_workspace_activate_script(void);

/* Updates drag state (is the user currently grabbing a border) - called once per frame before drawing */
void editor_workspace_update(int window_w, int window_h);

/* Renders the entire workspace layout: the five areas + the camera area + the separating borders between them */
void editor_workspace_draw(int window_w, int window_h);

/* No resources loaded currently - present to keep lifecycle pattern stable */
void editor_workspace_shutdown(void);

#endif /* EDITOR_WORKSPACE_H */
