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
 */
#ifndef REBAX_TIM_INTERNAL_H
#define REBAX_TIM_INTERNAL_H
#include "formats/tim/tim_codec.h"
#include <stdio.h>

#define TIM2_PSM_CT32 0x00
#define TIM2_PSM_CT24 0x01
#define TIM2_PSM_CT16 0x02
#define TIM2_PSM_CT16S 0x0a
#define TIM2_PSM_T8 0x13
#define TIM2_PSM_T4 0x14
#define TIM2_PSM_T8H 0x1b
#define TIM2_PSM_T4HL 0x1a
#define TIM2_PSM_T4HH 0x2c

typedef struct {
    unsigned char *p;
    size_t n;
    size_t cap;
} tim_buffer_t;
unsigned tim_rd16(const unsigned char *p);
unsigned long tim_rd32(const unsigned char *p);
unsigned long long tim_rd64(const unsigned char *p);
void tim_wr16(unsigned char *p, unsigned v);
void tim_wr32(unsigned char *p, unsigned long v);
void tim_wr64(unsigned char *p, unsigned long long v);
void *tim_alloc(size_t n);
unsigned char *tim_rgba_alloc(int w, int h);
void tim_buffer_put(tim_buffer_t *b, const void *p, size_t n);
int tim_write_file(const char *path, const unsigned char *p, size_t n);
int tim_read_file(const char *path, unsigned char **p, size_t *n);
void tim_unpack_ps1_5551(unsigned v, unsigned char *out);
void tim_unpack_ps2_5551(unsigned v, unsigned char *out);
unsigned tim_pack_ps1_5551(const unsigned char *p);
unsigned tim_pack_ps2_5551(const unsigned char *p);
void tim_swizzle32(const unsigned char *src, unsigned char *dst, size_t n, int w, int h);
void tim_swizzle16(const unsigned char *src, unsigned char *dst, size_t n, int w, int h);
void tim_unswizzle32(const unsigned char *src, unsigned char *dst, size_t n, int w, int h);
void tim_unswizzle16(const unsigned char *src, unsigned char *dst, size_t n, int w, int h);
void tim_swizzle_clut8(const unsigned char *src, unsigned char *dst);
void tim_unswizzle_clut8(const unsigned char *src, unsigned char *dst);
int tim_pow2_resize(const unsigned char *src, int w, int h, int mode, unsigned char **out, int *ow, int *oh);
int tim_quantize(const unsigned char *rgba, int w, int h, int colors, int dither, unsigned char **indices,
                 unsigned char **palette);
void tim_downsample(const unsigned char *src, int w, int h, unsigned char **out, int *ow, int *oh);
int tim2_decode_mem(const unsigned char *data, size_t size, int level, rebax_decoded_image_t *out);
int tim1_decode_mem(const unsigned char *data, size_t size, rebax_decoded_image_t *out);
int tim1_encode_mem(const unsigned char *rgba, int w, int h, rebax_tim1_format_t fmt,
                    const rebax_tim_encode_options_t *opt, tim_buffer_t *out);
int tim2_encode_mem(const unsigned char *rgba, int w, int h, rebax_tim2_format_t fmt,
                    const rebax_tim_encode_options_t *opt, tim_buffer_t *out);
#endif
