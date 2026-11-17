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

typedef struct {
    int width;
    int height;
    unsigned char *rgba;
} rebax_decoded_image_t;

int rebax_tim_decode_file(const char *path, rebax_decoded_image_t *out);
void rebax_tim_decoded_free(rebax_decoded_image_t *image);
int rebax_tim_write_ps1_16(const char *path, const unsigned char *rgba, int width, int height);
int rebax_image_format_supported(rebax_image_platform_t platform, const char *extension);
int rebax_image_format_rank(rebax_image_platform_t platform, const char *extension);
const char *rebax_image_best_format(rebax_image_platform_t platform);

#endif
