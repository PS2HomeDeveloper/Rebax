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
#include "ps1_tim.h"

static uint32_t word_at(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

int ps1_tim_mode_supported(int mode) {
    return mode >= 0 && mode <= 3;
}

int ps1_tim_texture_width(int mode, int image_words) {
    if (image_words < 0 || !ps1_tim_mode_supported(mode)) return 0;
    if (mode == 0) return image_words * 4;
    if (mode == 1) return image_words * 2;
    if (mode == 2) return image_words;
    return (image_words * 2) / 3;
}

#define PS1_TIM_AREA_X 512
#define PS1_TIM_AREA_END 1024
#define PS1_TIM_PAGE_WORDS 64
#define PS1_TIM_CLUT_Y 480
#define PS1_TIM_CLUT_END 512

static int g_next_x = PS1_TIM_AREA_X;
static int g_next_y = 0;
static int g_next_clut = PS1_TIM_CLUT_Y;

int ps1_tim_upload(const void *data, size_t size, ps1_tim_image_t *out_image) {
    const uint8_t *p = (const uint8_t *)data;
    size_t offset = 0;
    uint32_t flags;
    if (p == NULL || out_image == NULL || size < 8 || word_at(p) != 0x10) return 0;
    memset(out_image, 0, sizeof(*out_image));
    flags = word_at(p + 4);
    out_image->mode = (int)(flags & 7u);
    if (!ps1_tim_mode_supported(out_image->mode)) return 0;
    out_image->has_clut = (flags & 8u) != 0;
    offset = 8;
    if (out_image->has_clut) {
        uint32_t length;
        if (offset + 12 > size) return 0;
        length = word_at(p + offset);
        if (length < 12 || offset + length > size) return 0;
        if (g_next_clut >= PS1_TIM_CLUT_END) return 0;
        out_image->clut.x = 0;
        out_image->clut.y = (short)g_next_clut++;
        out_image->clut.w = (short)(p[offset + 8] | (p[offset + 9] << 8));
        out_image->clut.h = (short)(p[offset + 10] | (p[offset + 11] << 8));
        LoadImage(&out_image->clut, (uint32_t *)(p + offset + 12));
        DrawSync(0);
        offset += length;
    }
    if (offset + 12 > size) return 0;
    {
        uint32_t length = word_at(p + offset);
        if (length < 12 || offset + length > size) return 0;
        int pages;
        out_image->image.w = (short)(p[offset + 8] | (p[offset + 9] << 8));
        out_image->image.h = (short)(p[offset + 10] | (p[offset + 11] << 8));
        out_image->width = ps1_tim_texture_width(out_image->mode, out_image->image.w);
        out_image->height = out_image->image.h;
        if (out_image->mode > 2 || out_image->width > 256 || out_image->height > 256) return 0;
        pages = (out_image->image.w + PS1_TIM_PAGE_WORDS - 1) / PS1_TIM_PAGE_WORDS;
        if (g_next_x + pages * PS1_TIM_PAGE_WORDS > PS1_TIM_AREA_END) {
            g_next_x = PS1_TIM_AREA_X;
            g_next_y += 256;
        }
        if (g_next_y > 256) return 0;
        out_image->image.x = (short)g_next_x;
        out_image->image.y = (short)g_next_y;
        out_image->v = 0;
        g_next_x += pages * PS1_TIM_PAGE_WORDS;
        LoadImage(&out_image->image, (uint32_t *)(p + offset + 12));
        DrawSync(0);
    }
    return 1;
}
