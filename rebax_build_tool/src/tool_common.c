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

#include "tool_common.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "filesystem/rebax_fs.h"

void die(const char *fmt, ...) {
    va_list ap;
    fprintf(stderr, "[rebax-tool] error: ");
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
    exit(1);
}

void *xmalloc(size_t n) {
    void *p = malloc(n ? n : 1);
    if (!p) die("out of memory");
    return p;
}

void *xrealloc(void *p, size_t n) {
    void *q = realloc(p, n ? n : 1);
    if (!q) die("out of memory");
    return q;
}

char *xstrdup(const char *s) {
    size_t n = strlen(s);
    char *p = (char *)xmalloc(n + 1);
    memcpy(p, s, n + 1);
    return p;
}

void sb_reserve(sb_t *b, size_t extra) {
    if (b->len + extra + 1 > b->cap) {
        size_t nc = b->cap ? b->cap : 256;
        while (nc < b->len + extra + 1) nc *= 2;
        b->data = (unsigned char *)xrealloc(b->data, nc);
        b->cap = nc;
    }
}

void sb_append(sb_t *b, const void *src, size_t n) {
    sb_reserve(b, n);
    if (n) memcpy(b->data + b->len, src, n);
    b->len += n;
    b->data[b->len] = 0;
}

void sb_puts(sb_t *b, const char *s) { sb_append(b, s, strlen(s)); }

void sb_putc(sb_t *b, unsigned char c) { sb_append(b, &c, 1); }

void sb_printf(sb_t *b, const char *fmt, ...) {
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

void sb_free(sb_t *b) {
    free(b->data);
    b->data = NULL;
    b->len = b->cap = 0;
}

void put_le32(sb_t *b, uint32_t v) {
    unsigned char t[4] = { (unsigned char)v, (unsigned char)(v >> 8),
                           (unsigned char)(v >> 16), (unsigned char)(v >> 24) };
    sb_append(b, t, 4);
}

void put_be32(sb_t *b, uint32_t v) {
    unsigned char t[4] = { (unsigned char)(v >> 24), (unsigned char)(v >> 16),
                           (unsigned char)(v >> 8), (unsigned char)v };
    sb_append(b, t, 4);
}

/* reads a whole file; result is NUL-terminated (extra byte not counted) */
unsigned char *read_file(const char *path, size_t *out_len) {
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

int is_sep(char c) { return c == '/' || c == '\\'; }

void ensure_parent_dir(const char *path) {
    char *tmp = xstrdup(path);
    size_t i = strlen(tmp);
    while (i > 0 && !is_sep(tmp[i - 1])) i--;
    if (i > 1) {
        tmp[i - 1] = '\0';
        if (!rebax_fs_mkdir_p(tmp)) die("cannot create directory '%s'", tmp);
    }
    free(tmp);
}

void write_file(const char *path, const void *data, size_t len) {
    ensure_parent_dir(path);
    FILE *f = fopen(path, "wb");
    if (!f) die("cannot write '%s'", path);
    if (len && fwrite(data, 1, len, f) != len) { fclose(f); die("write error on '%s'", path); }
    if (fclose(f) != 0) die("write error on '%s'", path);
}

int write_text_file(const char *path, const char *text) {
    FILE *f = fopen(path, "wb");
    if (!f) return 0;
    size_t n = strlen(text);
    int ok = fwrite(text, 1, n, f) == n;
    if (fclose(f) != 0) ok = 0;
    return ok;
}
