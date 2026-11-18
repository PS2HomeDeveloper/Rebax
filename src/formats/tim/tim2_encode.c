/*
 *  ____  _____ ____    _    __  __
 * |  _ \| ____| _ \ / _ \   \  /
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
static unsigned psm_for(rebax_tim2_format_t f) {
    switch (f) {
    case REBAX_TIM2_32BIT:
        return TIM2_PSM_CT32;
    case REBAX_TIM2_24BIT:
        return TIM2_PSM_CT24;
    case REBAX_TIM2_16BIT:
        return TIM2_PSM_CT16;
    case REBAX_TIM2_16S:
        return TIM2_PSM_CT16S;
    case REBAX_TIM2_8BIT:
        return TIM2_PSM_T8;
    case REBAX_TIM2_4BIT:
        return TIM2_PSM_T4;
    case REBAX_TIM2_8H:
        return TIM2_PSM_T8H;
    case REBAX_TIM2_4HL:
        return TIM2_PSM_T4HL;
    default:
        return TIM2_PSM_T4HH;
    }
}
static unsigned long long tex0_for(int w, int h, unsigned psm, int cpsm, int cbp, int csm) {
    int unit = (psm == TIM2_PSM_T8) ? 128 : (psm == TIM2_PSM_T4 ? 256 : 64), tbw = (w + unit - 1) / unit;
    if (tbw < 1)
        tbw = 1;
    int tw = 0, th = 0, v = 1;
    while (v < w && tw < 15) {
        v <<= 1;
        tw++;
    }
    v = 1;
    while (v < h && th < 15) {
        v <<= 1;
        th++;
    }
    return ((unsigned long long)tbw << 14) | ((unsigned long long)psm << 20) |
           ((unsigned long long)tw << 26) | ((unsigned long long)th << 30) | (1ull << 34) |
           ((unsigned long long)cbp << 37) | ((unsigned long long)cpsm << 51) |
           ((unsigned long long)csm << 55) | (1ull << 61);
}
static int pack_level(const unsigned char *rgba, int w, int h, rebax_tim2_format_t fmt,
                      const rebax_tim_encode_options_t *o, unsigned char **out, size_t *size,
                      unsigned char **clut, size_t *clut_size) {
    unsigned char *raw = NULL, *packed = NULL, *idx = NULL, *pal = NULL;
    size_t n = (size_t)w * h * 4, i;
    int colors;
    unsigned psm = psm_for(fmt);
    if (fmt == REBAX_TIM2_32BIT) {
        n = (size_t)w * h * 4;
        raw = (unsigned char *)malloc(n);
        if (!raw)
            return 0;
        for (i = 0; i < (size_t)w * h; i++) {
            raw[i * 4] = rgba[i * 4];
            raw[i * 4 + 1] = rgba[i * 4 + 1];
            raw[i * 4 + 2] = rgba[i * 4 + 2];
            raw[i * 4 + 3] = o && o->premultiply_alpha
                                 ? (unsigned char)(rgba[i * 4 + 3] * rgba[i * 4 + 3] / 255)
                                 : rgba[i * 4 + 3];
        }
    } else if (fmt == REBAX_TIM2_24BIT) {
        n = (size_t)w * h * 3;
        raw = (unsigned char *)malloc(n);
        if (!raw)
            return 0;
        for (i = 0; i < (size_t)w * h; i++)
            memcpy(raw + i * 3, rgba + i * 4, 3);
    } else if (fmt == REBAX_TIM2_16BIT || fmt == REBAX_TIM2_16S) {
        n = (size_t)w * h * 2;
        raw = (unsigned char *)malloc(n);
        if (!raw)
            return 0;
        for (i = 0; i < (size_t)w * h; i++)
            tim_wr16(raw + i * 2, tim_pack_ps2_5551(rgba + i * 4));
    } else {
        colors = (fmt == REBAX_TIM2_8BIT || fmt == REBAX_TIM2_8H) ? 256 : 16;
        if (!tim_quantize(rgba, w, h, colors, o ? o->dither : 0, &idx, &pal))
            return 0;
        if (fmt == REBAX_TIM2_8BIT) {
            n = (size_t)w * h;
            raw = idx;
            idx = NULL;
        } else if (fmt == REBAX_TIM2_4BIT) {
            n = ((size_t)w * h + 1) / 2;
            raw = (unsigned char *)calloc(n, 1);
            if (!raw) {
                free(idx);
                free(pal);
                return 0;
            }
            for (i = 0; i < (size_t)w * h; i++)
                raw[i / 2] |= (unsigned char)((idx[i] & 15) << ((i & 1) * 4));
        } else {
            n = (size_t)w * h * 4;
            raw = (unsigned char *)calloc(n, 1);
            if (!raw) {
                free(idx);
                free(pal);
                return 0;
            }
            for (i = 0; i < (size_t)w * h; i++)
                raw[i * 4 + 3] = (unsigned char)(fmt == REBAX_TIM2_8H    ? idx[i]
                                                 : fmt == REBAX_TIM2_4HL ? (idx[i] & 15)
                                                                         : ((idx[i] & 15) << 4));
        }
        if (!*clut) {
            int cf = o && o->clut_format ? o->clut_format : 32, entry = cf == 16 || cf == 161 ? 2 : 4;
            *clut_size = (size_t)colors * entry;
            *clut = (unsigned char *)malloc(*clut_size);
            if (!*clut_size || !*clut) {
                free(raw);
                free(idx);
                free(pal);
                return 0;
            }
            for (i = 0; i < (size_t)colors; i++) {
                if (entry == 2)
                    tim_wr16(*clut + i * 2, tim_pack_ps2_5551(pal + i * 4));
                else {
                    (*clut)[i * entry] = pal[i * 4];
                    (*clut)[i * entry + 1] = pal[i * 4 + 1];
                    (*clut)[i * entry + 2] = pal[i * 4 + 2];
                    (*clut)[i * entry + 3] = pal[i * 4 + 3];
                }
            }
            if (colors == 256 && !(o && o->csm2)) {
                unsigned char tmp[1024];
                memcpy(tmp, *clut, 1024);
                tim_swizzle_clut8(tmp, *clut);
            }
        }
        free(idx);
        free(pal);
    }
    free(packed);
    *out = raw;
    *size = n;
    return 1;
}
int tim2_encode_mem(const unsigned char *rgba, int w, int h, rebax_tim2_format_t fmt,
                    const rebax_tim_encode_options_t *o, tim_buffer_t *out) {
    unsigned char *work = NULL, *level = NULL, *clut = NULL, *swz = NULL;
    int ww = w, hh = h, levels = 1, lw, lh;
    size_t sz = 0, clut_n = 0, total_image = 0, i;
    tim_buffer_t images = {0};
    rebax_tim_encode_options_t def;
    if (!o) {
        rebax_tim_encode_options_default(&def);
        o = &def;
    }
    if (!tim_pow2_resize(rgba, w, h, o->resize_mode, &work, &ww, &hh))
        return 0;
    lw = ww;
    lh = hh;
    for (;;) {
        if (!pack_level(work, lw, lh, fmt, o, &level, &sz, &clut, &clut_n))
            goto fail;
        if (o->swizzle && (fmt == REBAX_TIM2_32BIT || fmt == REBAX_TIM2_16BIT || fmt == REBAX_TIM2_16S ||
                           fmt == REBAX_TIM2_8H || fmt == REBAX_TIM2_4HL || fmt == REBAX_TIM2_4HH)) {
            swz = (unsigned char *)malloc((size_t)((fmt == REBAX_TIM2_32BIT || fmt == REBAX_TIM2_8H ||
                                                    fmt == REBAX_TIM2_4HL || fmt == REBAX_TIM2_4HH)
                                                       ? ((lw + 63) / 64) * ((lh + 31) / 32) * 8192
                                                       : ((lw + 63) / 64) * ((lh + 63) / 64) * 8192));
            if (!swz) {
                free(level);
                goto fail;
            }
            if (fmt == REBAX_TIM2_32BIT || fmt == REBAX_TIM2_8H || fmt == REBAX_TIM2_4HL ||
                fmt == REBAX_TIM2_4HH)
                tim_swizzle32(level, swz, sz, lw, lh);
            else
                tim_swizzle16(level, swz, sz, lw, lh);
            free(level);
            level = swz;
            sz = (size_t)((fmt == REBAX_TIM2_32BIT || fmt == REBAX_TIM2_8H || fmt == REBAX_TIM2_4HL ||
                           fmt == REBAX_TIM2_4HH)
                              ? ((lw + 63) / 64) * ((lh + 31) / 32) * 8192
                              : ((lw + 63) / 64) * ((lh + 63) / 64) * 8192);
            swz = NULL;
        }
        tim_buffer_put(&images, level, sz);
        total_image += sz;
        free(level);
        level = NULL;
        if (!o->mipmaps || fmt >= REBAX_TIM2_8BIT || lw == 1 && lh == 1)
            break;
        {
            unsigned char *next = NULL;
            int nw, nh;
            tim_downsample(work, lw, lh, &next, &nw, &nh);
            free(work);
            work = next;
            lw = nw;
            lh = nh;
            levels++;
        }
    }
    {
        unsigned char fh[16] = {'T', 'I', 'M', '2', 4, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0};
        unsigned char ph[48] = {0};
        unsigned psm = psm_for(fmt), cpsm = (o->clut_format == 16 || o->clut_format == 161)
                                                ? (o->clut_format == 161 ? 0x0a : 0x02)
                                                : 0;
        tim_wr32(ph, 48u + (unsigned long)total_image + (unsigned long)clut_n);
        tim_wr32(ph + 4, (unsigned long)clut_n);
        tim_wr32(ph + 8, (unsigned long)total_image);
        tim_wr16(ph + 12, 48);
        tim_wr16(ph + 14,
                 (unsigned)(clut_n ? (fmt == REBAX_TIM2_8BIT || fmt == REBAX_TIM2_8H ? 256 : 16) : 0));
        ph[17] = (unsigned char)levels;
        ph[18] = (unsigned char)(clut_n ? (cpsm == 0 ? 3 : (cpsm == 2 ? 1 : 1)) : 0);
        ph[19] =
            (unsigned char)((fmt == REBAX_TIM2_8BIT || fmt == REBAX_TIM2_8H)                             ? 2
                            : (fmt == REBAX_TIM2_4BIT || fmt == REBAX_TIM2_4HL || fmt == REBAX_TIM2_4HH) ? 4
                            : fmt == REBAX_TIM2_16BIT || fmt == REBAX_TIM2_16S                           ? 1
                            : fmt == REBAX_TIM2_24BIT                                                    ? 2
                                                                                                         : 0);
        tim_wr16(ph + 20, ww);
        tim_wr16(ph + 22, hh);
        tim_wr64(ph + 24, tex0_for(ww, hh, psm, cpsm, 0, o->csm2 ? 1 : 0));
        tim_buffer_put(out, fh, 16);
        tim_buffer_put(out, ph, 48);
        tim_buffer_put(out, images.p, images.n);
        if (clut_n)
            tim_buffer_put(out, clut, clut_n);
    }
    free(work);
    free(images.p);
    free(clut);
    return out->p != NULL;
fail:
    free(work);
    free(level);
    free(swz);
    free(images.p);
    free(clut);
    return 0;
}
