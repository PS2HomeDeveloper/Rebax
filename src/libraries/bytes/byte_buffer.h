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

#ifndef BYTE_BUFFER_H
#define BYTE_BUFFER_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    unsigned char *data;
    size_t size;
    size_t cap;
    int failed;
} byte_buf_t;

void byte_buf_init(byte_buf_t *b);
void byte_buf_free(byte_buf_t *b);
void byte_buf_clear(byte_buf_t *b);
int byte_buf_put(byte_buf_t *b, const void *src, size_t n);
int byte_buf_zero(byte_buf_t *b, size_t n);
int byte_buf_u8(byte_buf_t *b, uint32_t v);
int byte_buf_u16(byte_buf_t *b, uint32_t v);
int byte_buf_u32(byte_buf_t *b, uint32_t v);
int byte_buf_u64(byte_buf_t *b, uint64_t v);
void byte_buf_set_u32(byte_buf_t *b, size_t pos, uint32_t v);

#endif
