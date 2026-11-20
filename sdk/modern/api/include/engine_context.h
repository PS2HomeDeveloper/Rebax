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

#ifndef ENGINE_CONTEXT_H
#define ENGINE_CONTEXT_H

#include <SDL2/SDL.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ENGINE_REFERENCE_WIDTH 640
#define ENGINE_REFERENCE_HEIGHT 448

typedef struct {
    const char *title;
    int width;
    int height;
    Uint32 window_flags;
    Uint32 renderer_flags;
} engine_context_config_t;

int engine_context_init(const engine_context_config_t *config);
void engine_context_shutdown(void);
SDL_Window *engine_context_get_window(void);
SDL_Renderer *engine_context_get_renderer(void);
int engine_context_get_width(void);
int engine_context_get_height(void);
float engine_context_scale_x(void);
float engine_context_scale_y(void);
float engine_context_delta_time(void);
void engine_context_set_delta_time(float delta_time);

#ifdef __cplusplus
}
#endif

#endif
