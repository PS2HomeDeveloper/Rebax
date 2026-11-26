/*
 *  ____  _____ ____    _    __  __
 * |  _ \| ____| __ )  / \   \ \/ /
 * | |_) |  _| |  _ \ / _ \   \  /
 * |  _ <| |___| |_) / ___ \  /  \
 * |_| \_\_____|____/_/   \_\/ /_\
 *
 * Rebax Engine
 * Copyright (c) 2026 PS2HomeDeveloper
 *
 * Licensed under the MIT License.
 * See the LICENSE file for details.
 */
#include <stddef.h>
#include <string.h>
#include "node_interface.h"
#include "engine_context.h"
#include "embedded_images.h"
#include "ps1_tim.h"

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
    ps1_tim_image_t tim;
    int loaded;
} sprite2d_instance_t;

static void sprite2d_init(void *self) {
    sprite2d_instance_t *instance = (sprite2d_instance_t *)self;
    memset(&instance->tim, 0, sizeof(instance->tim));
    instance->loaded = 0;
}

static void sprite2d_update(void *self, float delta_time) {
    (void)self;
    (void)delta_time;
}

static void sprite2d_load(sprite2d_instance_t *instance) {
    const embedded_image_t *image;
    if (instance->loaded || instance->image_path == NULL || instance->image_path[0] == '\0') return;
    image = embedded_image_find(instance->image_path);
    if (image != NULL && image->data != NULL && image->data_size != 0)
        instance->loaded = ps1_tim_upload(image->data, image->data_size, &instance->tim);
}

static void sprite2d_draw(void *self) {
    sprite2d_instance_t *instance = (sprite2d_instance_t *)self;
    ps1_engine_context_t *context = ps1_engine_context();
    sprite2d_load(instance);
    if (instance->loaded) {
        DR_TPAGE *page = (DR_TPAGE *)ps1_engine_primitive(context, 1, sizeof(DR_TPAGE));
        SPRT *sprite = (SPRT *)ps1_engine_primitive(context, 1, sizeof(SPRT));
        if (page == NULL || sprite == NULL) return;
        setDrawTPage(page, 0, 0, getTPage(instance->tim.mode, 0, instance->tim.image.x, instance->tim.image.y));
        setSprt(sprite);
        setXY0(sprite, (int)instance->position_x, (int)instance->position_y);
        setWH(sprite, instance->tim.width, instance->tim.height);
        setUV0(sprite, 0, instance->tim.v);
        setRGB0(sprite, 128, 128, 128);
        if (instance->tim.has_clut) setClut(sprite, instance->tim.clut.x, instance->tim.clut.y);
        return;
    }
    {
        TILE *tile = (TILE *)ps1_engine_primitive(context, 1, sizeof(TILE));
        if (tile == NULL) return;
        setTile(tile);
        setXY0(tile, (int)instance->position_x, (int)instance->position_y);
        setWH(tile, instance->raw_width > 0 ? instance->raw_width : 16, instance->raw_height > 0 ? instance->raw_height : 16);
        setRGB0(tile, 255, 255, 255);
    }
}

static void sprite2d_destroy(void *self) {
    sprite2d_instance_t *instance = (sprite2d_instance_t *)self;
    instance->loaded = 0;
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
    sprite2d_properties, sizeof(sprite2d_properties) / sizeof(sprite2d_properties[0]),
    sizeof(sprite2d_instance_t), sprite2d_offsets
};

/* @NODE type=NODE_TYPE_SPRITE_2D name=Sprite2D icon=sprite2d properties=sprite2d_properties */
