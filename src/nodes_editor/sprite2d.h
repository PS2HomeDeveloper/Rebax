#ifndef REBAX_SPRITE2D_EDITOR_H
#define REBAX_SPRITE2D_EDITOR_H
#include "node_registry.h"
int sprite2d_editor_hit_test(const node_property_value_t *values, int property_count,
                              int screen_x, int screen_y, int pivot_x, int pivot_y,
                              float viewport_zoom);
#endif
