#include "libraries/apk/apk_buffer.h"

#include <stdlib.h>
#include <string.h>

void apk_buf_init(apk_buf_t *b) {
    b->data = NULL;
    b->size = 0;
    b->cap = 0;
    b->failed = 0;
}

void apk_buf_free(apk_buf_t *b) {
    free(b->data);
    apk_buf_init(b);
}

void apk_buf_clear(apk_buf_t *b) {
    b->size = 0;
    b->failed = 0;
}

static int reserve(apk_buf_t *b, size_t extra) {
    if (b->failed) return 0;
    if (b->size + extra <= b->cap) return 1;
    size_t cap = b->cap ? b->cap : 256;
    while (cap < b->size + extra) cap *= 2;
    unsigned char *p = (unsigned char *)realloc(b->data, cap);
    if (!p) {
        b->failed = 1;
        return 0;
    }
    b->data = p;
    b->cap = cap;
    return 1;
}

int apk_buf_put(apk_buf_t *b, const void *src, size_t n) {
    if (n == 0) return 1;
    if (!reserve(b, n)) return 0;
    memcpy(b->data + b->size, src, n);
    b->size += n;
    return 1;
}

int apk_buf_zero(apk_buf_t *b, size_t n) {
    if (n == 0) return 1;
    if (!reserve(b, n)) return 0;
    memset(b->data + b->size, 0, n);
    b->size += n;
    return 1;
}

int apk_buf_u8(apk_buf_t *b, uint32_t v) {
    unsigned char c = (unsigned char)(v & 0xff);
    return apk_buf_put(b, &c, 1);
}

int apk_buf_u16(apk_buf_t *b, uint32_t v) {
    unsigned char c[2] = {(unsigned char)(v & 0xff), (unsigned char)((v >> 8) & 0xff)};
    return apk_buf_put(b, c, 2);
}

int apk_buf_u32(apk_buf_t *b, uint32_t v) {
    unsigned char c[4] = {(unsigned char)(v & 0xff), (unsigned char)((v >> 8) & 0xff), (unsigned char)((v >> 16) & 0xff), (unsigned char)((v >> 24) & 0xff)};
    return apk_buf_put(b, c, 4);
}

int apk_buf_u64(apk_buf_t *b, uint64_t v) {
    return apk_buf_u32(b, (uint32_t)(v & 0xffffffffu)) && apk_buf_u32(b, (uint32_t)(v >> 32));
}

void apk_buf_set_u32(apk_buf_t *b, size_t pos, uint32_t v) {
    if (b->failed || pos + 4 > b->size) return;
    b->data[pos] = (unsigned char)(v & 0xff);
    b->data[pos + 1] = (unsigned char)((v >> 8) & 0xff);
    b->data[pos + 2] = (unsigned char)((v >> 16) & 0xff);
    b->data[pos + 3] = (unsigned char)((v >> 24) & 0xff);
}
