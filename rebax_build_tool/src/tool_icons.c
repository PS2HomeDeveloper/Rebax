/*
 *  ____  _____ ____    _    __  __
 * |  _ \| ____| __ )  / \   \ \/ /
 * | |_) |  _| |  _ \ / _ \   \  / 
 * |  _ <| |___| |_) / ___ \  /  \ 
 * |_| \_\_____|____/_/   \_\/_/\_\
 *
 * Rebax Engine
 * Copyright (c) 2026 PS2HomeDeveloper
 *
 * Licensed under the MIT License.
 * See the LICENSE file for details.
 */

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_NO_HDR
#define STBI_NO_LINEAR
#include "../../third_party/stb_image.h"

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "filesystem/rebax_fs.h"
#include "libraries/zip/zip_writer.h"
#include "tool_commands.h"
#include "tool_common.h"

/* ============================================================
 * Adler32
 * ============================================================ */

static uint32_t adler32(const unsigned char *p, size_t n) {
    uint32_t a = 1, b = 0;
    for (size_t i = 0; i < n; i++) {
        a = (a + p[i]) % 65521u;
        b = (b + a) % 65521u;
    }
    return (b << 16) | a;
}

/* ============================================================
 * Deflate (LZ77 + fixed Huffman codes) - used for the PNG atlas
 * ============================================================ */

typedef struct {
    sb_t out;
    uint32_t bitbuf;
    int bitcnt;
} bw_t;

static void bw_bits(bw_t *w, uint32_t value, int n) {
    w->bitbuf |= value << w->bitcnt;
    w->bitcnt += n;
    while (w->bitcnt >= 8) {
        sb_putc(&w->out, (unsigned char)(w->bitbuf & 0xFF));
        w->bitbuf >>= 8;
        w->bitcnt -= 8;
    }
}

/* Huffman codes go into the stream most-significant bit first */
static void bw_huff(bw_t *w, uint32_t code, int n) {
    uint32_t r = 0;
    for (int i = 0; i < n; i++) {
        r = (r << 1) | (code & 1);
        code >>= 1;
    }
    bw_bits(w, r, n);
}

static const int LEN_BASE[29] = { 3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31,
                                  35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258 };
static const int LEN_EXTRA[29] = { 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2,
                                   3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0 };
static const int DIST_BASE[30] = { 1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193,
                                   257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145,
                                   8193, 12289, 16385, 24577 };
static const int DIST_EXTRA[30] = { 0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6,
                                    7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13 };

static void put_literal(bw_t *w, int lit) {
    if (lit < 144) bw_huff(w, (uint32_t)(0x30 + lit), 8);
    else bw_huff(w, (uint32_t)(0x190 + lit - 144), 9);
}

static void put_length(bw_t *w, int len) {
    int i = 28;
    while (LEN_BASE[i] > len) i--;
    int sym = 257 + i;
    if (sym < 280) bw_huff(w, (uint32_t)(sym - 256), 7);
    else bw_huff(w, (uint32_t)(0xC0 + sym - 280), 8);
    if (LEN_EXTRA[i]) bw_bits(w, (uint32_t)(len - LEN_BASE[i]), LEN_EXTRA[i]);
}

static void put_dist(bw_t *w, int dist) {
    int i = 29;
    while (DIST_BASE[i] > dist) i--;
    bw_huff(w, (uint32_t)i, 5);
    if (DIST_EXTRA[i]) bw_bits(w, (uint32_t)(dist - DIST_BASE[i]), DIST_EXTRA[i]);
}

#define HASH_BITS 15
#define HASH_SIZE (1 << HASH_BITS)
#define MAX_CHAIN 128
#define WINDOW 32768

static uint32_t hash3(const unsigned char *p) {
    uint32_t v = ((uint32_t)p[0] << 16) | ((uint32_t)p[1] << 8) | p[2];
    return (v * 2654435761u) >> (32 - HASH_BITS);
}

/* raw deflate stream, one final fixed-Huffman block */
static void deflate_fixed(const unsigned char *in, size_t n, sb_t *out) {
    bw_t w = { { 0, 0, 0 }, 0, 0 };
    int32_t *head = (int32_t *)xmalloc(sizeof(int32_t) * HASH_SIZE);
    int32_t *prev = (int32_t *)xmalloc(sizeof(int32_t) * (n ? n : 1));
    for (int i = 0; i < HASH_SIZE; i++) head[i] = -1;

    bw_bits(&w, 1, 1); /* BFINAL */
    bw_bits(&w, 1, 2); /* BTYPE = 01 (fixed Huffman) */

    size_t i = 0;
    while (i < n) {
        int best_len = 0, best_dist = 0;
        size_t max_len = n - i < 258 ? n - i : 258;

        if (i + 3 <= n) {
            int32_t cand = head[hash3(in + i)];
            int chain = 0;
            while (cand >= 0 && (i - (size_t)cand) <= WINDOW && chain++ < MAX_CHAIN) {
                size_t l = 0;
                while (l < max_len && in[(size_t)cand + l] == in[i + l]) l++;
                if ((int)l > best_len) {
                    best_len = (int)l;
                    best_dist = (int)(i - (size_t)cand);
                    if (l == max_len) break;
                }
                cand = prev[cand];
            }
        }

        size_t advance;
        if (best_len >= 3) {
            put_length(&w, best_len);
            put_dist(&w, best_dist);
            advance = (size_t)best_len;
        } else {
            put_literal(&w, in[i]);
            advance = 1;
        }

        for (size_t k = 0; k < advance; k++) {
            size_t pos = i + k;
            if (pos + 3 <= n) {
                uint32_t h = hash3(in + pos);
                prev[pos] = head[h];
                head[h] = (int32_t)pos;
            }
        }
        i += advance;
    }

    bw_huff(&w, 0, 7); /* end of block (symbol 256) */
    if (w.bitcnt > 0) bw_bits(&w, 0, 8 - w.bitcnt);

    sb_append(out, w.out.data, w.out.len);
    sb_free(&w.out);
    free(head);
    free(prev);
}

static void zlib_compress(const unsigned char *in, size_t n, sb_t *out) {
    sb_putc(out, 0x78);
    sb_putc(out, 0x01);
    deflate_fixed(in, n, out);
    put_be32(out, adler32(in, n));
}

/* ============================================================
 * PNG writer (RGBA, 8 bit)
 * ============================================================ */

static void png_chunk(sb_t *out, const char *type, const unsigned char *data, size_t len) {
    put_be32(out, (uint32_t)len);
    size_t start = out->len;
    sb_append(out, type, 4);
    if (len) sb_append(out, data, len);
    put_be32(out, zip_crc32(0, out->data + start, len + 4));
}

static void write_png_rgba(const char *path, const unsigned char *rgba, int w, int h) {
    size_t stride = (size_t)w * 4;
    unsigned char *raw = (unsigned char *)xmalloc((stride + 1) * (size_t)h);
    for (int y = 0; y < h; y++) {
        raw[(size_t)y * (stride + 1)] = 0; /* filter: none */
        memcpy(raw + (size_t)y * (stride + 1) + 1, rgba + (size_t)y * stride, stride);
    }

    sb_t z = { 0, 0, 0 };
    zlib_compress(raw, (stride + 1) * (size_t)h, &z);
    free(raw);

    sb_t png = { 0, 0, 0 };
    static const unsigned char sig[8] = { 0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A };
    sb_append(&png, sig, 8);

    unsigned char ihdr[13];
    ihdr[0] = (unsigned char)(w >> 24); ihdr[1] = (unsigned char)(w >> 16);
    ihdr[2] = (unsigned char)(w >> 8);  ihdr[3] = (unsigned char)w;
    ihdr[4] = (unsigned char)(h >> 24); ihdr[5] = (unsigned char)(h >> 16);
    ihdr[6] = (unsigned char)(h >> 8);  ihdr[7] = (unsigned char)h;
    ihdr[8] = 8;  /* bit depth */
    ihdr[9] = 6;  /* color type: RGBA */
    ihdr[10] = 0; ihdr[11] = 0; ihdr[12] = 0;
    png_chunk(&png, "IHDR", ihdr, 13);
    png_chunk(&png, "IDAT", z.data, z.len);
    png_chunk(&png, "IEND", NULL, 0);

    write_file(path, png.data, png.len);
    sb_free(&z);
    sb_free(&png);
}

/* ============================================================
 * Icon atlas + icon names
 * ============================================================ */

#define ATLAS_SIZE 256
#define ICON_CELL 16
#define ATLAS_COLUMNS (ATLAS_SIZE / ICON_CELL)

/* source-over compositing of one icon onto the atlas at (x, y),
 * clipped to the atlas bounds (same result as magick composite) */
static void blit_icon(unsigned char *atlas, const unsigned char *icon, int iw, int ih, int x, int y) {
    for (int sy = 0; sy < ih; sy++) {
        int dy = y + sy;
        if (dy >= ATLAS_SIZE) break;
        for (int sx = 0; sx < iw; sx++) {
            int dx = x + sx;
            if (dx >= ATLAS_SIZE) break;
            const unsigned char *s = icon + ((size_t)sy * (size_t)iw + (size_t)sx) * 4;
            unsigned char *d = atlas + ((size_t)dy * ATLAS_SIZE + (size_t)dx) * 4;
            if (s[3] == 0) continue;
            if (s[3] == 255 || d[3] == 0) {
                memcpy(d, s, 4);
            } else {
                float fs = (float)s[3] / 255.0f;
                float fd = (float)d[3] / 255.0f;
                float fo = fs + fd * (1.0f - fs);
                for (int c = 0; c < 3; c++) {
                    float v = ((float)s[c] * fs + (float)d[c] * fd * (1.0f - fs)) / fo;
                    d[c] = (unsigned char)(v + 0.5f);
                }
                d[3] = (unsigned char)(fo * 255.0f + 0.5f);
            }
        }
    }
}

int cmd_icon_atlas(int argc, char **argv) {
    if (argc < 3) die("usage: icon-atlas <out.png> <icon.png>...");
    const char *out_path = argv[2];
    int count = argc - 3;
    if (count > ATLAS_COLUMNS * ATLAS_COLUMNS) {
        die("too many icons (%d): the %dx%d atlas holds at most %d", count, ATLAS_SIZE,
            ATLAS_SIZE, ATLAS_COLUMNS * ATLAS_COLUMNS);
    }

    unsigned char *atlas = (unsigned char *)calloc((size_t)ATLAS_SIZE * ATLAS_SIZE * 4, 1);
    if (!atlas) die("out of memory");

    for (int i = 0; i < count; i++) {
        const char *path = argv[3 + i];
        int w = 0, h = 0, comp = 0;
        unsigned char *pix = stbi_load(path, &w, &h, &comp, 4);
        if (!pix) die("cannot read PNG '%s': %s", path, stbi_failure_reason());
        if (w != ICON_CELL || h != ICON_CELL) {
            stbi_image_free(pix);
            free(atlas);
            die("icon '%s' is %dx%d; every engine icon must be exactly %dx%d", path, w, h,
                ICON_CELL, ICON_CELL);
        }
        blit_icon(atlas, pix, w, h, (i % ATLAS_COLUMNS) * ICON_CELL, (i / ATLAS_COLUMNS) * ICON_CELL);
        stbi_image_free(pix);
    }

    write_png_rgba(out_path, atlas, ATLAS_SIZE, ATLAS_SIZE);
    free(atlas);
    return 0;
}


static void write_icon_pages_header(const char *header_path, int page_count) {
    sb_t out = { 0, 0, 0 };
    sb_puts(&out, "/* Generated by rebax-tool icon-atlas-pages; do not edit. */\n");
    sb_puts(&out, "#ifndef ICON_ATLAS_PAGES_H\n#define ICON_ATLAS_PAGES_H\n\n");
    sb_printf(&out, "#define ICON_ATLAS_PAGE_COUNT %d\n\n", page_count);
    sb_puts(&out, "#endif\n");
    write_file(header_path, out.data, out.len);
    sb_free(&out);
}

int cmd_icon_atlas_pages(int argc, char **argv) {
    if (argc < 4) die("usage: icon-atlas-pages <out_dir> <out_header> <icon.png>...");
    const char *out_dir = argv[2];
    const char *header_path = argv[3];
    int count = argc - 4;
    if (count <= 0) die("no icon sources were provided");
    if (!rebax_fs_mkdir_p(out_dir)) die("cannot create icon atlas directory '%s'", out_dir);

    /* Remove old generated pages, but never touch icons_src. */
    for (int page = 1; page <= 4096; page++) {
        char old_path[1600];
        snprintf(old_path, sizeof(old_path), "%s/icons%d.png", out_dir, page);
        if (rebax_fs_exists(old_path)) {
            if (!rebax_fs_remove_recursive(old_path)) die("cannot remove old atlas '%s'", old_path);
        }
    }

    int page_count = (count + ATLAS_COLUMNS * ATLAS_COLUMNS - 1) /
                     (ATLAS_COLUMNS * ATLAS_COLUMNS);
    for (int page = 0; page < page_count; page++) {
        unsigned char *atlas = (unsigned char *)calloc((size_t)ATLAS_SIZE * ATLAS_SIZE * 4, 1);
        if (!atlas) die("out of memory");
        int first = page * ATLAS_COLUMNS * ATLAS_COLUMNS;
        int last = first + ATLAS_COLUMNS * ATLAS_COLUMNS;
        if (last > count) last = count;
        for (int i = first; i < last; i++) {
            const char *path = argv[4 + i];
            int w = 0, h = 0, comp = 0;
            unsigned char *pix = stbi_load(path, &w, &h, &comp, 4);
            if (!pix) {
                free(atlas);
                die("cannot read icon '%s': %s", path, stbi_failure_reason());
            }
            if (w != ICON_CELL || h != ICON_CELL) {
                stbi_image_free(pix);
                free(atlas);
                die("icon '%s' is %dx%d; every engine icon must be exactly %dx%d", path, w, h,
                    ICON_CELL, ICON_CELL);
            }
            int slot = i - first;
            blit_icon(atlas, pix, w, h, (slot % ATLAS_COLUMNS) * ICON_CELL,
                      (slot / ATLAS_COLUMNS) * ICON_CELL);
            stbi_image_free(pix);
        }
        char out_path[1600];
        snprintf(out_path, sizeof(out_path), "%s/icons%d.png", out_dir, page + 1);
        write_png_rgba(out_path, atlas, ATLAS_SIZE, ATLAS_SIZE);
        free(atlas);
    }
    write_icon_pages_header(header_path, page_count);
    return 0;
}

int cmd_icon_names(int argc, char **argv) {
    if (argc < 3) die("usage: icon-names <out.h> <icon.png>...");
    int count = argc - 3;

    sb_t out = { 0, 0, 0 };
    sb_puts(&out, "/* Automatically generated during the build from embedded/images/icons/icons_src/ - do not edit manually */\n");
    sb_puts(&out, "#ifndef ICON_NAMES_H\n#define ICON_NAMES_H\n");

    for (int i = 0; i < count; i++) {
        char base[1600], name[1600], safe[1600];
        rebax_fs_basename(argv[3 + i], base, (int)sizeof(base));
        rebax_fs_basename_no_ext(argv[3 + i], name, (int)sizeof(name));
        (void)base;

        /* tr -- '- ' '__' | tr -dc 'A-Za-z0-9_' */
        size_t k = 0;
        for (size_t j = 0; name[j] && k < sizeof(safe) - 1; j++) {
            char c = name[j];
            if (c == '-' || c == ' ') c = '_';
            if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_') {
                safe[k++] = c;
            }
        }
        safe[k] = '\0';
        sb_printf(&out, "#define ICON_%s %d\n", safe, i);
    }
    sb_printf(&out, "#define ICON_COUNT %d\n", count);
    sb_puts(&out, "#endif\n");

    write_file(argv[2], out.data, out.len);
    sb_free(&out);
    return 0;
}

