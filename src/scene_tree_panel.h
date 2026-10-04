#ifndef SCENE_TREE_PANEL_H
#define SCENE_TREE_PANEL_H

/* This panel is responsible only for displaying the scene tree. Scene data lives in scene_data.h. */
void scene_tree_panel_update(int x, int y, int w, int h);
void scene_tree_panel_draw(int x, int y, int w, int h);
void scene_tree_panel_shutdown(void);

#endif /* SCENE_TREE_PANEL_H */
