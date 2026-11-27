/* Rebax public API compatibility layer for PS2. */
#include <stdio.h>
#include <stdlib.h>
#include "rebax.h"
struct rebax_image_t { unsigned char *pixels; int width, height; };
void rebax_log(const char *message) { if (message) printf("[rebax] %s\n", message); }
float rebax_delta_time(void) { return 0.016f; }
int rebax_key_down(int key) { (void)key; return 0; }
int rebax_key_pressed(int key) { (void)key; return 0; }
int rebax_find_node(const char *name) { (void)name; return -1; }
int rebax_node_get_transform2d(int node, rebax_transform2d_t *out) { (void)node; if (!out) return 0; *out=(rebax_transform2d_t){{0,0},{1,1},0}; return 1; }
int rebax_node_set_transform2d(int node, const rebax_transform2d_t *value) { (void)node; return value != NULL; }
int rebax_node_set_visible(int node, int visible) { (void)node; (void)visible; return 1; }
int rebax_node_is_visible(int node) { (void)node; return 1; }
rebax_image_t *rebax_image_load(const char *path) { (void)path; return NULL; }
void rebax_image_free(rebax_image_t *image) { free(image); }
int rebax_image_draw(rebax_image_t *image,float x,float y,float w,float h) { (void)image;(void)x;(void)y;(void)w;(void)h; return 0; }
void rebax_quit(void) { }
