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
#ifndef REBAX_TIM_CODEC_H
#define REBAX_TIM_CODEC_H

#include <stddef.h>
#include <stdint.h>

typedef enum {
    REBAX_IMAGE_PLATFORM_PS1 = 0,
    REBAX_IMAGE_PLATFORM_PS2 = 1,
    REBAX_IMAGE_PLATFORM_MODERN = 2
} rebax_image_platform_t;

typedef enum {
    REBAX_TIM1_4BIT = 0,
    REBAX_TIM1_8BIT = 1,
    REBAX_TIM1_16BIT = 2,
    REBAX_TIM1_24BIT = 3
} rebax_tim1_format_t;

typedef enum {
    REBAX_TIM2_32BIT = 0,
    REBAX_TIM2_24BIT = 1,
    REBAX_TIM2_16BIT = 2,
    REBAX_TIM2_16S = 3,
    REBAX_TIM2_8BIT = 4,
    REBAX_TIM2_4BIT = 5,
    REBAX_TIM2_8H = 6,
    REBAX_TIM2_4HL = 7,
    REBAX_TIM2_4HH = 8
} rebax_tim2_format_t;

typedef struct {
    int width;
    int height;
    unsigned char *rgba;
} rebax_decoded_image_t;

typedef struct {
    int dither;
    int premultiply_alpha;
    int resize_mode;
    int swizzle;
    int mipmaps;
    int csm2;
    int clut_format;
    int vram_x;
    int vram_y;
    int clut_x;
    int clut_y;
    int palette_count;
} rebax_tim_encode_options_t;

void rebax_tim_encode_options_default(rebax_tim_encode_options_t *options);
int rebax_tim_decode_file(const char *path, rebax_decoded_image_t *out);
int rebax_tim_decode_file_level(const char *path, int level, rebax_decoded_image_t *out);
void rebax_tim_decoded_free(rebax_decoded_image_t *image);
int rebax_tim_encode_file(const char *path, const unsigned char *rgba, int width, int height,
                          rebax_tim1_format_t format, const rebax_tim_encode_options_t *options);
int rebax_tim2_encode_file(const char *path, const unsigned char *rgba, int width, int height,
                           rebax_tim2_format_t format, const rebax_tim_encode_options_t *options);
int rebax_tim_write_ps1_16(const char *path, const unsigned char *rgba, int width, int height);
int rebax_image_format_supported(rebax_image_platform_t platform, const char *extension);
int rebax_image_format_rank(rebax_image_platform_t platform, const char *extension);
const char *rebax_image_best_format(rebax_image_platform_t platform);
int rebax_tim2_format_from_file(const char *path);
int rebax_tim1_format_from_file(const char *path);

#endif
