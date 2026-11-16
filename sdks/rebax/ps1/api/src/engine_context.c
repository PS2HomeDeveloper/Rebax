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
#include <string.h>
#include "engine_context.h"

static ps1_engine_context_t g_context;

void ps1_engine_init(ps1_engine_context_t *context, int width, int height, int pal) {
    int flags = DISP_WIDTH_320;
    if (width <= 256) flags = DISP_WIDTH_256;
    else if (width >= 512) flags = DISP_WIDTH_512;
    if (height > 240) flags |= DISP_HEIGHT_HIGH;
    if (pal) flags |= DISP_MODE_PAL;
    ResetGraph(0);
    SetDefDrawEnv(&context->buffers[0].draw, 0, 0, width, height);
    SetDefDispEnv(&context->buffers[0].display, 0, 0, width, height);
    SetDefDrawEnv(&context->buffers[1].draw, 0, height, width, height);
    SetDefDispEnv(&context->buffers[1].display, 0, height, width, height);
    context->buffers[0].draw.isbg = 1;
    context->buffers[1].draw.isbg = 1;
    setRGB0(&context->buffers[0].draw, 0, 0, 0);
    setRGB0(&context->buffers[1].draw, 0, 0, 0);
    SetVideoMode(MODE_NTSC);
    SetDefDispEnv(&context->buffers[0].display, 0, 0, width, height);
    SetDefDispEnv(&context->buffers[1].display, 0, height, width, height);
    context->active_buffer = 0;
    context->next_packet = context->buffers[0].packet_buffer;
    context->width = width;
    context->height = height;
    ClearOTagR(context->buffers[0].ordering_table, 16);
    ClearOTagR(context->buffers[1].ordering_table, 16);
    SetDispMask(1);
    (void)flags;
}

void ps1_engine_begin(ps1_engine_context_t *context) {
    ps1_render_buffer_t *buffer = &context->buffers[context->active_buffer];
    context->next_packet = buffer->packet_buffer;
    ClearOTagR(buffer->ordering_table, 16);
}

void ps1_engine_end(ps1_engine_context_t *context) {
    ps1_render_buffer_t *draw = &context->buffers[context->active_buffer];
    ps1_render_buffer_t *display = &context->buffers[context->active_buffer ^ 1];
    DrawSync(0);
    VSync(0);
    PutDispEnv(&display->display);
    DrawOTagEnv(&draw->ordering_table[15], &draw->draw);
    context->active_buffer ^= 1;
    context->next_packet = display->packet_buffer;
    ClearOTagR(display->ordering_table, 16);
}

void *ps1_engine_primitive(ps1_engine_context_t *context, int z, size_t size) {
    ps1_render_buffer_t *buffer = &context->buffers[context->active_buffer];
    if (z < 0) z = 0;
    if (z > 15) z = 15;
    if ((size_t)(context->next_packet - buffer->packet_buffer) + size > sizeof(buffer->packet_buffer)) return NULL;
    void *primitive = context->next_packet;
    addPrim(&buffer->ordering_table[z], primitive);
    context->next_packet += size;
    return primitive;
}

ps1_engine_context_t *ps1_engine_context(void) { return &g_context; }
