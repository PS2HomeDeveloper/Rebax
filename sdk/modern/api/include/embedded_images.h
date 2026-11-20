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

#ifndef EMBEDDED_IMAGES_H
#define EMBEDDED_IMAGES_H

typedef struct {
    const char *path;
    int width;
    int height;
    const unsigned char *data;
} embedded_image_t;

const embedded_image_t *embedded_image_find(const char *path);

#endif
