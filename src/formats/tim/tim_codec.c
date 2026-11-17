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
 * See the LICENSE file for details.
 */
#include "formats/tim/tim_codec.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define PSM_CT32 0x00
#define PSM_CT24 0x01
#define PSM_CT16 0x02
#define PSM_CT16S 0x0a
#define PSM_T8 0x13
#define PSM_T4 0x14
#define PSM_T8H 0x1b
#define PSM_T4HL 0x24
#define PSM_T4HH 0x2c

static unsigned r16(const unsigned char *p) { return p[0] | ((unsigned)p[1] << 8); }
static unsigned long r32(const unsigned char *p) { return (unsigned long)p[0] | ((unsigned long)p[1] << 8) | ((unsigned long)p[2] << 16) | ((unsigned long)p[3] << 24); }
static unsigned long long r64(const unsigned char *p) { unsigned long long v = 0; int i; for (i = 0; i < 8; i++) v |= (unsigned long long)p[i] << (i * 8); return v; }
static unsigned char alpha_ps2(unsigned v) { v = (v * 255u + 64u) / 128u; return (unsigned char)(v > 255u ? 255u : v); }
static void unpack5551(unsigned v, unsigned char *o, int ps1) {
    unsigned r = v & 31u, g = (v >> 5) & 31u, b = (v >> 10) & 31u;
    o[0] = (unsigned char)((r << 3) | (r >> 2));
    o[1] = (unsigned char)((g << 3) | (g >> 2));
    o[2] = (unsigned char)((b << 3) | (b >> 2));
    if (ps1) {
        if (v == 0) o[3] = 0;
        else if (v & 0x8000u) o[3] = (r == 0 && g == 0 && b == 0) ? 255 : 128;
        else o[3] = 255;
    } else o[3] = (v & 0x8000u) ? 255 : 0;
}
static int read_all(const char *path, unsigned char **data, size_t *size) {
    FILE *f = fopen(path, "rb"); long n;
    if (!f || fseek(f, 0, SEEK_END) != 0) { if (f) fclose(f); return 0; }
    n = ftell(f); if (n < 0 || fseek(f, 0, SEEK_SET) != 0) { fclose(f); return 0; }
    *data = (unsigned char *)malloc((size_t)n); if (!*data) { fclose(f); return 0; }
    *size = (size_t)n;
    if (fread(*data, 1, *size, f) != *size) { free(*data); *data = NULL; fclose(f); return 0; }
    fclose(f); return 1;
}
static unsigned char *alloc_rgba(int w, int h) {
    size_t n;
    if (w <= 0 || h <= 0 || (size_t)w > ((size_t)-1) / (size_t)h) return NULL;
    n = (size_t)w * (size_t)h; if (n > ((size_t)-1) / 4) return NULL;
    return (unsigned char *)malloc(n * 4);
}
static void clut8_unswizzle(const unsigned char *in, unsigned char *out) {
    static const int map[4] = {0, 2, 1, 3}; int i;
    for (i = 0; i < 256; i++) {
        int block = i / 32, inner = i % 32, stripe = inner / 8, pos = inner % 8;
        int j = block * 32 + map[stripe] * 8 + pos;
        memcpy(out + i * 4, in + j * 4, 4);
    }
}
static unsigned char *unswizzle32(const unsigned char *src, size_t n, int w, int h) {
    static const unsigned char bo[32] = {0,1,4,5,16,17,20,21,2,3,6,7,18,19,22,23,8,9,12,13,24,25,28,29,10,11,14,15,26,27,30,31};
    unsigned char *dst = (unsigned char *)calloc(n ? n : 1, 1); int x, y, pages = w / 64;
    if (!dst) return NULL; if (pages < 1) pages = 1;
    for (y = 0; y < h; y++) for (x = 0; x < w; x++) {
        int px = x & 63, py = y & 31, bi = (py / 8) * 8 + (px / 8), ab = bo[bi & 31];
        size_t si = (size_t)(((y / 32) * pages + x / 64) * 2048 + ab * 64 + (py & 7) * 8 + (px & 7)) * 4;
        size_t di = ((size_t)y * (size_t)w + (size_t)x) * 4;
        if (si + 4 <= n) memcpy(dst + di, src + si, 4);
    }
    return dst;
}
static unsigned char *unswizzle16(const unsigned char *src, size_t n, int w, int h) {
    static const unsigned char bo[64] = {0,2,8,10,32,34,40,42,1,3,9,11,33,35,41,43,4,6,12,14,36,38,44,46,5,7,13,15,37,39,45,47,16,18,24,26,48,50,56,58,17,19,25,27,49,51,57,59,20,22,28,30,52,54,60,62,21,23,29,31,53,55,61,63};
    unsigned char *dst = (unsigned char *)calloc(n ? n : 1, 1); int x, y, pages = w / 64;
    if (!dst) return NULL; if (pages < 1) pages = 1;
    for (y = 0; y < h; y++) for (x = 0; x < w; x++) {
        int px = x & 63, py = y & 63, bi = (py / 8) * 4 + px / 16, ab = bo[bi & 63];
        size_t si = (size_t)(((y / 64) * pages + x / 64) * 4096 + ab * 128 + (py & 7) * 16 + (px & 15)) * 2;
        size_t di = ((size_t)y * (size_t)w + (size_t)x) * 2;
        if (si + 2 <= n) memcpy(dst + di, src + si, 2);
    }
    return dst;
}
static void decode_tim(const unsigned char *d, size_t n, rebax_decoded_image_t *o) {
    unsigned flag, mode, off = 8, len, wu, h, w, i; const unsigned char *clut = NULL, *pix;
    unsigned cw = 0, ch = 0; unsigned char *pal = NULL;
    if (n < 8 || r32(d) != 0x10u) return;
    flag = (unsigned)r32(d + 4); mode = flag & 3u;
    if (flag & 8u) { if (n - off < 12) return; len = (unsigned)r32(d + off); if (len < 12 || len > n - off) return; cw = r16(d + off + 8); ch = r16(d + off + 10); if ((size_t)cw * ch * 2 > len - 12) return; clut = d + off + 12; off += len; }
    if (n - off < 12) return; len = (unsigned)r32(d + off); wu = r16(d + off + 8); h = r16(d + off + 10); if (len < 12 || len > n - off) return;
    if (mode == 0) w = wu * 4; else if (mode == 1) w = wu * 2; else if (mode == 2) w = wu; else if (mode == 3) w = (wu * 2) / 3; else return;
    o->rgba = alloc_rgba((int)w, (int)h); if (!o->rgba) return; o->width = (int)w; o->height = (int)h; pix = d + off + 12;
    if (mode == 2) for (i = 0; i < w * h; i++) unpack5551(r16(pix + i * 2), o->rgba + i * 4, 1);
    else if (mode == 3) for (i = 0; i < w * h; i++) { memcpy(o->rgba + i * 4, pix + i * 3, 3); o->rgba[i * 4 + 3] = 255; }
    else if (clut && (mode == 0 || mode == 1)) {
        unsigned count = mode == 0 ? 16 : 256; pal = (unsigned char *)malloc(count * 4); if (!pal) return;
        for (i = 0; i < count; i++) unpack5551(r16(clut + i * 2), pal + i * 4, 1);
        for (i = 0; i < w * h; i++) { unsigned k = mode == 1 ? pix[i] : ((i & 1) ? pix[i / 2] >> 4 : pix[i / 2] & 15); memcpy(o->rgba + i * 4, pal + k * 4, 4); }
        free(pal);
    }
}
static void decode_tim2(const unsigned char *d, size_t n, rebax_decoded_image_t *o) {
    const unsigned char *h, *pix, *clut; unsigned long block, cs, is; unsigned w, hh, psm, cpsm, csm, i, np; size_t start; unsigned char *linear = NULL, *pal = NULL;
    if (n < 64 || memcmp(d, "TIM2", 4) != 0) return;
    h = d + 16; block = r32(h); cs = r32(h + 4); is = r32(h + 8); if (r16(h + 12) < 48 || 16u + block > n) return;
    w = r16(h + 20); hh = r16(h + 22); psm = (unsigned)((r64(h + 24) >> 20) & 63u); cpsm = (unsigned)((r64(h + 24) >> 51) & 15u); csm = (unsigned)((r64(h + 24) >> 55) & 1u);
    start = 16u + r16(h + 12); if (start > n || is > n - start || cs > n - start - is) return; pix = d + start; clut = pix + is; np = w * hh;
    o->rgba = alloc_rgba((int)w, (int)hh); if (!o->rgba) return; o->width = (int)w; o->height = (int)hh;
    if (r16(h + 12) && d[5] == 1) { if (psm == PSM_CT32 || psm == PSM_T8H || psm == PSM_T4HL || psm == PSM_T4HH) linear = unswizzle32(pix, is, (int)w, (int)hh); else if (psm == PSM_CT16 || psm == PSM_CT16S) linear = unswizzle16(pix, is, (int)w, (int)hh); if (linear) pix = linear; }
    if (psm == PSM_CT32) for (i = 0; i < np; i++) { memcpy(o->rgba + i * 4, pix + i * 4, 3); o->rgba[i * 4 + 3] = alpha_ps2(pix[i * 4 + 3]); }
    else if (psm == PSM_CT24) for (i = 0; i < np; i++) { memcpy(o->rgba + i * 4, pix + i * 3, 3); o->rgba[i * 4 + 3] = 255; }
    else if (psm == PSM_CT16 || psm == PSM_CT16S) for (i = 0; i < np; i++) unpack5551(r16(pix + i * 2), o->rgba + i * 4, 0);
    else if (psm == PSM_T8 || psm == PSM_T4 || psm == PSM_T8H || psm == PSM_T4HL || psm == PSM_T4HH) {
        unsigned count = (psm == PSM_T8 || psm == PSM_T8H) ? 256 : 16, entry = cpsm == PSM_CT16 || cpsm == PSM_CT16S ? 2 : 4;
        if (cs < (unsigned long)(count * entry)) { free(linear); free(o->rgba); o->rgba = NULL; return; }
        pal = (unsigned char *)malloc(count * 4); if (!pal) { free(linear); free(o->rgba); o->rgba = NULL; return; }
        for (i = 0; i < count; i++) { if (entry == 2) unpack5551(r16(clut + i * 2), pal + i * 4, 0); else { memcpy(pal + i * 4, clut + i * 4, 3); pal[i * 4 + 3] = cpsm == PSM_CT24 ? 255 : alpha_ps2(clut[i * 4 + 3]); } }
        if (count == 256 && csm == 0) { unsigned char copy[1024]; memcpy(copy, pal, 1024); clut8_unswizzle(copy, pal); }
        for (i = 0; i < np; i++) { unsigned k; if (psm == PSM_T8H) k = pix[i * 4 + 3]; else if (psm == PSM_T4HL) k = pix[i * 4 + 3] & 15; else if (psm == PSM_T4HH) k = (pix[i * 4 + 3] >> 4) & 15; else if (count == 256) k = pix[i]; else k = (i & 1) ? pix[i / 2] >> 4 : pix[i / 2] & 15; memcpy(o->rgba + i * 4, pal + k * 4, 4); }
        free(pal);
    } else { free(o->rgba); o->rgba = NULL; }
    free(linear);
}
int rebax_tim_decode_file(const char *path, rebax_decoded_image_t *out) {
    unsigned char *d = NULL; size_t n = 0; const char *p;
    if (!out || !path) return 0; memset(out, 0, sizeof(*out)); if (!read_all(path, &d, &n)) return 0;
    p = strrchr(path, '.'); if (p && !strcasecmp(p, ".tim")) decode_tim(d, n, out); else if (p && (!strcasecmp(p, ".tm2") || !strcasecmp(p, ".tim2"))) decode_tim2(d, n, out);
    if (!out->rgba && n >= 4 && memcmp(d, "TIM2", 4) == 0) decode_tim2(d, n, out);
    if (!out->rgba && n >= 4 && r32(d) == 0x10u) decode_tim(d, n, out);
    free(d); return out->rgba != NULL;
}
void rebax_tim_decoded_free(rebax_decoded_image_t *image) { if (image) { free(image->rgba); memset(image, 0, sizeof(*image)); } }
int rebax_tim_write_ps1_16(const char *path, const unsigned char *rgba, int width, int height) {
    FILE *f; unsigned i; unsigned long length; unsigned r, g, b, a; unsigned short value;
    if (!path || !rgba || width <= 0 || height <= 0 || width > 65535 || height > 65535) return 0;
    length = 12ul + (unsigned long)width * (unsigned long)height * 2ul;
    f = fopen(path, "wb"); if (!f) return 0;
    if (fwrite("\x10\0\0\0\x02\0\0\0", 1, 8, f) != 8 || fwrite(&length, 4, 1, f) != 1) { fclose(f); return 0; }
    { unsigned short rect[4] = {0, 0, (unsigned short)width, (unsigned short)height}; if (fwrite(rect, sizeof(rect), 1, f) != 1) { fclose(f); return 0; } }
    for (i = 0; i < (unsigned)width * (unsigned)height; i++) {
        r = rgba[i * 4] >> 3; g = rgba[i * 4 + 1] >> 3; b = rgba[i * 4 + 2] >> 3; a = rgba[i * 4 + 3];
        if (a == 0) value = 0;
        else { value = (unsigned short)(((a < 255 ? 1u : 0u) << 15) | (b << 10) | (g << 5) | r); if (value == 0) value = 0x8000; }
        if (fwrite(&value, sizeof(value), 1, f) != 1) { fclose(f); return 0; }
    }
    return fclose(f) == 0;
}
int rebax_image_format_supported(rebax_image_platform_t p, const char *e) {
    if (!e) return 0;
    if (p == REBAX_IMAGE_PLATFORM_PS1) return !strcasecmp(e, ".tim") || !strcasecmp(e, "tim");
    if (p == REBAX_IMAGE_PLATFORM_PS2) return !strcasecmp(e, ".tm2") || !strcasecmp(e, "tm2") || !strcasecmp(e, ".tim2") || !strcasecmp(e, "tim2") || !strcasecmp(e, ".raw") || !strcasecmp(e, "raw") || !strcasecmp(e, ".tex") || !strcasecmp(e, "tex") || !strcasecmp(e, ".png") || !strcasecmp(e, "png");
    return !strcasecmp(e, ".png") || !strcasecmp(e, "png") || !strcasecmp(e, ".jpg") || !strcasecmp(e, "jpg") || !strcasecmp(e, ".jpeg") || !strcasecmp(e, "jpeg") || !strcasecmp(e, ".bmp") || !strcasecmp(e, "bmp") || !strcasecmp(e, ".tga") || !strcasecmp(e, "tga") || !strcasecmp(e, ".tif") || !strcasecmp(e, "tif") || !strcasecmp(e, ".tiff") || !strcasecmp(e, "tiff");
}
int rebax_image_format_rank(rebax_image_platform_t p, const char *e) {
    if (!rebax_image_format_supported(p, e)) return -1;
    if (p == REBAX_IMAGE_PLATFORM_PS2) { if (!strcasecmp(e, ".tm2") || !strcasecmp(e, "tm2") || !strcasecmp(e, ".tim2") || !strcasecmp(e, "tim2")) return 0; if (!strcasecmp(e, ".raw") || !strcasecmp(e, "raw")) return 1; if (!strcasecmp(e, ".tex") || !strcasecmp(e, "tex")) return 2; return 3; }
    return 0;
}
const char *rebax_image_best_format(rebax_image_platform_t p) { return p == REBAX_IMAGE_PLATFORM_PS1 ? ".tim" : p == REBAX_IMAGE_PLATFORM_PS2 ? ".tm2" : ".png"; }
