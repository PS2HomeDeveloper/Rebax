/*
 *  ____  _____ ____    _    __  __
 * |  _ \| ____| __ )  / \   \ \/ /
 * | |_) |  _| |  _ \ / _ \   \  / 
 * |  _ <| |___| |_) / ___ \  /  \ 
 * |_| \_\_____|____/_/   \_\/_/\_\
 *
 * Rebax Engine
 * Copyright (c) 2026 PS2HomeDeveloper
 *
 * Licensed under the MIT License.
 * See the LICENSE file for details.
 */

/* @EXPORT_LIBS: -lSDL2 -lm */
/* @EXPORT_INCLUDES: -I${REBAX_SDK_ROOT}/graphics/include */
#include <stddef.h>
#include <stdint.h>
#include <limits.h>
#include <math.h>
#include <SDL2/SDL.h>
#include "node_interface.h"
#include "engine_context.h"
#include "asset_image.h"

typedef struct {
    float position_x;
    float position_y;
    float rotation;
    float scale_x;
    float scale_y;
    const char *image_path;
    int raw_width;
    int raw_height;
    int raw_format;
    SDL_Texture *texture;
    int texture_loaded;
    int load_failed;
} sprite2d_instance_t;

static void sprite2d_init(void *self) {
    sprite2d_instance_t *instance = (sprite2d_instance_t *)self;
    instance->texture = NULL;
    instance->texture_loaded = 0;
}

static void sprite2d_load_texture(sprite2d_instance_t *instance) {
    if (instance->texture_loaded || instance->load_failed || instance->image_path == NULL || instance->image_path[0] == '\0') return;
    SDL_Renderer *renderer = engine_context_get_renderer();
    if (renderer == NULL) return;
    instance->load_failed = 1;
    int width = 0, height = 0;
    unsigned char *pixels = asset_image_load(instance->image_path, &width, &height);
    if (pixels == NULL || width <= 0 || height <= 0) {
        asset_image_free(pixels);
        return;
    }
    SDL_Texture *texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, width, height);
    if (texture == NULL) {
        asset_image_free(pixels);
        return;
    }
    if (SDL_UpdateTexture(texture, NULL, pixels, width * 4) != 0 ||
        SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND) != 0) {
        SDL_DestroyTexture(texture);
        asset_image_free(pixels);
        return;
    }
    asset_image_free(pixels);
    instance->texture = texture;
    instance->texture_loaded = 1;
    instance->load_failed = 0;
}

static void sprite2d_update(void *self, float delta_time) {
    (void)self;
    (void)delta_time;
}

static void sprite2d_draw(void *self) {
    sprite2d_instance_t *instance = (sprite2d_instance_t *)self;
    sprite2d_load_texture(instance);
    SDL_Renderer *renderer = engine_context_get_renderer();
    if (renderer == NULL) return;
    if (!instance->texture_loaded) return;
    int width = 0, height = 0;
    if (SDL_QueryTexture(instance->texture, NULL, NULL, &width, &height) != 0 || width <= 0 || height <= 0) return;
    float scale_x = engine_context_scale_x();
    float scale_y = engine_context_scale_y();
    SDL_FRect destination = {
        instance->position_x * scale_x - ((float)width * instance->scale_x * scale_x * 0.5f),
        instance->position_y * scale_y - ((float)height * instance->scale_y * scale_y * 0.5f),
        (float)width * instance->scale_x * scale_x,
        (float)height * instance->scale_y * scale_y
    };
    if (destination.w <= 0.0f || destination.h <= 0.0f) return;
    double degrees = (double)instance->rotation * (180.0 / 3.14159265358979323846);
    SDL_RenderCopyExF(renderer, instance->texture, NULL, &destination, degrees, NULL, SDL_FLIP_NONE);
}

static void sprite2d_destroy(void *self) {
    sprite2d_instance_t *instance = (sprite2d_instance_t *)self;
    if (instance->texture != NULL) SDL_DestroyTexture(instance->texture);
    instance->texture = NULL;
    instance->texture_loaded = 0;
}

static const node_property_t sprite2d_properties[] = {
    { "Position X", NODE_PROPERTY_TYPE_FLOAT, { .f = 0.0f }, 0 },
    { "Position Y", NODE_PROPERTY_TYPE_FLOAT, { .f = 0.0f }, 0 },
    { "Rotation", NODE_PROPERTY_TYPE_FLOAT, { .f = 0.0f }, 0 },
    { "Scale X", NODE_PROPERTY_TYPE_FLOAT, { .f = 1.0f }, 0 },
    { "Scale Y", NODE_PROPERTY_TYPE_FLOAT, { .f = 1.0f }, 0 },
    { "Image Path", NODE_PROPERTY_TYPE_STRING, { .s = "" }, 1 },
    { "Raw Width", NODE_PROPERTY_TYPE_INT, { .i = 0 }, 0 },
    { "Raw Height", NODE_PROPERTY_TYPE_INT, { .i = 0 }, 0 },
    { "Raw Format", NODE_PROPERTY_TYPE_INT, { .i = 0 }, 0 }
};
static const size_t sprite2d_offsets[] = {
    offsetof(sprite2d_instance_t, position_x), offsetof(sprite2d_instance_t, position_y),
    offsetof(sprite2d_instance_t, rotation), offsetof(sprite2d_instance_t, scale_x),
    offsetof(sprite2d_instance_t, scale_y), offsetof(sprite2d_instance_t, image_path),
    offsetof(sprite2d_instance_t, raw_width), offsetof(sprite2d_instance_t, raw_height),
    offsetof(sprite2d_instance_t, raw_format)
};
const node_interface_t sprite2d_interface = {
    sprite2d_init, sprite2d_update, sprite2d_draw, sprite2d_destroy,
    sprite2d_properties, (int)(sizeof(sprite2d_properties) / sizeof(sprite2d_properties[0])),
    sizeof(sprite2d_instance_t), sprite2d_offsets
};
/* @NODE type=NODE_TYPE_SPRITE_2D name=Sprite2D icon=sprite2d properties=sprite2d_properties */
