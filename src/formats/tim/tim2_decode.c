/*
 *  ____  _____ ____    _    __  __
 * |  _ \| ____| __ )  / \   \ \/ /
 * |_) | _| | _ \ / _ \   \  /
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
#include <stdlib.h>
static size_t psm_bytes(unsigned p, int w, int h) {
    size_t n = (size_t)w * h;
    if (p == TIM2_PSM_CT32 || p == TIM2_PSM_T8H || p == TIM2_PSM_T4HL || p == TIM2_PSM_T4HH)
        return n * 4;
    if (p == TIM2_PSM_CT24)
        return n * 3;
    if (p == TIM2_PSM_CT16 || p == TIM2_PSM_CT16S)
        return n * 2;
    if (p == TIM2_PSM_T8)
        return n;
    if (p == TIM2_PSM_T4)
        return (n + 1) / 2;
    return 0;
}
static size_t psm_swizzled_bytes(unsigned p, int w, int h) {
    if (p == TIM2_PSM_CT32 || p == TIM2_PSM_T8H || p == TIM2_PSM_T4HL || p == TIM2_PSM_T4HH)
        return (size_t)((w + 63) / 64) * ((h + 31) / 32) * 8192;
    if (p == TIM2_PSM_CT16 || p == TIM2_PSM_CT16S)
        return (size_t)((w + 63) / 64) * ((h + 63) / 64) * 8192;
    return psm_bytes(p, w, h);
}
static unsigned alpha_ps2(unsigned a) {
    a = (a * 255u + 64u) / 128u;
    return a > 255 ? 255 : a;
}
int tim2_decode_mem(const unsigned char *d, size_t n, int level, rebax_decoded_image_t *out) {
    const unsigned char *h, *pix, *clut, *base_pix;
    unsigned long block, cs, is;
    unsigned w, hh, psm, cpsm, csm, mips, i, np, levw, levh, lev;
    size_t start, off;
    unsigned char *linear = NULL, *pal = NULL;
    if (!d || !out || n < 64 || memcmp(d, "TIM2", 4) != 0)
        return 0;
    memset(out, 0, sizeof(*out));
    h = d + 16;
    block = tim_rd32(h);
    cs = tim_rd32(h + 4);
    is = tim_rd32(h + 8);
    mips = h[17];
    if (tim_rd16(h + 12) < 48 || 16u + block > n || level < 0 || level >= mips)
        return 0;
    w = tim_rd16(h + 20);
    hh = tim_rd16(h + 22);
    psm = (unsigned)((tim_rd64(h + 24) >> 20) & 63);
    cpsm = (unsigned)((tim_rd64(h + 24) >> 51) & 15);
    csm = (unsigned)((tim_rd64(h + 24) >> 55) & 1);
    start = 16u + tim_rd16(h + 12);
    if (start > n || is > n - start || cs > n - start - is)
        return 0;
    base_pix = d + start;
    clut = base_pix + is;
    pix = base_pix;
    levw = w;
    levh = hh;
    off = 0;
    for (lev = 0; lev < (unsigned)level; lev++) {
        size_t z = (h[5] == 1) ? psm_swizzled_bytes(psm, levw, levh) : psm_bytes(psm, levw, levh);
        if (z > is - off)
            return 0;
        off += z;
        levw = levw > 1 ? levw / 2 : 1;
        levh = levh > 1 ? levh / 2 : 1;
    }
    pix += off;
    w = levw;
    hh = levh;
    np = w * hh;
    out->rgba = tim_rgba_alloc((int)w, (int)hh);
    if (!out->rgba)
        return 0;
    out->width = (int)w;
    out->height = (int)hh;
    if (h[5] == 1) {
        size_t z = psm_swizzled_bytes(psm, w, hh);
        linear = (unsigned char *)tim_alloc(psm_bytes(psm, w, hh));
        if (psm == TIM2_PSM_CT32 || psm == TIM2_PSM_T8H || psm == TIM2_PSM_T4HL || psm == TIM2_PSM_T4HH)
            tim_unswizzle32(pix, linear, z, w, hh);
        else if (psm == TIM2_PSM_CT16 || psm == TIM2_PSM_CT16S)
            tim_unswizzle16(pix, linear, z, w, hh);
        if (linear)
            pix = linear;
    }
    if (psm == TIM2_PSM_CT32) {
        for (i = 0; i < np; i++) {
            memcpy(out->rgba + i * 4, pix + i * 4, 3);
            out->rgba[i * 4 + 3] = (unsigned char)alpha_ps2(pix[i * 4 + 3]);
        }
    } else if (psm == TIM2_PSM_CT24) {
        for (i = 0; i < np; i++) {
            memcpy(out->rgba + i * 4, pix + i * 3, 3);
            out->rgba[i * 4 + 3] = 255;
        }
    } else if (psm == TIM2_PSM_CT16 || psm == TIM2_PSM_CT16S) {
        for (i = 0; i < np; i++)
            tim_unpack_ps2_5551(tim_rd16(pix + i * 2), out->rgba + i * 4);
    } else if (psm == TIM2_PSM_T8 || psm == TIM2_PSM_T4 || psm == TIM2_PSM_T8H || psm == TIM2_PSM_T4HL ||
               psm == TIM2_PSM_T4HH) {
        unsigned count = (psm == TIM2_PSM_T8 || psm == TIM2_PSM_T8H) ? 256 : 16,
                 entry = (cpsm == TIM2_PSM_CT16 || cpsm == TIM2_PSM_CT16S) ? 2 : 4;
        if (cs < (unsigned long)count * entry) {
            free(linear);
            free(out->rgba);
            out->rgba = NULL;
            return 0;
        }
        pal = (unsigned char *)malloc(count * 4);
        if (!pal) {
            free(linear);
            free(out->rgba);
            out->rgba = NULL;
            return 0;
        }
        for (i = 0; i < count; i++) {
            if (entry == 2)
                tim_unpack_ps2_5551(tim_rd16(clut + i * 2), pal + i * 4);
            else {
                memcpy(pal + i * 4, clut + i * 4, 3);
                pal[i * 4 + 3] = cpsm == TIM2_PSM_CT24 ? 255 : (unsigned char)alpha_ps2(clut[i * 4 + 3]);
            }
        }
        if (count == 256 && !csm) {
            unsigned char tmp[1024];
            memcpy(tmp, pal, 1024);
            tim_unswizzle_clut8(tmp, pal);
        }
        for (i = 0; i < np; i++) {
            unsigned k;
            if (psm == TIM2_PSM_T8H)
                k = pix[i * 4 + 3];
            else if (psm == TIM2_PSM_T4HL)
                k = pix[i * 4 + 3] & 15;
            else if (psm == TIM2_PSM_T4HH)
                k = (pix[i * 4 + 3] >> 4) & 15;
            else if (count == 256)
                k = pix[i];
            else
                k = (i & 1) ? pix[i / 2] >> 4 : pix[i / 2] & 15;
            memcpy(out->rgba + i * 4, pal + k * 4, 4);
        }
    } else {
        free(linear);
        free(out->rgba);
        out->rgba = NULL;
        return 0;
    }
    free(pal);
    free(linear);
    return 1;
}
