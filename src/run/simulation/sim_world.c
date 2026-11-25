#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "run/simulation/sim_world.h"
#include "platform/common/window.h"
#include "formats/image_decoder.h"
#include "node_sim_registry_generated.h"

#define SIM_MAX_NODES SCENE_DATA_MAX_NODES
#define SIM_MAX_TEXTURES 64
#define SIM_MAX_WARNINGS 16
#define SIM_WARNING_LENGTH 160
#define SIM_MAX_HOOKS 16
#define SIM_PI 3.14159265358979323846

typedef struct {
    char path[256];
    window_texture_t *texture;
    int width;
    int height;
    int failed;
    size_t bytes;
} sim_texture_t;

typedef struct {
    node_type_t type;
    int parent;
    node_property_value_t *values;
    int property_count;
    sim_transform_t transform;
} sim_node_t;

typedef struct {
    sim_world_frame_hook_t callback;
    void *user_data;
} sim_hook_t;

struct sim_world {
    sim_node_t nodes[SIM_MAX_NODES];
    int node_count;
    sim_texture_t textures[SIM_MAX_TEXTURES];
    int texture_count;
    size_t texture_bytes;
    size_t texture_budget;
    int max_texture_size;
    char warnings[SIM_MAX_WARNINGS][SIM_WARNING_LENGTH];
    int warning_count;
    sim_hook_t hooks[SIM_MAX_HOOKS];
    int hook_count;
};

static char *sim_strdup(const char *value) {
    size_t length;
    char *copy;
    if (value == NULL) value = "";
    length = strlen(value) + 1;
    copy = (char *)malloc(length);
    if (copy != NULL) memcpy(copy, value, length);
    return copy;
}

static void add_warning(sim_world_t *world, const char *message) {
    int i;
    if (world == NULL || message == NULL || message[0] == '\0') return;
    for (i = 0; i < world->warning_count; i++) {
        if (strcmp(world->warnings[i], message) == 0) return;
    }
    if (world->warning_count >= SIM_MAX_WARNINGS) return;
    snprintf(world->warnings[world->warning_count], SIM_WARNING_LENGTH, "%s", message);
    world->warning_count++;
}

static int property_index(const sim_node_t *node, const char *name) {
    const node_registry_entry_t *entry;
    int i;
    if (node == NULL || name == NULL) return -1;
    entry = node_registry_get(node->type);
    if (entry == NULL) return -1;
    for (i = 0; i < node->property_count; i++) {
        if (entry->properties[i].name != NULL &&
            strcmp(entry->properties[i].name, name) == 0) return i;
    }
    return -1;
}

static float property_float(const sim_node_t *node, const char *name,
                            float fallback) {
    int index = property_index(node, name);
    if (index < 0 || node->values == NULL) return fallback;
    return node->values[index].f;
}

static sim_transform_t local_transform(const sim_node_t *node) {
    sim_transform_t transform;
    transform.x = property_float(node, "Position X", 0.0f);
    transform.y = property_float(node, "Position Y", 0.0f);
    transform.rotation = property_float(node, "Rotation", 0.0f);
    transform.scale_x = property_float(node, "Scale X", 1.0f);
    transform.scale_y = property_float(node, "Scale Y", 1.0f);
    return transform;
}

static sim_transform_t combine_transform(sim_transform_t parent,
                                         sim_transform_t local) {
    sim_transform_t result;
    float cosine = cosf(parent.rotation);
    float sine = sinf(parent.rotation);
    result.x = parent.x + cosine * local.x * parent.scale_x -
               sine * local.y * parent.scale_y;
    result.y = parent.y + sine * local.x * parent.scale_x +
               cosine * local.y * parent.scale_y;
    result.rotation = parent.rotation + local.rotation;
    result.scale_x = parent.scale_x * local.scale_x;
    result.scale_y = parent.scale_y * local.scale_y;
    return result;
}

static void release_contents(sim_world_t *world) {
    int i;
    int property;
    const node_registry_entry_t *entry;
    if (world == NULL) return;
    for (i = 0; i < world->texture_count; i++) {
        window_destroy_texture(world->textures[i].texture);
    }
    for (i = 0; i < world->node_count; i++) {
        entry = node_registry_get(world->nodes[i].type);
        if (world->nodes[i].values != NULL) {
            for (property = 0; property < world->nodes[i].property_count; property++) {
                if (entry != NULL &&
                    entry->properties[property].type == NODE_PROPERTY_TYPE_STRING) {
                    free(world->nodes[i].values[property].s);
                }
            }
            free(world->nodes[i].values);
        }
    }
    world->node_count = 0;
    world->texture_count = 0;
    world->texture_bytes = 0;
}

static sim_texture_t *texture_for(sim_world_t *world, const char *path,
                                  int raw_width, int raw_height, int raw_format) {
    sim_texture_t *texture;
    unsigned char *pixels;
    int width;
    int height;
    int i;
    if (path == NULL || path[0] == '\0') return NULL;
    for (i = 0; i < world->texture_count; i++) {
        if (strcmp(world->textures[i].path, path) == 0) return &world->textures[i];
    }
    if (world->texture_count >= SIM_MAX_TEXTURES) {
        add_warning(world, "Simulation texture limit reached");
        return NULL;
    }
    texture = &world->textures[world->texture_count++];
    memset(texture, 0, sizeof(*texture));
    snprintf(texture->path, sizeof(texture->path), "%s", path);
    width = 0;
    height = 0;
    pixels = image_loader_editor_load(path, &width, &height,
                                      raw_width, raw_height, raw_format);
    if (pixels == NULL || width <= 0 || height <= 0) {
        free(pixels);
        texture->failed = 1;
        add_warning(world, "Simulation image could not be loaded");
        return texture;
    }
    if (world->max_texture_size > 0 &&
        (width > world->max_texture_size || height > world->max_texture_size)) {
        free(pixels);
        texture->failed = 1;
        add_warning(world, "Simulation texture exceeds profile size");
        return texture;
    }
    texture->texture = window_create_texture(pixels, width, height);
    free(pixels);
    texture->width = width;
    texture->height = height;
    texture->bytes = (size_t)width * (size_t)height * 4u;
    if (texture->texture == NULL) {
        texture->failed = 1;
        add_warning(world, "Simulation texture could not be created");
        return texture;
    }
    world->texture_bytes += texture->bytes;
    if (world->texture_budget > 0 && world->texture_bytes > world->texture_budget) {
        add_warning(world, "Simulation texture memory budget exceeded");
    }
    return texture;
}

sim_world_t *sim_world_create(void) {
    return (sim_world_t *)calloc(1, sizeof(sim_world_t));
}

void sim_world_destroy(sim_world_t *world) {
    if (world == NULL) return;
    release_contents(world);
    free(world);
}

void sim_world_set_limits(sim_world_t *world, int texture_memory_mb,
                          int max_texture_size) {
    if (world == NULL) return;
    world->texture_budget = texture_memory_mb > 0 ?
        (size_t)texture_memory_mb * 1024u * 1024u : 0;
    world->max_texture_size = max_texture_size > 0 ? max_texture_size : 0;
}

int sim_world_build_from_scene(sim_world_t *world) {
    int scene_count;
    int i;
    if (world == NULL) return 0;
    release_contents(world);
    scene_count = scene_data_get_node_count();
    if (scene_count < 0) scene_count = 0;
    if (scene_count > SIM_MAX_NODES) {
        scene_count = SIM_MAX_NODES;
        add_warning(world, "Simulation node limit reached");
    }
    for (i = 0; i < scene_count; i++) {
        const node_registry_entry_t *entry;
        node_property_value_t *source;
        sim_node_t *node;
        int property;
        entry = node_registry_get(scene_data_get_type(i));
        if (entry == NULL || entry->property_count < 0) {
            add_warning(world, "Simulation scene contains an unknown node");
            continue;
        }
        node = &world->nodes[world->node_count];
        memset(node, 0, sizeof(*node));
        node->type = scene_data_get_type(i);
        node->parent = scene_data_get_parent(i);
        if (node->parent < -1 || node->parent >= i) node->parent = -1;
        node->property_count = entry->property_count;
        source = scene_data_get_values(i);
        if (node->property_count > 0) {
            node->values = (node_property_value_t *)calloc(
                (size_t)node->property_count, sizeof(*node->values));
            if (node->values == NULL) {
                release_contents(world);
                return 0;
            }
            for (property = 0; property < node->property_count; property++) {
                if (source != NULL && entry->properties[property].type ==
                    NODE_PROPERTY_TYPE_STRING) {
                    node->values[property].s = sim_strdup(source[property].s);
                } else if (source != NULL) {
                    node->values[property] = source[property];
                }
            }
        }
        node->transform = local_transform(node);
        if (node->parent >= 0) {
            node->transform = combine_transform(
                world->nodes[node->parent].transform, node->transform);
        }
        world->node_count++;
    }
    return 1;
}

static const node_sim_registry_entry_t *simulation_entry(node_type_t type) {
    int i;
    for (i = 0; i < NODE_SIM_REGISTRY_GENERATED_COUNT; i++) {
        if ((node_type_t)g_node_sim_registry[i].type == type) {
            return &g_node_sim_registry[i];
        }
    }
    return NULL;
}

void sim_world_update(sim_world_t *world, float delta_time) {
    int i;
    const node_sim_registry_entry_t *entry;
    if (world == NULL) return;
    for (i = 0; i < world->node_count; i++) {
        entry = simulation_entry(world->nodes[i].type);
        if (entry != NULL && entry->update != NULL) {
            entry->update(world, i, delta_time);
        }
    }
    for (i = 0; i < world->hook_count; i++) {
        if (world->hooks[i].callback != NULL) {
            world->hooks[i].callback(world, delta_time, world->hooks[i].user_data);
        }
    }
}

void sim_world_draw(sim_world_t *world) {
    int i;
    const node_sim_registry_entry_t *entry;
    if (world == NULL) return;
    for (i = 0; i < world->node_count; i++) {
        entry = simulation_entry(world->nodes[i].type);
        if (entry != NULL && entry->draw != NULL) entry->draw(world, i);
    }
}

void sim_world_draw_sprite(sim_world_t *world, int node_index) {
    sim_node_t *node;
    sim_texture_t *texture;
    int path_index;
    int raw_width_index;
    int raw_height_index;
    int raw_format_index;
    int raw_width;
    int raw_height;
    int raw_format;
    int width;
    int height;
    int x;
    int y;
    int draw_width;
    int draw_height;
    if (world == NULL || node_index < 0 || node_index >= world->node_count) return;
    node = &world->nodes[node_index];
    path_index = property_index(node, "Image Path");
    raw_width_index = property_index(node, "Raw Width");
    raw_height_index = property_index(node, "Raw Height");
    raw_format_index = property_index(node, "Raw Format");
    if (path_index < 0 || node->values == NULL) return;
    raw_width = raw_width_index >= 0 ? node->values[raw_width_index].i : 0;
    raw_height = raw_height_index >= 0 ? node->values[raw_height_index].i : 0;
    raw_format = raw_format_index >= 0 ? node->values[raw_format_index].i : 0;
    texture = texture_for(world, node->values[path_index].s,
                          raw_width, raw_height, raw_format);
    if (texture == NULL || texture->failed || texture->texture == NULL) return;
    width = property_float(node, "Scale X", 1.0f) * texture->width;
    height = property_float(node, "Scale Y", 1.0f) * texture->height;
    if (width < 1 || height < 1) return;
    draw_width = abs(width);
    draw_height = abs(height);
    x = (int)node->transform.x - draw_width / 2;
    y = (int)node->transform.y - draw_height / 2;
    window_draw_texture_region_rotated(texture->texture, 0, 0,
        texture->width, texture->height, x, y, draw_width, draw_height,
        (double)node->transform.rotation * 180.0 / SIM_PI);
}

int sim_world_register_frame_hook(sim_world_t *world,
                                  sim_world_frame_hook_t callback,
                                  void *user_data) {
    if (world == NULL || callback == NULL || world->hook_count >= SIM_MAX_HOOKS) return 0;
    world->hooks[world->hook_count].callback = callback;
    world->hooks[world->hook_count].user_data = user_data;
    world->hook_count++;
    return 1;
}

void sim_world_unregister_frame_hook(sim_world_t *world,
                                     sim_world_frame_hook_t callback,
                                     void *user_data) {
    int i;
    if (world == NULL) return;
    for (i = 0; i < world->hook_count; i++) {
        if (world->hooks[i].callback == callback &&
            world->hooks[i].user_data == user_data) {
            world->hooks[i] = world->hooks[world->hook_count - 1];
            world->hook_count--;
            return;
        }
    }
}

int sim_world_get_node_count(const sim_world_t *world) {
    return world != NULL ? world->node_count : 0;
}

int sim_world_get_transform(const sim_world_t *world, int index,
                            sim_transform_t *out) {
    if (world == NULL || out == NULL || index < 0 || index >= world->node_count) return 0;
    *out = world->nodes[index].transform;
    return 1;
}

int sim_world_get_property(const sim_world_t *world, int index,
                           const char *name, node_property_value_t *out) {
    int property;
    if (world == NULL || out == NULL || index < 0 || index >= world->node_count) return 0;
    property = property_index(&world->nodes[index], name);
    if (property < 0 || world->nodes[index].values == NULL) return 0;
    *out = world->nodes[index].values[property];
    return 1;
}

int sim_world_set_property(sim_world_t *world, int index, const char *name,
                           node_property_value_t value) {
    const node_registry_entry_t *entry;
    int property;
    if (world == NULL || index < 0 || index >= world->node_count) return 0;
    property = property_index(&world->nodes[index], name);
    entry = node_registry_get(world->nodes[index].type);
    if (property < 0 || entry == NULL || world->nodes[index].values == NULL) return 0;
    if (entry->properties[property].type == NODE_PROPERTY_TYPE_STRING) {
        free(world->nodes[index].values[property].s);
        world->nodes[index].values[property].s = sim_strdup(value.s);
    } else {
        world->nodes[index].values[property] = value;
    }
    return 1;
}

const char *sim_world_warning(const sim_world_t *world) {
    return sim_world_warning_at(world, 0);
}

int sim_world_warning_count(const sim_world_t *world) {
    return world != NULL ? world->warning_count : 0;
}

const char *sim_world_warning_at(const sim_world_t *world, int index) {
    if (world == NULL || index < 0 || index >= world->warning_count) return "";
    return world->warnings[index];
}
