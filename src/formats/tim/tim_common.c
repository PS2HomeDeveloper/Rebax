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
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */
#include "formats/tim/tim_internal.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>

unsigned tim_rd16(const unsigned char *p) {
    return p[0] | ((unsigned)p[1] << 8);
}
unsigned long tim_rd32(const unsigned char *p) {
    return (unsigned long)p[0] | ((unsigned long)p[1] << 8) | ((unsigned long)p[2] << 16) |
           ((unsigned long)p[3] << 24);
}
unsigned long long tim_rd64(const unsigned char *p) {
    unsigned long long v = 0;
    int i;
    for (i = 0; i < 8; i++)
        v |= (unsigned long long)p[i] << (i * 8);
    return v;
}
void tim_wr16(unsigned char *p, unsigned v) {
    p[0] = (unsigned char)v;
    p[1] = (unsigned char)(v >> 8);
}
void tim_wr32(unsigned char *p, unsigned long v) {
    p[0] = (unsigned char)v;
    p[1] = (unsigned char)(v >> 8);
    p[2] = (unsigned char)(v >> 16);
    p[3] = (unsigned char)(v >> 24);
}
void tim_wr64(unsigned char *p, unsigned long long v) {
    int i;
    for (i = 0; i < 8; i++)
        p[i] = (unsigned char)(v >> (i * 8));
}
void *tim_alloc(size_t n) {
    return calloc(1, n ? n : 1);
}
unsigned char *tim_rgba_alloc(int w, int h) {
    size_t n;
    if (w <= 0 || h <= 0 || (size_t)w > (size_t)-1 / (size_t)h)
        return NULL;
    n = (size_t)w * (size_t)h;
    if (n > (size_t)-1 / 4)
        return NULL;
    return (unsigned char *)malloc(n * 4);
}
void tim_buffer_put(tim_buffer_t *b, const void *p, size_t n) {
    size_t need;
    if (!n)
        return;
    if (b->n > SIZE_MAX - n)
        return;
    need = b->n + n;
    if (need > b->cap) {
        size_t c = b->cap ? b->cap : 256;
        while (c < need) {
            if (c > SIZE_MAX / 2) {
                c = need;
                break;
            }
            c *= 2;
        }
        b->p = (unsigned char *)realloc(b->p, c);
        b->cap = c;
    }
    if (b->p)
        memcpy(b->p + b->n, p, n);
    b->n = need;
}
int tim_write_file(const char *path, const unsigned char *p, size_t n) {
    FILE *f = fopen(path, "wb");
    int ok;
    if (!f)
        return 0;
    ok = fwrite(p, 1, n, f) == n;
    ok = ok && fclose(f) == 0;
    return ok;
}
int tim_read_file(const char *path, unsigned char **p, size_t *n) {
    FILE *f;
    long len;
    if (!p || !n)
        return 0;
    *p = NULL;
    *n = 0;
    f = fopen(path, "rb");
    if (!f)
        return 0;
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return 0;
    }
    len = ftell(f);
    if (len < 0 || fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return 0;
    }
    *p = (unsigned char *)malloc((size_t)len);
    if (!*p) {
        fclose(f);
        return 0;
    }
    *n = (size_t)len;
    if (fread(*p, 1, *n, f) != *n) {
        free(*p);
        *p = NULL;
        *n = 0;
        fclose(f);
        return 0;
    }
    fclose(f);
    return 1;
}
void tim_unpack_ps1_5551(unsigned v, unsigned char *o) {
    unsigned r = v & 31, g = (v >> 5) & 31, b = (v >> 10) & 31;
    o[0] = (unsigned char)((r << 3) | (r >> 2));
    o[1] = (unsigned char)((g << 3) | (g >> 2));
    o[2] = (unsigned char)((b << 3) | (b >> 2));
    if (v == 0)
        o[3] = 0;
    else if (v & 0x8000)
        o[3] = (r == 0 && g == 0 && b == 0) ? 255 : 128;
    else
        o[3] = 255;
}
void tim_unpack_ps2_5551(unsigned v, unsigned char *o) {
    unsigned r = v & 31, g = (v >> 5) & 31, b = (v >> 10) & 31;
    o[0] = (unsigned char)((r << 3) | (r >> 2));
    o[1] = (unsigned char)((g << 3) | (g >> 2));
    o[2] = (unsigned char)((b << 3) | (b >> 2));
    o[3] = (v & 0x8000) ? 255 : 0;
}
unsigned tim_pack_ps1_5551(const unsigned char *p) {
    unsigned r = p[0] * 31 / 255, g = p[1] * 31 / 255, b = p[2] * 31 / 255, a = p[3];
    unsigned v;
    if (!a)
        return 0;
    v = ((a < 255) ? 0x8000u : 0) | r | (g << 5) | (b << 10);
    return v ? v : 0x8000u;
}
unsigned tim_pack_ps2_5551(const unsigned char *p) {
    unsigned r = (p[0] * 31 + 127) / 255, g = (p[1] * 31 + 127) / 255, b = (p[2] * 31 + 127) / 255;
    return ((p[3] >= 128) ? 0x8000u : 0) | r | (g << 5) | (b << 10);
}
static const unsigned char BO32[32] = {0, 1, 4,  5,  16, 17, 20, 21, 2,  3,  6,  7,  18, 19, 22, 23,
                                       8, 9, 12, 13, 24, 25, 28, 29, 10, 11, 14, 15, 26, 27, 30, 31};
static const unsigned char BO16[64] = {0,  2,  8,  10, 32, 34, 40, 42, 1,  3,  9,  11, 33, 35, 41, 43,
                                       4,  6,  12, 14, 36, 38, 44, 46, 5,  7,  13, 15, 37, 39, 45, 47,
                                       16, 18, 24, 26, 48, 50, 56, 58, 17, 19, 25, 27, 49, 51, 57, 59,
                                       20, 22, 28, 30, 52, 54, 60, 62, 21, 23, 29, 31, 53, 55, 61, 63};
void tim_swizzle32(const unsigned char *s, unsigned char *d, size_t n, int w, int h) {
    int x, y, pw = w / 64;
    if (pw < 1)
        pw = 1;
    memset(d, 0, n);
    for (y = 0; y < h; y++)
        for (x = 0; x < w; x++) {
            int px = x & 63, py = y & 31, bi = (py / 8) * 8 + px / 8, ab = BO32[bi & 31];
            size_t si = ((size_t)y * w + x) * 4,
                   di = (size_t)(((y / 32) * pw + x / 64) * 2048 + ab * 64 + (py & 7) * 8 + (px & 7)) * 4;
            if (si + 4 <= n && di + 4 <= n)
                memcpy(d + di, s + si, 4);
        }
}
void tim_swizzle16(const unsigned char *s, unsigned char *d, size_t n, int w, int h) {
    int x, y, pw = w / 64;
    if (pw < 1)
        pw = 1;
    memset(d, 0, n);
    for (y = 0; y < h; y++)
        for (x = 0; x < w; x++) {
            int px = x & 63, py = y & 63, bi = (py / 8) * 4 + px / 16, ab = BO16[bi & 63];
            size_t si = ((size_t)y * w + x) * 2,
                   di = (size_t)(((y / 64) * pw + x / 64) * 4096 + ab * 128 + (py & 7) * 16 + (px & 15)) * 2;
            if (si + 2 <= n && di + 2 <= n)
                memcpy(d + di, s + si, 2);
        }
}
void tim_unswizzle32(const unsigned char *s, unsigned char *d, size_t n, int w, int h) {
    int x, y, pw = w / 64;
    if (pw < 1)
        pw = 1;
    memset(d, 0, (size_t)w * h * 4);
    for (y = 0; y < h; y++)
        for (x = 0; x < w; x++) {
            int px = x & 63, py = y & 31, bi = (py / 8) * 8 + px / 8, ab = BO32[bi & 31];
            size_t si = (size_t)(((y / 32) * pw + x / 64) * 2048 + ab * 64 + (py & 7) * 8 + (px & 7)) * 4,
                   di = ((size_t)y * w + x) * 4;
            if (si + 4 <= n)
                memcpy(d + di, s + si, 4);
        }
}
void tim_unswizzle16(const unsigned char *s, unsigned char *d, size_t n, int w, int h) {
    int x, y, pw = w / 64;
    if (pw < 1)
        pw = 1;
    memset(d, 0, (size_t)w * h * 2);
    for (y = 0; y < h; y++)
        for (x = 0; x < w; x++) {
            int px = x & 63, py = y & 63, bi = (py / 8) * 4 + px / 16, ab = BO16[bi & 63];
            size_t si = (size_t)(((y / 64) * pw + x / 64) * 4096 + ab * 128 + (py & 7) * 16 + (px & 15)) * 2,
                   di = ((size_t)y * w + x) * 2;
            if (si + 2 <= n)
                memcpy(d + di, s + si, 2);
        }
}
void tim_swizzle_clut8(const unsigned char *s, unsigned char *d) {
    static const int m[4] = {0, 2, 1, 3};
    int i;
    for (i = 0; i < 256; i++) {
        int b = i / 32, z = i % 32, st = z / 8, p = z % 8, j = b * 32 + m[st] * 8 + p;
        memcpy(d + j * 4, s + i * 4, 4);
    }
}
void tim_unswizzle_clut8(const unsigned char *s, unsigned char *d) {
    static const int m[4] = {0, 2, 1, 3};
    int i;
    for (i = 0; i < 256; i++) {
        int b = i / 32, z = i % 32, st = z / 8, p = z % 8, j = b * 32 + m[st] * 8 + p;
        memcpy(d + i * 4, s + j * 4, 4);
    }
}
static int p2(int n) {
    int v = 1;
    while (v < n && v < 32768)
        v <<= 1;
    return v;
}
int tim_pow2_resize(const unsigned char *s, int w, int h, int mode, unsigned char **out, int *ow, int *oh) {
    int nw = w, nh = h, x, y;
    unsigned char *d;
    if (!out || !ow || !oh)
        return 0;
    if (mode == 0) {
        *out = (unsigned char *)malloc((size_t)w * h * 4);
        if (!*out)
            return 0;
        memcpy(*out, s, (size_t)w * h * 4);
        *ow = w;
        *oh = h;
        return 1;
    }
    if (mode == 1) {
        nw = p2(w);
        nh = p2(h);
    } else {
        nw = 1;
        while (nw * 2 <= w)
            nw *= 2;
        nh = 1;
        while (nh * 2 <= h)
            nh *= 2;
    }
    d = tim_rgba_alloc(nw, nh);
    if (!d)
        return 0;
    for (y = 0; y < nh; y++)
        for (x = 0; x < nw; x++) {
            int sx = x * w / nw, sy = y * h / nh;
            memcpy(d + ((size_t)y * nw + x) * 4, s + ((size_t)sy * w + sx) * 4, 4);
        }
    *out = d;
    *ow = nw;
    *oh = nh;
    return 1;
}
void tim_downsample(const unsigned char *s, int w, int h, unsigned char **out, int *ow, int *oh) {
    int nw = w > 1 ? w / 2 : 1, nh = h > 1 ? h / 2 : 1, x, y, c;
    unsigned char *d = tim_rgba_alloc(nw, nh);
    if (!d) {
        *out = NULL;
        return;
    }
    for (y = 0; y < nh; y++)
        for (x = 0; x < nw; x++)
            for (c = 0; c < 4; c++) {
                int x0 = x * 2, y0 = y * 2, n = 0, sum = 0, xx, yy;
                for (yy = y0; yy < y0 + 2 && yy < h; yy++)
                    for (xx = x0; xx < x0 + 2 && xx < w; xx++) {
                        sum += s[((size_t)yy * w + xx) * 4 + c];
                        n++;
                    }
                d[((size_t)y * nw + x) * 4 + c] = (unsigned char)(sum / n);
            }
    *out = d;
    *ow = nw;
    *oh = nh;
}
