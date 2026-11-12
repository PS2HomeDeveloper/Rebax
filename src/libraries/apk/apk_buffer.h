#ifndef APK_BUFFER_H
#define APK_BUFFER_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    unsigned char *data;
    size_t size;
    size_t cap;
    int failed;
} apk_buf_t;

void apk_buf_init(apk_buf_t *b);
void apk_buf_free(apk_buf_t *b);
void apk_buf_clear(apk_buf_t *b);
int apk_buf_put(apk_buf_t *b, const void *src, size_t n);
int apk_buf_zero(apk_buf_t *b, size_t n);
int apk_buf_u8(apk_buf_t *b, uint32_t v);
int apk_buf_u16(apk_buf_t *b, uint32_t v);
int apk_buf_u32(apk_buf_t *b, uint32_t v);
int apk_buf_u64(apk_buf_t *b, uint64_t v);
void apk_buf_set_u32(apk_buf_t *b, size_t pos, uint32_t v);

#endif
