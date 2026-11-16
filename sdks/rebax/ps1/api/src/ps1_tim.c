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

int ps1_tim_upload(const void *data, size_t size, ps1_tim_image_t *out_image) {
    const uint8_t *p = (const uint8_t *)data;
    size_t offset = 0;
    uint32_t flags;
    if (p == NULL || out_image == NULL || size < 8 || word_at(p) != 0x10) return 0;
    memset(out_image, 0, sizeof(*out_image));
    flags = word_at(p + 4);
    out_image->mode = (int)(flags & 7u);
    out_image->has_clut = (flags & 8u) != 0;
    offset = 8;
    if (out_image->has_clut) {
        uint32_t length;
        if (offset + 12 > size) return 0;
        length = word_at(p + offset);
        if (length < 12 || offset + length > size) return 0;
        out_image->clut.x = (short)(p[offset + 4] | (p[offset + 5] << 8));
        out_image->clut.y = (short)(p[offset + 6] | (p[offset + 7] << 8));
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
        out_image->image.x = (short)(p[offset + 4] | (p[offset + 5] << 8));
        out_image->image.y = (short)(p[offset + 6] | (p[offset + 7] << 8));
        out_image->image.w = (short)(p[offset + 8] | (p[offset + 9] << 8));
        out_image->image.h = (short)(p[offset + 10] | (p[offset + 11] << 8));
        LoadImage(&out_image->image, (uint32_t *)(p + offset + 12));
        DrawSync(0);
    }
    return 1;
}
