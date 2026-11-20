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
#ifndef REBAX_PS1_ENGINE_CONTEXT_H
#define REBAX_PS1_ENGINE_CONTEXT_H

#include <stddef.h>
#include <stdint.h>
#include <psxgpu.h>
#include <psxapi.h>

typedef struct {
    DISPENV display;
    DRAWENV draw;
    uint32_t ordering_table[16];
    uint8_t packet_buffer[8192];
} ps1_render_buffer_t;

typedef struct {
    ps1_render_buffer_t buffers[2];
    uint8_t *next_packet;
    int active_buffer;
    int width;
    int height;
} ps1_engine_context_t;

void ps1_engine_init(ps1_engine_context_t *context, int width, int height, int pal);
void ps1_engine_begin(ps1_engine_context_t *context);
void ps1_engine_end(ps1_engine_context_t *context);
void *ps1_engine_primitive(ps1_engine_context_t *context, int z, size_t size);
ps1_engine_context_t *ps1_engine_context(void);

#endif
