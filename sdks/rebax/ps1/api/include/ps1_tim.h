/*
 *  ____  _____ ____    _    __  __
 * |  _ \| ____| __ )  / \   \ \/ /
 * | |_) |  _| |  _ \ / _ \   \  /
 * |  _ <| |___| |_) / ___ \  /  \
 * |_| \_\_____|____/_/   \_\/ /_\
 *
 * Rebax Engine
 * Copyright (c) 2026 PS2HomeDeveloper
 *
 * Licensed under the MIT License.
 * See the LICENSE file for details.
 */
#ifndef REBAX_PS1_TIM_H
#define REBAX_PS1_TIM_H

#include <stddef.h>
#include <stdint.h>
#include <psxgpu.h>

typedef struct {
    RECT image;
    RECT clut;
    int mode;
    int has_clut;
} ps1_tim_image_t;

int ps1_tim_upload(const void *data, size_t size, ps1_tim_image_t *out_image);
int ps1_tim_mode_supported(int mode);
int ps1_tim_texture_width(int mode, int image_words);

#endif
