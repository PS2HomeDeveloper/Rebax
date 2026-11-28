/* Rebax Engine public native API implementation. */
/* @EXPORT_LIBS: -lSDL2 -lm */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL2/SDL.h>
#include "rebax.h"
#include "engine_context.h"
#include "engine_input.h"
#include "asset_image.h"
#include "scene_runtime.h"
#include "node_interface.h"

struct rebax_image_t { unsigned char *pixels; int width, height; SDL_Texture *texture; };
static int g_quit_requested;

void rebax_log(const char *message) { if (message) fprintf(stderr, "[rebax] %s\n", message); }
float rebax_delta_time(void) { return engine_context_delta_time(); }
int rebax_key_down(int key) { return key >= 0 && key < SDL_NUM_SCANCODES && engine_input_key_down((SDL_Scancode)key); }
int rebax_key_pressed(int key) { return key >= 0 && key < SDL_NUM_SCANCODES && engine_input_key_pressed((SDL_Scancode)key); }
#define REBAX_MAX_NODES 4096
static struct { const char *name; const node_interface_t *iface; void *instance; int visible; } g_nodes[REBAX_MAX_NODES];
static int g_node_count;

int rebax_runtime_register(const char *name, const node_interface_t *iface, void *instance) {
    if (g_node_count >= REBAX_MAX_NODES) return -1;
    g_nodes[g_node_count].name = name;
    g_nodes[g_node_count].iface = iface;
    g_nodes[g_node_count].instance = instance;
    g_nodes[g_node_count].visible = 1;
    return g_node_count++;
}
int rebax_runtime_visible(int node) { return node < 0 || node >= g_node_count || g_nodes[node].visible; }

static void *typed_property(int node, const char *name, int type) {
    if (node < 0 || node >= g_node_count || !name) return NULL;
    const node_interface_t *iface = g_nodes[node].iface;
    for (int i = 0; i < iface->property_count; i++) {
        if (iface->properties[i].type == type && strcmp(iface->properties[i].name, name) == 0)
            return ((char *)g_nodes[node].instance + iface->property_offsets[i]);
    }
    return NULL;
}
static float *float_property(int node, const char *name) { return (float *)typed_property(node, name, NODE_PROPERTY_TYPE_FLOAT); }
static int *int_property(int node, const char *name) { return (int *)typed_property(node, name, NODE_PROPERTY_TYPE_INT); }
int rebax_node_get_float(int node, const char *property, float *out_value) { float *p = float_property(node, property); if (!p || !out_value) return 0; *out_value = *p; return 1; }
int rebax_node_set_float(int node, const char *property, float value) { float *p = float_property(node, property); if (!p) return 0; *p = value; return 1; }
int rebax_node_get_int(int node, const char *property, int *out_value) { int *p = int_property(node, property); if (!p || !out_value) return 0; *out_value = *p; return 1; }
int rebax_node_set_int(int node, const char *property, int value) { int *p = int_property(node, property); if (!p) return 0; *p = value; return 1; }
int rebax_find_node(const char *name) {
    if (!name) return -1;
    for (int i = 0; i < g_node_count; i++) {
        if (g_nodes[i].name && strcmp(g_nodes[i].name, name) == 0) return i;
    }
    return -1;
}
int rebax_node_get_transform2d(int node, rebax_transform2d_t *out_transform) {
    float *x = float_property(node, "Position X"), *y = float_property(node, "Position Y");
    float *r = float_property(node, "Rotation"), *sx = float_property(node, "Scale X"), *sy = float_property(node, "Scale Y");
    if (!out_transform || !x || !y) return 0;
    out_transform->position.x = *x;
    out_transform->position.y = *y;
    out_transform->rotation = r ? *r : 0.0f;
    out_transform->scale.x = sx ? *sx : 1.0f;
    out_transform->scale.y = sy ? *sy : 1.0f;
    return 1;
}
int rebax_node_set_transform2d(int node, const rebax_transform2d_t *transform) {
    float *x = float_property(node, "Position X"), *y = float_property(node, "Position Y");
    float *r = float_property(node, "Rotation"), *sx = float_property(node, "Scale X"), *sy = float_property(node, "Scale Y");
    if (!transform || !x || !y) return 0;
    *x = transform->position.x;
    *y = transform->position.y;
    if (r) *r = transform->rotation;
    if (sx) *sx = transform->scale.x;
    if (sy) *sy = transform->scale.y;
    return 1;
}
int rebax_node_set_visible(int node, int visible) {
    if (node < 0 || node >= g_node_count) return 0;
    g_nodes[node].visible = visible != 0;
    return 1;
}
int rebax_node_is_visible(int node) { return rebax_runtime_visible(node); }
rebax_image_t *rebax_image_load(const char *asset_path) {
    rebax_image_t *image;
    if (!asset_path) return NULL;
    image = (rebax_image_t *)calloc(1, sizeof(*image));
    if (!image) return NULL;
    image->pixels = asset_image_load(asset_path, &image->width, &image->height);
    if (!image->pixels) { free(image); return NULL; }
    SDL_Renderer *renderer = engine_context_get_renderer();
    if (renderer) image->texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, image->width, image->height);
    if (image->texture) SDL_UpdateTexture(image->texture, NULL, image->pixels, image->width * 4);
    return image;
}
void rebax_image_free(rebax_image_t *image) { if (!image) return; if (image->texture) SDL_DestroyTexture(image->texture); asset_image_free(image->pixels); free(image); }
int rebax_image_draw(rebax_image_t *image, float x, float y, float width, float height) {
    if (!image || !image->texture) return 0;
    SDL_Rect dst = {(int)x, (int)y, width > 0 ? (int)width : image->width, height > 0 ? (int)height : image->height};
    return SDL_RenderCopy(engine_context_get_renderer(), image->texture, NULL, &dst) == 0;
}
void rebax_quit(void) { g_quit_requested = 1; SDL_Event event; memset(&event, 0, sizeof(event)); event.type = SDL_QUIT; SDL_PushEvent(&event); }
