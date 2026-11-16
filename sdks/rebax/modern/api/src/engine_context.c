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

#include <string.h>
#include "engine_context.h"
#include "engine_input.h"

static SDL_Window *g_window;
static SDL_Renderer *g_renderer;
static int g_width;
static int g_height;
static int g_reference_width = ENGINE_REFERENCE_WIDTH;
static int g_reference_height = ENGINE_REFERENCE_HEIGHT;
static float g_delta_time;

int engine_context_init(const engine_context_config_t *config) {
    engine_context_config_t defaults = { "Rebax Game", ENGINE_REFERENCE_WIDTH,
        ENGINE_REFERENCE_HEIGHT, SDL_WINDOW_RESIZABLE, SDL_RENDERER_ACCELERATED };
    if (config == NULL) config = &defaults;
    int width = config->width > 0 ? config->width : ENGINE_REFERENCE_WIDTH;
    int height = config->height > 0 ? config->height : ENGINE_REFERENCE_HEIGHT;
    g_reference_width = width;
    g_reference_height = height;
    Uint32 window_flags = config->window_flags;
    Uint32 renderer_flags = config->renderer_flags;
    if (SDL_WasInit(SDL_INIT_VIDEO) != 0 || g_window != NULL || g_renderer != NULL) return 0;
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_GAMECONTROLLER | SDL_INIT_JOYSTICK) != 0) {
        SDL_Quit();
        return 0;
    }
    g_window = SDL_CreateWindow(config->title != NULL ? config->title : "Rebax Game",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width, height, window_flags);
    if (g_window == NULL) goto fail;
    g_renderer = SDL_CreateRenderer(g_window, -1, renderer_flags);
    if (g_renderer == NULL && (renderer_flags & SDL_RENDERER_ACCELERATED) != 0) {
        g_renderer = SDL_CreateRenderer(g_window, -1, SDL_RENDERER_SOFTWARE);
    }
    if (g_renderer == NULL) goto fail;
    if (SDL_SetRenderDrawBlendMode(g_renderer, SDL_BLENDMODE_BLEND) != 0) goto fail;
    SDL_GetRendererOutputSize(g_renderer, &g_width, &g_height);
    if (g_width <= 0 || g_height <= 0) { g_width = width; g_height = height; }
    g_delta_time = 0.0f;
    engine_input_init();
    return 1;
fail:
    if (g_renderer != NULL) SDL_DestroyRenderer(g_renderer);
    if (g_window != NULL) SDL_DestroyWindow(g_window);
    g_renderer = NULL;
    g_window = NULL;
    SDL_Quit();
    return 0;
}

void engine_context_shutdown(void) {
    engine_input_shutdown();
    if (g_renderer != NULL) SDL_DestroyRenderer(g_renderer);
    if (g_window != NULL) SDL_DestroyWindow(g_window);
    g_renderer = NULL;
    g_window = NULL;
    g_width = 0;
    g_height = 0;
    g_delta_time = 0.0f;
    g_reference_width = ENGINE_REFERENCE_WIDTH;
    g_reference_height = ENGINE_REFERENCE_HEIGHT;
    SDL_Quit();
}

SDL_Window *engine_context_get_window(void) { return g_window; }
SDL_Renderer *engine_context_get_renderer(void) { return g_renderer; }
int engine_context_get_width(void) {
    int width = 0, height = 0;
    if (g_renderer != NULL && SDL_GetRendererOutputSize(g_renderer, &width, &height) == 0 && width > 0) g_width = width;
    return g_width;
}
int engine_context_get_height(void) {
    int width = 0, height = 0;
    if (g_renderer != NULL && SDL_GetRendererOutputSize(g_renderer, &width, &height) == 0 && height > 0) g_height = height;
    return g_height;
}
float engine_context_scale_x(void) { return (float)engine_context_get_width() / (float)g_reference_width; }
float engine_context_scale_y(void) { return (float)engine_context_get_height() / (float)g_reference_height; }
float engine_context_delta_time(void) { return g_delta_time; }
void engine_context_set_delta_time(float delta_time) {
    g_delta_time = delta_time >= 0.0f ? delta_time : 0.0f;
}
