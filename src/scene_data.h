/* scene_data.h - live scene data and saving, independent of any rendering interface. */
#ifndef SCENE_DATA_H
#define SCENE_DATA_H

#include "node_types.h"
#include "node_registry.h"

#define SCENE_DATA_MAX_NODES 128

void scene_data_add_node(node_type_t type, const char *name);
void scene_data_clear(void);
int scene_data_get_node_count(void);
node_type_t scene_data_get_type(int index);
const char *scene_data_get_name(int index);
node_property_value_t *scene_data_get_values(int index);
int scene_data_get_parent(int index);
int scene_data_has_children(int index);
int scene_data_is_expanded(int index);
void scene_data_toggle_expanded(int index);
int scene_data_build_visible(int *out_indices, int *out_depths, int max_items);
int scene_data_get_selected_index(void);
void scene_data_select_index(int index);
void scene_data_clear_selection(void);
void scene_data_add_to_selection(int index);
int scene_data_is_selected(int index);
void scene_data_set_float_property(int index, int property_index, float value);
int scene_data_serialize(char *buffer, int buffer_size);
int scene_data_deserialize(const char *buffer);
void scene_data_shutdown(void);

#endif /* SCENE_DATA_H */
