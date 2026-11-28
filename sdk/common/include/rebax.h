/*
 * Rebax Engine public native API.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */
#ifndef REBAX_H
#define REBAX_H
#ifdef __cplusplus
extern "C" {
#endif

typedef struct { float x, y; } rebax_vec2_t;
typedef struct { rebax_vec2_t position, scale; float rotation; } rebax_transform2d_t;
typedef struct rebax_image_t rebax_image_t;

void rebax_log(const char *message);
float rebax_delta_time(void);
int rebax_key_down(int key);
int rebax_key_pressed(int key);
int rebax_find_node(const char *name);
int rebax_node_get_transform2d(int node, rebax_transform2d_t *out_transform);
int rebax_node_set_transform2d(int node, const rebax_transform2d_t *transform);
int rebax_node_set_visible(int node, int visible);
int rebax_node_is_visible(int node);
int rebax_node_get_float(int node, const char *property, float *out_value);
int rebax_node_set_float(int node, const char *property, float value);
int rebax_node_get_int(int node, const char *property, int *out_value);
int rebax_node_set_int(int node, const char *property, int value);
rebax_image_t *rebax_image_load(const char *asset_path);
void rebax_image_free(rebax_image_t *image);
int rebax_image_draw(rebax_image_t *image, float x, float y, float width, float height);
void rebax_quit(void);

void rebax_game_start(void);
void rebax_game_update(float delta_time);
void rebax_game_stop(void);

#ifdef __cplusplus
}
#endif
#endif
