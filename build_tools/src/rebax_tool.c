/*
 * ============================================================
 * rebax_tool.c  -  Rebax build tool (single executable)
 * ============================================================
 * One small program that replaces every external command the
 * Makefile used to call (mkdir, rm, cp, find, wc, tar, awk,
 * basename, tr, magick).  The Makefile builds it once with the
 * host C compiler and then calls it for every build step, so the
 * build behaves the same on Windows CMD, Linux and macOS.
 *
 * Build (host compiler, from the project root; the Makefile does this itself):
 *   cc -std=c99 -O2 -o build-tools/rebax-tool build-tools/src/rebax_tool.c \
 *      build-tools/src/rebax_fs.c build-tools/src/xz_embedded.c -lm
 *
 * Commands:
 *   rebax-tool mkdir <dir>...                      like  mkdir -p
 *   rebax-tool rm <path>...                        like  rm -rf
 *   rebax-tool cp <src> <dst>                      like  cp
 *   rebax-tool exists <path>...                    prints 1 if all exist, else 0
 *   rebax-tool dir-empty <dir>                     prints 1 if missing or without files, else 0
 *   rebax-tool which <program>                     prints the full path found in PATH (exit 1 if none)
 *   rebax-tool extract <archive.tar.xz> <dest>     like  tar -xJf
 *   rebax-tool pack <out.tar.xz> <base> <entry>    like  tar -cJf out -C base entry
 *   rebax-tool icon-names <out.h> <icon.png>...    generates icon_names.h
 *   rebax-tool icon-atlas <out.png> <icon.png>...  builds the 256x256 icon atlas
 *   rebax-tool node-registry <registry.h> <types.h> <node.c>...
 *   rebax-tool node-editor-registry <out.h> <node_editor.c>...
 *
 * Exit code 0 = success, non-zero = failure (message on stderr).
 * exists / dir-empty answer through stdout so Make can read the answer
 * with $(shell ...); they exit 0 whatever the answer is.
 *
 * Notes:
 *  - Uses no external program and no library except rebax_fs
 *    (file system + tar.xz extraction), xz_embedded, and stb_image.h
 *    from the project's src/ folder (PNG decoding only).
 *  - The PNG writer and the deflate compressor are written here.
 *  - The .xz writer uses the public-domain 7-Zip LZMA2 encoder at a
 *    fixed high-compression setting.  The command interface deliberately
 *    exposes no compression settings: every build gets the same result.
 * ============================================================
 */

#define _POSIX_C_SOURCE 200809L

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "rebax_fs.h"
#include "lzma2_encoder.h"

#ifndef _WIN32
#include <unistd.h>
#endif

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_NO_HDR
#define STBI_NO_LINEAR
#include "../../src/stb_image.h"

/* ============================================================
 * Small utilities
 * ============================================================ */

static void die(const char *fmt, ...) {
    va_list ap;
    fprintf(stderr, "[rebax-tool] error: ");
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
    exit(1);
}

static void *xmalloc(size_t n) {
    void *p = malloc(n ? n : 1);
    if (!p) die("out of memory");
    return p;
}

static void *xrealloc(void *p, size_t n) {
    void *q = realloc(p, n ? n : 1);
    if (!q) die("out of memory");
    return q;
}

static char *xstrdup(const char *s) {
    size_t n = strlen(s);
    char *p = (char *)xmalloc(n + 1);
    memcpy(p, s, n + 1);
    return p;
}

/* growable byte/string buffer */
typedef struct {
    unsigned char *data;
    size_t len;
    size_t cap;
} sb_t;

static void sb_reserve(sb_t *b, size_t extra) {
    if (b->len + extra + 1 > b->cap) {
        size_t nc = b->cap ? b->cap : 256;
        while (nc < b->len + extra + 1) nc *= 2;
        b->data = (unsigned char *)xrealloc(b->data, nc);
        b->cap = nc;
    }
}

static void sb_append(sb_t *b, const void *src, size_t n) {
    sb_reserve(b, n);
    if (n) memcpy(b->data + b->len, src, n);
    b->len += n;
    b->data[b->len] = 0;
}

static void sb_puts(sb_t *b, const char *s) { sb_append(b, s, strlen(s)); }

static void sb_putc(sb_t *b, unsigned char c) { sb_append(b, &c, 1); }

static void sb_printf(sb_t *b, const char *fmt, ...) {
    va_list ap, ap2;
    va_start(ap, fmt);
    va_copy(ap2, ap);
    int n = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (n < 0) die("format error");
    sb_reserve(b, (size_t)n);
    vsnprintf((char *)b->data + b->len, (size_t)n + 1, fmt, ap2);
    va_end(ap2);
    b->len += (size_t)n;
}

static void sb_free(sb_t *b) {
    free(b->data);
    b->data = NULL;
    b->len = b->cap = 0;
}

static void put_le32(sb_t *b, uint32_t v) {
    unsigned char t[4] = { (unsigned char)v, (unsigned char)(v >> 8),
                           (unsigned char)(v >> 16), (unsigned char)(v >> 24) };
    sb_append(b, t, 4);
}

static void put_be32(sb_t *b, uint32_t v) {
    unsigned char t[4] = { (unsigned char)(v >> 24), (unsigned char)(v >> 16),
                           (unsigned char)(v >> 8), (unsigned char)v };
    sb_append(b, t, 4);
}

/* reads a whole file; result is NUL-terminated (extra byte not counted) */
static unsigned char *read_file(const char *path, size_t *out_len) {
    FILE *f = fopen(path, "rb");
    if (!f) die("cannot open '%s'", path);
    sb_t b = { 0, 0, 0 };
    unsigned char chunk[65536];
    size_t n;
    while ((n = fread(chunk, 1, sizeof(chunk), f)) > 0) sb_append(&b, chunk, n);
    if (ferror(f)) { fclose(f); die("read error on '%s'", path); }
    fclose(f);
    if (!b.data) sb_append(&b, "", 0);
    *out_len = b.len;
    return b.data;
}

static int is_sep(char c) { return c == '/' || c == '\\'; }

static void ensure_parent_dir(const char *path) {
    char *tmp = xstrdup(path);
    size_t i = strlen(tmp);
    while (i > 0 && !is_sep(tmp[i - 1])) i--;
    if (i > 1) {
        tmp[i - 1] = '\0';
        if (!rebax_fs_mkdir_p(tmp)) die("cannot create directory '%s'", tmp);
    }
    free(tmp);
}

static void write_file(const char *path, const void *data, size_t len) {
    ensure_parent_dir(path);
    FILE *f = fopen(path, "wb");
    if (!f) die("cannot write '%s'", path);
    if (len && fwrite(data, 1, len, f) != len) { fclose(f); die("write error on '%s'", path); }
    if (fclose(f) != 0) die("write error on '%s'", path);
}

/* ============================================================
 * CRC32 / Adler32
 * ============================================================ */

static uint32_t g_crc_table[256];
static int g_crc_ready = 0;

static void crc_init(void) {
    if (g_crc_ready) return;
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t c = i;
        for (int k = 0; k < 8; k++) c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
        g_crc_table[i] = c;
    }
    g_crc_ready = 1;
}

static uint32_t crc32_update(uint32_t crc, const unsigned char *p, size_t n) {
    crc_init();
    crc = ~crc;
    for (size_t i = 0; i < n; i++) crc = g_crc_table[(crc ^ p[i]) & 0xFF] ^ (crc >> 8);
    return ~crc;
}

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
    put_be32(out, crc32_update(0, out->data + start, len + 4));
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

static int cmd_icon_atlas(int argc, char **argv) {
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
            fprintf(stderr, "[rebax-tool] warning: '%s' is %dx%d, expected %dx%d\n", path, w, h,
                    ICON_CELL, ICON_CELL);
        }
        blit_icon(atlas, pix, w, h, (i % ATLAS_COLUMNS) * ICON_CELL, (i / ATLAS_COLUMNS) * ICON_CELL);
        stbi_image_free(pix);
    }

    write_png_rgba(out_path, atlas, ATLAS_SIZE, ATLAS_SIZE);
    free(atlas);
    return 0;
}

static int cmd_icon_names(int argc, char **argv) {
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

/* ============================================================
 * Node registry generators (port of the two awk scripts)
 * ============================================================ */

static int is_ws(char c) { return c == ' ' || c == '\t'; }

typedef struct {
    char *name;
    sb_t body;
    int count;
} prop_t;

typedef struct {
    char *type, *name, *icon, *props;
} node_entry_t;

typedef struct {
    prop_t *props;
    int prop_count;
    node_entry_t *entries;
    int entry_count;
} registry_t;

static prop_t *find_prop(registry_t *r, const char *name) {
    for (int i = 0; i < r->prop_count; i++) {
        if (strcmp(r->props[i].name, name) == 0) return &r->props[i];
    }
    return NULL;
}

static prop_t *begin_prop(registry_t *r, const char *name) {
    prop_t *p = find_prop(r, name);
    if (p) {
        p->body.len = 0;
        if (p->body.data) p->body.data[0] = 0;
        p->count = 0;
        return p;
    }
    r->props = (prop_t *)xrealloc(r->props, sizeof(prop_t) * (size_t)(r->prop_count + 1));
    p = &r->props[r->prop_count++];
    p->name = xstrdup(name);
    p->body.data = NULL;
    p->body.len = p->body.cap = 0;
    p->count = 0;
    return p;
}

/* ^static[ \t]+const[ \t]+node_property_t[ \t]+NAME\[\][ \t]*=[ \t]*\{ */
static int match_prop_begin(const char *line, char *name_out, size_t name_size) {
    const char *p = line;
    static const char *words[3] = { "static", "const", "node_property_t" };
    for (int i = 0; i < 3; i++) {
        size_t wl = strlen(words[i]);
        if (strncmp(p, words[i], wl) != 0) return 0;
        p += wl;
        if (!is_ws(*p)) return 0;
        while (is_ws(*p)) p++;
    }
    const char *start = p;
    while ((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9') || *p == '_') p++;
    size_t n = (size_t)(p - start);
    if (n == 0 || n >= name_size) return 0;
    if (p[0] != '[' || p[1] != ']') return 0;
    p += 2;
    while (is_ws(*p)) p++;
    if (*p != '=') return 0;
    p++;
    while (is_ws(*p)) p++;
    if (*p != '{') return 0;
    memcpy(name_out, start, n);
    name_out[n] = '\0';
    return 1;
}

/* pointer just after the LAST occurrence of tag (followed by whitespace when
 * need_ws), skipping the whitespace run. Returns line unchanged if none. */
static char *after_last_tag(char *line, const char *tag, int need_ws) {
    size_t tl = strlen(tag);
    char *best = NULL;
    for (char *p = strstr(line, tag); p; p = strstr(p + 1, tag)) {
        if (need_ws && !is_ws(p[tl])) continue;
        best = p;
    }
    if (!best) return line;
    char *q = best + tl;
    while (is_ws(*q)) q++;
    return q;
}

static void cut_comment_end(char *s) {
    char *e = strstr(s, "*/");
    if (!e) return;
    while (e > s && is_ws(e[-1])) e--;
    *e = '\0';
}

/* iterates the lines of a file (CR removed); fn returns nothing, state in ctx */
typedef void (*line_fn)(char *line, int first_line, void *ctx);

static void for_each_line(const char *path, line_fn fn, void *ctx) {
    size_t len = 0;
    unsigned char *data = read_file(path, &len);
    char *p = (char *)data;
    char *end = p + len;
    int first = 1;
    while (p < end) {
        char *nl = memchr(p, '\n', (size_t)(end - p));
        char *line_end = nl ? nl : end;
        char save = *line_end;
        *line_end = '\0';
        size_t ll = (size_t)(line_end - p);
        if (ll > 0 && p[ll - 1] == '\r') p[ll - 1] = '\0';
        fn(p, first, ctx);
        first = 0;
        *line_end = save;
        p = nl ? nl + 1 : end;
    }
    free(data);
}

typedef struct {
    registry_t reg;
    int in_prop_block;
    prop_t *cur_prop;
} node_parse_t;

static void node_line(char *line, int first_line, void *vctx) {
    node_parse_t *c = (node_parse_t *)vctx;

    if (first_line) {
        c->in_prop_block = 0;
        c->cur_prop = NULL;
    }

    char prop_name[512];
    if (match_prop_begin(line, prop_name, sizeof(prop_name))) {
        c->cur_prop = begin_prop(&c->reg, prop_name);
        c->in_prop_block = 1;
        return;
    }

    if (c->in_prop_block && line[0] == '}' && line[1] == ';') {
        c->in_prop_block = 0;
        return;
    }

    if (c->in_prop_block) {
        const char *t = line;
        while (is_ws(*t)) t++;
        if (*t == '{') c->cur_prop->count++;
        sb_puts(&c->cur_prop->body, line);
        sb_putc(&c->cur_prop->body, '\n');
        return;
    }

    if (strstr(line, "@NODE")) {
        char *s = after_last_tag(line, "@NODE", 1);
        cut_comment_end(s);

        const char *type = "", *name = "", *icon = "", *props = "";
        for (;;) {
            while (is_ws(*s)) s++;
            if (!*s) break;
            char *tok = s;
            while (*s && !is_ws(*s)) s++;
            if (*s) *s++ = '\0';

            char *eq = strchr(tok, '=');
            if (!eq) continue;
            *eq = '\0';
            const char *key = tok;
            char *val = eq + 1;
            size_t vl = strlen(val);
            if (val[0] == '"') { val++; vl--; }
            if (vl > 0 && val[vl - 1] == '"') val[vl - 1] = '\0';

            if (strcmp(key, "type") == 0) type = val;
            else if (strcmp(key, "name") == 0) name = val;
            else if (strcmp(key, "icon") == 0) icon = val;
            else if (strcmp(key, "properties") == 0) props = val;
        }

        c->reg.entries = (node_entry_t *)xrealloc(c->reg.entries,
                                                  sizeof(node_entry_t) * (size_t)(c->reg.entry_count + 1));
        node_entry_t *e = &c->reg.entries[c->reg.entry_count++];
        e->type = xstrdup(type);
        e->name = xstrdup(name);
        e->icon = xstrdup(icon);
        e->props = xstrdup(props);
    }
}

static int cmd_node_registry(int argc, char **argv) {
    if (argc < 5) die("usage: node-registry <registry.h> <types.h> <node.c>...");
    node_parse_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    for (int i = 4; i < argc; i++) for_each_line(argv[i], node_line, &ctx);
    registry_t *r = &ctx.reg;

    sb_t out = { 0, 0, 0 };
    sb_puts(&out,
            "/* ============================================================\n"
            " * node_registry_generated.h\n"
            " * ============================================================\n"
            " * Automatically generated during the build from each @NODE line\n"
            " * in src/nodes .c files and their declared property arrays.\n"
            " * ============================================================\n"
            " */\n"
            "\n"
            "#ifndef NODE_REGISTRY_GENERATED_H\n"
            "#define NODE_REGISTRY_GENERATED_H\n"
            "\n");

    for (int e = 0; e < r->entry_count; e++) {
        const char *pv = r->entries[e].props;
        if (strcmp(pv, "NULL") != 0) {
            prop_t *p = find_prop(r, pv);
            sb_printf(&out, "static const node_property_t %s_gen[] = {\n", pv);
            if (p && p->body.len) sb_append(&out, p->body.data, p->body.len);
            sb_puts(&out, "};\n\n");
        }
    }

    sb_puts(&out, "static const node_registry_entry_t g_node_registry_table[] = {\n");
    for (int e = 0; e < r->entry_count; e++) {
        node_entry_t *en = &r->entries[e];
        if (strcmp(en->props, "NULL") == 0) {
            sb_printf(&out, "    { %s, \"%s\", ICON_%s, NULL, 0 },\n", en->type, en->name, en->icon);
        } else {
            prop_t *p = find_prop(r, en->props);
            sb_printf(&out, "    { %s, \"%s\", ICON_%s, %s_gen, %d },\n", en->type, en->name,
                      en->icon, en->props, p ? p->count : 0);
        }
    }
    sb_puts(&out, "};\n");
    sb_printf(&out, "#define NODE_REGISTRY_GENERATED_COUNT %d\n", r->entry_count);
    sb_puts(&out, "\n");
    sb_puts(&out, "#endif /* NODE_REGISTRY_GENERATED_H */\n");

    /* node_types.h: unique node types in first-seen order */
    sb_t types = { 0, 0, 0 };
    sb_puts(&types,
            "/* ============================================================\n"
            " * node_types.h\n"
            " * ============================================================\n"
            " * Automatically generated during the build from each @NODE line\n"
            " * in src/nodes .c files. Do not edit manually.\n"
            " * A new node adds a new @NODE line and appears automatically.\n"
            " * ============================================================\n"
            " */\n"
            "\n"
            "#ifndef NODE_TYPES_H\n"
            "#define NODE_TYPES_H\n"
            "\n"
            "typedef enum {\n");

    int unique = 0;
    const char **seen = (const char **)xmalloc(sizeof(char *) * (size_t)(r->entry_count + 1));
    for (int e = 0; e < r->entry_count; e++) {
        int dup = 0;
        for (int k = 0; k < unique; k++) {
            if (strcmp(seen[k], r->entries[e].type) == 0) { dup = 1; break; }
        }
        if (!dup) seen[unique++] = r->entries[e].type;
    }
    for (int u = 0; u < unique; u++) {
        sb_printf(&types, "    %s%s\n", seen[u], (u < unique - 1) ? "," : "");
    }
    sb_puts(&types, "} node_type_t;\n\n#endif /* NODE_TYPES_H */\n");

    write_file(argv[2], out.data, out.len);
    write_file(argv[3], types.data, types.len);
    sb_free(&out);
    sb_free(&types);
    free(seen);
    return 0;
}

typedef struct {
    char *type, *draw_2d, *draw_3d;
} editor_entry_t;

typedef struct {
    editor_entry_t *entries;
    int count;
} editor_parse_t;

static void editor_line(char *line, int first_line, void *vctx) {
    (void)first_line;
    editor_parse_t *c = (editor_parse_t *)vctx;
    if (!strstr(line, "@NODE_EDITOR")) return;

    char *s = after_last_tag(line, "@NODE_EDITOR", 0);
    cut_comment_end(s);

    const char *type = "", *d2 = "NULL", *d3 = "NULL";
    for (;;) {
        while (is_ws(*s)) s++;
        if (!*s) break;
        char *tok = s;
        while (*s && !is_ws(*s)) s++;
        if (*s) *s++ = '\0';

        char *eq = strchr(tok, '=');
        if (!eq) continue;
        *eq = '\0';
        const char *key = tok;
        const char *val = eq + 1;
        if (strcmp(key, "type") == 0) type = val;
        else if (strcmp(key, "draw_2d") == 0) d2 = val;
        else if (strcmp(key, "draw_3d") == 0) d3 = val;
    }
    if (type[0] == '\0') return;

    c->entries = (editor_entry_t *)xrealloc(c->entries, sizeof(editor_entry_t) * (size_t)(c->count + 1));
    editor_entry_t *e = &c->entries[c->count++];
    e->type = xstrdup(type);
    e->draw_2d = xstrdup(d2);
    e->draw_3d = xstrdup(d3);
}

static int cmd_node_editor_registry(int argc, char **argv) {
    if (argc < 4) die("usage: node-editor-registry <out.h> <node_editor.c>...");
    editor_parse_t ctx = { NULL, 0 };
    for (int i = 3; i < argc; i++) for_each_line(argv[i], editor_line, &ctx);

    static const char *proto =
        "(const node_property_value_t *values, int property_count, int cam_x, int cam_y, int cam_w, int cam_h, int selected);\n";

    sb_t out = { 0, 0, 0 };
    sb_puts(&out,
            "/* ============================================================\n"
            " * node_editor_registry_generated.h\n"
            " * ============================================================\n"
            " * Automatically generated during the build from each @NODE_EDITOR\n"
            " * line in src/nodes_editor .c files.\n"
            " * ============================================================\n"
            " */\n"
            "\n"
            "#ifndef NODE_EDITOR_REGISTRY_GENERATED_H\n"
            "#define NODE_EDITOR_REGISTRY_GENERATED_H\n"
            "\n");

    for (int e = 0; e < ctx.count; e++) {
        if (strcmp(ctx.entries[e].draw_2d, "NULL") != 0) sb_printf(&out, "void %s%s", ctx.entries[e].draw_2d, proto);
        if (strcmp(ctx.entries[e].draw_3d, "NULL") != 0) sb_printf(&out, "void %s%s", ctx.entries[e].draw_3d, proto);
    }
    sb_puts(&out, "\n");
    sb_puts(&out, "static const node_editor_registry_entry_t g_node_editor_registry_table[] = {\n");
    for (int e = 0; e < ctx.count; e++) {
        sb_printf(&out, "    { %s, %s, %s },\n", ctx.entries[e].type, ctx.entries[e].draw_2d, ctx.entries[e].draw_3d);
    }
    sb_puts(&out, "};\n");
    sb_printf(&out, "#define NODE_EDITOR_REGISTRY_GENERATED_COUNT %d\n", ctx.count);
    sb_puts(&out, "\n#endif /* NODE_EDITOR_REGISTRY_GENERATED_H */\n");

    write_file(argv[2], out.data, out.len);
    sb_free(&out);
    return 0;
}

/* ============================================================
 * tar writer (ustar) + LZMA2-compressed .xz writer
 * ============================================================ */

static void tar_put_octal(unsigned char *dst, size_t width, uint64_t v) {
    memset(dst, '0', width - 1);
    dst[width - 1] = '\0';
    size_t i = width - 1;
    while (i-- > 0 && v) {
        dst[i] = (unsigned char)('0' + (v & 7));
        v >>= 3;
    }
}

static void tar_add_header(sb_t *tar, const char *name, uint64_t size, char type, unsigned mode) {
    unsigned char h[512];
    memset(h, 0, sizeof(h));

    size_t nl = strlen(name);
    const char *name_part = name;
    size_t name_len = nl;
    const char *prefix = NULL;
    size_t prefix_len = 0;

    if (nl > 100) {
        int found = 0;
        for (size_t i = 1; i < nl; i++) {
            if (name[i] == '/' && i <= 155 && nl - i - 1 <= 100 && nl - i - 1 > 0) {
                prefix = name;
                prefix_len = i;
                name_part = name + i + 1;
                name_len = nl - i - 1;
                found = 1;
                break;
            }
        }
        if (!found) die("path too long for tar: %s", name);
    }

    memcpy(h, name_part, name_len);
    if (prefix) memcpy(h + 345, prefix, prefix_len);

    tar_put_octal(h + 100, 8, mode);
    tar_put_octal(h + 108, 8, 0);
    tar_put_octal(h + 116, 8, 0);
    tar_put_octal(h + 124, 12, size);
    tar_put_octal(h + 136, 12, 0);
    memset(h + 148, ' ', 8);
    h[156] = (unsigned char)type;
    memcpy(h + 257, "ustar", 6);
    h[263] = '0';
    h[264] = '0';
    tar_put_octal(h + 329, 8, 0);
    tar_put_octal(h + 337, 8, 0);

    unsigned sum = 0;
    for (int i = 0; i < 512; i++) sum += h[i];
    snprintf((char *)h + 148, 7, "%06o", sum);
    h[155] = ' ';

    sb_append(tar, h, 512);
}

static void tar_add_file(sb_t *tar, const char *name, const unsigned char *data, size_t size) {
    tar_add_header(tar, name, (uint64_t)size, '0', 0644);
    sb_append(tar, data, size);
    size_t pad = (512 - (size % 512)) % 512;
    for (size_t i = 0; i < pad; i++) sb_putc(tar, 0);
}

typedef struct {
    char *path; /* real path as found on disk */
    char *rel;  /* name inside the archive, '/' separators */
} tar_item_t;

typedef struct {
    tar_item_t *items;
    int count;
} tar_list_t;

static void tar_collect_cb(const char *path, void *user) {
    tar_list_t *l = (tar_list_t *)user;
    l->items = (tar_item_t *)xrealloc(l->items, sizeof(tar_item_t) * (size_t)(l->count + 1));
    l->items[l->count].path = xstrdup(path);
    l->items[l->count].rel = NULL;
    l->count++;
}

static int tar_item_cmp(const void *a, const void *b) {
    return strcmp(((const tar_item_t *)a)->rel, ((const tar_item_t *)b)->rel);
}

static void normalize_slashes(char *s) {
    for (; *s; s++) if (*s == '\\') *s = '/';
}

static void build_tar(sb_t *tar, const char *base, const char *entry) {
    char *base_clean = xstrdup(base[0] ? base : ".");
    size_t bl = strlen(base_clean);
    while (bl > 1 && is_sep(base_clean[bl - 1])) base_clean[--bl] = '\0';

    char *entry_norm = xstrdup(entry);
    normalize_slashes(entry_norm);
    size_t el = strlen(entry_norm);
    while (el > 0 && entry_norm[el - 1] == '/') entry_norm[--el] = '\0';
    if (el == 0) die("pack: empty entry name");

    size_t root_size = strlen(base_clean) + strlen(entry_norm) + 2;
    char *root = (char *)xmalloc(root_size);
    snprintf(root, root_size, "%s/%s", base_clean, entry_norm);

    if (!rebax_fs_exists(root)) die("pack: '%s' does not exist", root);

    if (!rebax_fs_is_dir(root)) {
        size_t len = 0;
        unsigned char *data = read_file(root, &len);
        tar_add_file(tar, entry_norm, data, len);
        free(data);
    } else {
        size_t dn = strlen(entry_norm) + 2;
        char *dirname = (char *)xmalloc(dn);
        snprintf(dirname, dn, "%s/", entry_norm);
        tar_add_header(tar, dirname, 0, '5', 0755);
        free(dirname);

        tar_list_t list = { NULL, 0 };
        rebax_fs_walk_files(root, tar_collect_cb, &list);

        size_t root_len = strlen(root);
        for (int i = 0; i < list.count; i++) {
            const char *tail = list.items[i].path + root_len;
            while (is_sep(*tail)) tail++;
            size_t rn = strlen(entry_norm) + strlen(tail) + 2;
            char *rel = (char *)xmalloc(rn);
            snprintf(rel, rn, "%s/%s", entry_norm, tail);
            normalize_slashes(rel);
            list.items[i].rel = rel;
        }
        if (list.count > 1) qsort(list.items, (size_t)list.count, sizeof(tar_item_t), tar_item_cmp);

        for (int i = 0; i < list.count; i++) {
            size_t len = 0;
            unsigned char *data = read_file(list.items[i].path, &len);
            tar_add_file(tar, list.items[i].rel, data, len);
            free(data);
            free(list.items[i].path);
            free(list.items[i].rel);
        }
        free(list.items);
    }

    for (int i = 0; i < 1024; i++) sb_putc(tar, 0); /* end-of-archive marker */
    free(root);
    free(entry_norm);
    free(base_clean);
}

static void put_vli(sb_t *b, uint64_t v) {
    while (v >= 0x80) {
        sb_putc(b, (unsigned char)(v | 0x80));
        v >>= 7;
    }
    sb_putc(b, (unsigned char)v);
}

/*
 * Uses the 7-Zip LZMA SDK's portable, single-threaded encoder.  These values
 * deliberately mirror a strong xz-style configuration without turning them
 * into user-facing options: level 9, optimal parser, 64 MiB maximum dictionary
 * and a 273-byte fast-byte window.  For small archives the SDK automatically
 * reduces the dictionary, so the decoder never reserves more memory than the
 * archive needs.
 */
static void lzma2_compress_high(const unsigned char *data, size_t n,
                                unsigned char **payload_out, size_t *payload_size_out,
                                unsigned char *dictionary_property_out) {
    CLzma2EncProps props;
    CLzma2EncHandle encoder;
    unsigned char *payload;
    size_t capacity;
    size_t payload_size;
    SRes result;

    /* LZMA2 can always fall back to copy chunks; this also covers that case. */
    if (n > SIZE_MAX - n / 64 - 65536) die("archive is too large to compress");
    capacity = n + n / 64 + 65536;
    payload = (unsigned char *)xmalloc(capacity ? capacity : 1);

    Lzma2EncProps_Init(&props);
    props.lzmaProps.level = 9;
    props.lzmaProps.dictSize = 64u * 1024u * 1024u;
    props.lzmaProps.reduceSize = (UInt64)n;
    props.lzmaProps.algo = 1;
    props.lzmaProps.fb = 273;
    props.lzmaProps.btMode = 1;
    props.lzmaProps.numHashBytes = 4;
    props.lzmaProps.mc = 1000;
    props.lzmaProps.numThreads = 1;
    props.numTotalThreads = 1;
    props.blockSize = LZMA2_ENC_PROPS_BLOCK_SIZE_SOLID;

    encoder = Lzma2Enc_Create(&g_Alloc, &g_BigAlloc);
    if (!encoder) die("cannot allocate the LZMA2 encoder");
    result = Lzma2Enc_SetProps(encoder, &props);
    if (result != SZ_OK) {
        Lzma2Enc_Destroy(encoder);
        free(payload);
        die("cannot configure the LZMA2 encoder (%d)", result);
    }

    *dictionary_property_out = Lzma2Enc_WriteProperties(encoder);
    payload_size = capacity;
    result = Lzma2Enc_Encode2(encoder, NULL, payload, &payload_size,
                               NULL, data, n, NULL);
    Lzma2Enc_Destroy(encoder);
    if (result != SZ_OK) {
        free(payload);
        die("LZMA2 compression failed (%d)", result);
    }

    *payload_out = payload;
    *payload_size_out = payload_size;
}

/* valid .xz container with one LZMA2-compressed block and a CRC32 check */
static void xz_compress(const unsigned char *data, size_t n, sb_t *out) {
    static const unsigned char magic[6] = { 0xFD, '7', 'z', 'X', 'Z', 0x00 };
    static const unsigned char flags[2] = { 0x00, 0x01 }; /* check type 1 = CRC32 */
    unsigned char *payload = NULL;
    size_t payload_size = 0;
    unsigned char dictionary_property = 0;

    lzma2_compress_high(data, n, &payload, &payload_size, &dictionary_property);

    sb_append(out, magic, 6);
    sb_append(out, flags, 2);
    put_le32(out, crc32_update(0, flags, 2));

    /* block header: size byte, flags, filter LZMA2, props size, dict byte, padding, CRC32 */
    unsigned char bh[12] = { 0x02, 0x00, 0x21, 0x01, 0, 0, 0, 0, 0, 0, 0, 0 };
    bh[4] = dictionary_property;
    uint32_t hc = crc32_update(0, bh, 8);
    bh[8] = (unsigned char)hc;
    bh[9] = (unsigned char)(hc >> 8);
    bh[10] = (unsigned char)(hc >> 16);
    bh[11] = (unsigned char)(hc >> 24);
    sb_append(out, bh, 12);

    size_t comp = payload_size;
    sb_append(out, payload, payload_size);
    free(payload);

    uint64_t unpadded = 12 + (uint64_t)comp + 4;
    while (comp & 3) { sb_putc(out, 0); comp++; }
    put_le32(out, crc32_update(0, data, n));

    sb_t idx = { 0, 0, 0 };
    sb_putc(&idx, 0x00);
    put_vli(&idx, 1);
    put_vli(&idx, unpadded);
    put_vli(&idx, (uint64_t)n);
    while (idx.len & 3) sb_putc(&idx, 0);
    put_le32(&idx, crc32_update(0, idx.data, idx.len));
    sb_append(out, idx.data, idx.len);

    unsigned char foot[6];
    uint32_t backward = (uint32_t)(idx.len / 4 - 1);
    foot[0] = (unsigned char)backward;
    foot[1] = (unsigned char)(backward >> 8);
    foot[2] = (unsigned char)(backward >> 16);
    foot[3] = (unsigned char)(backward >> 24);
    foot[4] = flags[0];
    foot[5] = flags[1];
    put_le32(out, crc32_update(0, foot, 6));
    sb_append(out, foot, 6);
    sb_putc(out, 'Y');
    sb_putc(out, 'Z');
    sb_free(&idx);
}

static int cmd_pack(int argc, char **argv) {
    if (argc != 5) die("usage: pack <out.tar.xz> <base_dir> <entry>");
    sb_t tar = { 0, 0, 0 };
    build_tar(&tar, argv[3], argv[4]);
    sb_t xz = { 0, 0, 0 };
    xz_compress(tar.data, tar.len, &xz);
    write_file(argv[2], xz.data, xz.len);
    sb_free(&tar);
    sb_free(&xz);
    return 0;
}

/* ============================================================
 * Simple file-system commands
 * ============================================================ */

static void count_cb(const char *path, void *user) {
    (void)path;
    (*(int *)user)++;
}

static int cmd_mkdir(int argc, char **argv) {
    if (argc < 3) die("usage: mkdir <dir>...");
    for (int i = 2; i < argc; i++) {
        if (!rebax_fs_mkdir_p(argv[i])) die("cannot create directory '%s'", argv[i]);
    }
    return 0;
}

static int cmd_rm(int argc, char **argv) {
    if (argc < 3) die("usage: rm <path>...");
    for (int i = 2; i < argc; i++) {
        if (!rebax_fs_remove_recursive(argv[i])) die("cannot remove '%s'", argv[i]);
    }
    return 0;
}

static int cmd_cp(int argc, char **argv) {
    if (argc != 4) die("usage: cp <src> <dst>");
    ensure_parent_dir(argv[3]);
    if (!rebax_fs_copy_file(argv[2], argv[3])) die("cannot copy '%s' to '%s'", argv[2], argv[3]);
    return 0;
}

static int cmd_exists(int argc, char **argv) {
    if (argc < 3) die("usage: exists <path>...");
    for (int i = 2; i < argc; i++) {
        if (!rebax_fs_exists(argv[i])) { puts("0"); return 0; }
    }
    puts("1");
    return 0;
}

static int cmd_dir_empty(int argc, char **argv) {
    if (argc != 3) die("usage: dir-empty <dir>");
    int files = 0;
    if (rebax_fs_is_dir(argv[2])) rebax_fs_walk_files(argv[2], count_cb, &files);
    puts(files == 0 ? "1" : "0");
    return 0;
}

/* usable program file? (POSIX: executable regular file; Windows: any file) */
static int is_program(const char *path) {
    if (!rebax_fs_exists(path) || rebax_fs_is_dir(path)) return 0;
#ifdef _WIN32
    return 1;
#else
    return access(path, X_OK) == 0;
#endif
}

static int cmd_which(int argc, char **argv) {
    if (argc != 3) die("usage: which <program>");
    const char *name = argv[2];
    int has_dir = 0;
    for (const char *c = name; *c; c++) if (is_sep(*c)) has_dir = 1;

#ifdef _WIN32
    const char path_sep = ';';
    static const char *const exts[] = { "", ".exe", ".cmd", ".bat", NULL };
#else
    const char path_sep = ':';
    static const char *const exts[] = { "", NULL };
#endif

    const char *env = getenv("PATH");
    if (has_dir || !env) env = "";

    /* has_dir: check the name itself once; otherwise every PATH entry */
    const char *p = env;
    int first = 1;
    while (first || *p) {
        char dir[1600];
        size_t dl = 0;
        if (has_dir) {
            dir[0] = '\0';
        } else {
            while (*p && *p != path_sep && dl < sizeof(dir) - 1) dir[dl++] = *p++;
            dir[dl] = '\0';
            if (*p == path_sep) p++;
            if (dl >= 2 && dir[0] == '"' && dir[dl - 1] == '"') {
                memmove(dir, dir + 1, dl - 2);
                dir[dl - 2] = '\0';
                dl -= 2;
            }
            if (dl == 0) { first = 0; continue; }
            while (dl > 1 && is_sep(dir[dl - 1])) dir[--dl] = '\0';
        }
        for (int e = 0; exts[e]; e++) {
            char full[1700];
            if (has_dir) snprintf(full, sizeof(full), "%s%s", name, exts[e]);
            else snprintf(full, sizeof(full), "%s/%s%s", dir, name, exts[e]);
            if (is_program(full)) { puts(full); return 0; }
        }
        first = 0;
        if (has_dir) break;
    }
    return 1;
}

static int cmd_extract(int argc, char **argv) {
    if (argc != 4) die("usage: extract <archive.tar.xz> <dest_dir>");
    if (!rebax_fs_extract_tar_xz(argv[2], argv[3])) die("cannot extract '%s'", argv[2]);
    return 0;
}

static void usage(void) {
    fputs("usage: rebax-tool <command> [args]\n"
          "  mkdir <dir>...                          create directories (mkdir -p)\n"
          "  rm <path>...                            remove files/directories (rm -rf)\n"
          "  cp <src> <dst>                          copy a file\n"
          "  exists <path>...                        print 1 if every path exists, else 0\n"
          "  dir-empty <dir>                         print 1 if missing or without files, else 0\n"
          "  which <program>                         print the full path found in PATH\n"
          "  extract <archive.tar.xz> <dest>         extract a .tar.xz archive\n"
          "  pack <out.tar.xz> <base> <entry>        create .tar.xz of <base>/<entry>\n"
          "  icon-names <out.h> <icon.png>...        generate icon_names.h\n"
          "  icon-atlas <out.png> <icon.png>...      build the icon atlas PNG\n"
          "  node-registry <registry.h> <types.h> <node.c>...\n"
          "  node-editor-registry <out.h> <node_editor.c>...\n",
          stderr);
}

int main(int argc, char **argv) {
    if (argc < 2) { usage(); return 2; }
    const char *cmd = argv[1];

    if (strcmp(cmd, "mkdir") == 0) return cmd_mkdir(argc, argv);
    if (strcmp(cmd, "rm") == 0) return cmd_rm(argc, argv);
    if (strcmp(cmd, "cp") == 0) return cmd_cp(argc, argv);
    if (strcmp(cmd, "exists") == 0) return cmd_exists(argc, argv);
    if (strcmp(cmd, "dir-empty") == 0) return cmd_dir_empty(argc, argv);
    if (strcmp(cmd, "which") == 0) return cmd_which(argc, argv);
    if (strcmp(cmd, "extract") == 0) return cmd_extract(argc, argv);
    if (strcmp(cmd, "pack") == 0) return cmd_pack(argc, argv);
    if (strcmp(cmd, "icon-names") == 0) return cmd_icon_names(argc, argv);
    if (strcmp(cmd, "icon-atlas") == 0) return cmd_icon_atlas(argc, argv);
    if (strcmp(cmd, "node-registry") == 0) return cmd_node_registry(argc, argv);
    if (strcmp(cmd, "node-editor-registry") == 0) return cmd_node_editor_registry(argc, argv);

    usage();
    return 2;
}
