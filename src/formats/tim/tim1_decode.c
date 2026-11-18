/*
 *  ____  _____ ____    _    __  __
 * |  _ \| ____| __ )  / \   \ \/ /
 * | |_) | _| | _ \ / _ \   \  /
 * |  _ <| |___| |_) / ___ \  /  \
 * |_| \_\_____|____/_/   \_\/_/\_\
 *
 * Rebax Engine
 * Copyright (c) 2026 PS2HomeDeveloper
 *
 * Licensed under the MIT License.
 */
#include "formats/tim/tim_internal.h"
#include <string.h>
static int tim1_width(int mode, unsigned words) {
    if (mode == 0)
        return (int)words * 4;
    if (mode == 1)
        return (int)words * 2;
    if (mode == 2)
        return (int)words;
    if (mode == 3)
        return (int)((words * 2) / 3);
    return 0;
}
int tim1_decode_mem(const unsigned char *d, size_t n, rebax_decoded_image_t *out) {
    unsigned flag, mode, off = 8, len, ww, h, w, i, count;
    const unsigned char *clut = NULL, *pix;
    unsigned char pal[256 * 4];
    size_t required;
    if (!d || !out || n < 8 || tim_rd32(d) != 0x10u)
        return 0;
    memset(out, 0, sizeof(*out));
    flag = (unsigned)tim_rd32(d + 4);
    mode = flag & 7;
    if (mode > 3)
        return 0;
    if (flag & 8) {
        if (n - off < 12)
            return 0;
        len = (unsigned)tim_rd32(d + off);
        if (len < 12 || len > n - off)
            return 0;
        count = mode == 0 ? 16 : mode == 1 ? 256 : 0;
        if (count && ((size_t)tim_rd16(d + off + 8) * tim_rd16(d + off + 10) * 2 < (size_t)count * 2))
            return 0;
        clut = d + off + 12;
        off += len;
    }
    if (n - off < 12)
        return 0;
    len = (unsigned)tim_rd32(d + off);
    if (len < 12 || len > n - off)
        return 0;
    ww = tim_rd16(d + off + 8);
    h = tim_rd16(d + off + 10);
    w = tim1_width(mode, ww);
    required = mode == 0   ? ((size_t)w * h + 1) / 2
               : mode == 1 ? (size_t)w * h
               : mode == 2 ? (size_t)w * h * 2
                           : (size_t)w * h * 3;
    if (w <= 0 || h == 0 || len - 12 < required)
        return 0;
    out->rgba = tim_rgba_alloc((int)w, (int)h);
    if (!out->rgba)
        return 0;
    out->width = (int)w;
    out->height = (int)h;
    pix = d + off + 12;
    if (mode == 2) {
        for (i = 0; i < w * h; i++)
            tim_unpack_ps1_5551(tim_rd16(pix + i * 2), out->rgba + i * 4);
    } else if (mode == 3) {
        for (i = 0; i < w * h; i++) {
            memcpy(out->rgba + i * 4, pix + i * 3, 3);
            out->rgba[i * 4 + 3] = 255;
        }
    } else {
        unsigned j;
        for (j = 0; j < count; j++)
            tim_unpack_ps1_5551(tim_rd16(clut + j * 2), pal + j * 4);
        for (i = 0; i < w * h; i++) {
            unsigned k = mode == 1 ? pix[i] : ((i & 1) ? pix[i / 2] >> 4 : pix[i / 2] & 15);
            memcpy(out->rgba + i * 4, pal + k * 4, 4);
        }
    }
    return 1;
}
