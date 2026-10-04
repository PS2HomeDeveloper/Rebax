/*
 * ============================================================
 * properties_panel.h
 * ============================================================
 * Displays the currently selected node's properties in scene_data - the
 * values shown and edited here are the live real values of the same node
 * instance in the scene (via scene_data_get_values), not a temporary
 * presentation copy - any change here is reflected immediately on the
 * same memory the viewport draws from.
 * ============================================================
 */

#ifndef PROPERTIES_PANEL_H
#define PROPERTIES_PANEL_H

/* Sets the node whose properties are displayed now by its real index in the scene tree
 * (not its type/name - a unique index that can distinguish two nodes with the same name) -
 * Called from scene_data when clicking any node in the tree */
void properties_panel_set_selected(int node_index);

/* Clears any current selection - the panel becomes empty */
void properties_panel_clear_selection(void);

/* Updates all value-edit fields (typing, focus, cursor...) and writes any
 * new value directly to the real node memory - called every frame before
 * drawing, even if there is no selection currently (it returns immediately then) */
void properties_panel_update(int x, int y, int w, int h);

void properties_panel_draw(int x, int y, int w, int h);
void properties_panel_shutdown(void);

#endif /* PROPERTIES_PANEL_H */
