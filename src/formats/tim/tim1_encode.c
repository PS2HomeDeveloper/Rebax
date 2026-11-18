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
#include <stdlib.h>
#include <string.h>
static void put16(tim_buffer_t *b, unsigned v) {
    unsigned char p[2];
    tim_wr16(p, v);
    tim_buffer_put(b, p, 2);
}
static void put32(tim_buffer_t *b, unsigned long v) {
    unsigned char p[4];
    tim_wr32(p, v);
    tim_buffer_put(b, p, 4);
}
int tim1_encode_mem(const unsigned char *rgba, int w, int h, rebax_tim1_format_t fmt,
                    const rebax_tim_encode_options_t *o, tim_buffer_t *out) {
    unsigned char *work = NULL, *idx = NULL, *pal = NULL;
    int ww = w, hh = h, colors, i;
    tim_buffer_t cl = {0}, px = {0};
    if (!rgba || !out || w <= 0 || h <= 0 || fmt > REBAX_TIM1_24BIT)
        return 0;
    if (!tim_pow2_resize(rgba, w, h, o ? o->resize_mode : 0, &work, &ww, &hh))
        return 0;
    if (fmt == REBAX_TIM1_4BIT || fmt == REBAX_TIM1_8BIT) {
        colors = fmt == REBAX_TIM1_4BIT ? 16 : 256;
        if (!tim_quantize(work, ww, hh, colors, o ? o->dither : 0, &idx, &pal)) {
            free(work);
            return 0;
        }
        put32(&cl, 12 + (unsigned long)colors * 2);
        put16(&cl, o ? o->clut_x : 0);
        put16(&cl, o ? o->clut_y : 0);
        put16(&cl, colors);
        put16(&cl, 1);
        for (i = 0; i < colors; i++)
            put16(&cl, tim_pack_ps1_5551(pal + i * 4));
        if (fmt == REBAX_TIM1_4BIT) {
            for (i = 0; i < ww * hh; i += 2) {
                unsigned char v = (unsigned char)((idx[i] & 15) | ((i + 1 < ww * hh ? idx[i + 1] : 0) << 4));
                tim_buffer_put(&px, &v, 1);
            }
        } else
            tim_buffer_put(&px, idx, (size_t)ww * hh);
    } else if (fmt == REBAX_TIM1_16BIT) {
        for (i = 0; i < ww * hh; i++)
            put16(&px, tim_pack_ps1_5551(work + i * 4));
    } else {
        for (i = 0; i < ww * hh; i++)
            tim_buffer_put(&px, work + i * 4, 3);
    }
    put32(out, 0x10);
    put32(out, (unsigned)fmt | (cl.n ? 8u : 0u));
    if (cl.n)
        tim_buffer_put(out, cl.p, cl.n);
    put32(out, 12 + (unsigned long)px.n);
    put16(out, o ? o->vram_x : 0);
    put16(out, o ? o->vram_y : 0);
    put16(out, fmt == 0 ? ww / 4 : fmt == 1 ? ww / 2 : fmt == 2 ? ww : ww * 3 / 2);
    put16(out, hh);
    tim_buffer_put(out, px.p, px.n);
    free(work);
    free(idx);
    free(pal);
    free(cl.p);
    free(px.p);
    return 1;
}
