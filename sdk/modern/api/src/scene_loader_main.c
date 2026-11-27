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

#include <stdlib.h>
#include <string.h>
#include "SDL2/SDL.h"
#include "engine_context.h"
#include "engine_input.h"
#include "scene_runtime.h"
#include "rebax_project_config.h"

#define MAX_ACTIVE_NODES 4096

int rebax_runtime_register(const char *name, const node_interface_t *iface, void *instance);
int rebax_runtime_visible(int node);

__attribute__((weak)) int main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    engine_context_config_t config = { "Rebax Game", REBAX_PROJECT_WIDTH,
        REBAX_PROJECT_HEIGHT, SDL_WINDOW_RESIZABLE, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC };
    SDL_SetHint(SDL_HINT_ORIENTATIONS, REBAX_PROJECT_WIDTH >= REBAX_PROJECT_HEIGHT ? "LandscapeLeft LandscapeRight" : "Portrait PortraitUpsideDown");
    if (!engine_context_init(&config)) return 1;
    void *instances[MAX_ACTIVE_NODES];
    const node_interface_t *interfaces[MAX_ACTIVE_NODES];
    int active_count = 0;
    for (int s = 0; s < g_scene_count; s++) {
        const scene_table_entry_t *scene = &g_scenes[s];
        if (scene->nodes == NULL || scene->node_count < 0) continue;
        for (int n = 0; n < scene->node_count && active_count < MAX_ACTIVE_NODES; n++) {
            const scene_node_entry_t *entry = &scene->nodes[n];
            void *instance = node_instantiate(entry->iface, entry->values);
            if (instance != NULL) {
                instances[active_count] = instance;
                interfaces[active_count] = entry->iface;
                rebax_runtime_register(entry->name, entry->iface, instance);
                active_count++;
            }
        }
    }
    int running = 1;
    Uint64 previous = SDL_GetPerformanceCounter();
    while (running) {
        SDL_Event event;
        engine_input_begin_frame();
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) running = 0;
            engine_input_process_event(&event);
        }
        engine_input_poll_controllers();
        Uint64 now = SDL_GetPerformanceCounter();
        Uint64 frequency = SDL_GetPerformanceFrequency();
        float delta_time = frequency != 0 ? (float)((double)(now - previous) / (double)frequency) : 0.0f;
        previous = now;
        if (delta_time > 0.25f) delta_time = 0.25f;
        engine_context_set_delta_time(delta_time);
        for (int i = 0; i < active_count; i++) {
            if (interfaces[i]->update != NULL) interfaces[i]->update(instances[i], delta_time);
        }
        SDL_Renderer *renderer = engine_context_get_renderer();
        if (renderer == NULL) { running = 0; break; }
        if (SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255) != 0 || SDL_RenderClear(renderer) != 0) { running = 0; break; }
        for (int i = 0; i < active_count; i++) {
            if (interfaces[i]->draw != NULL && rebax_runtime_visible(i)) interfaces[i]->draw(instances[i]);
        }
        SDL_RenderPresent(renderer);
    }
    for (int i = active_count - 1; i >= 0; i--) {
        if (interfaces[i]->destroy != NULL) interfaces[i]->destroy(instances[i]);
        free(instances[i]);
    }
    engine_context_shutdown();
    return 0;
}
