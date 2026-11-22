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

#include <gsKit.h>
#include <gsToolkit.h>

#include "image_decoder.h"

int image_loader_load_raw(GSGLOBAL *gsGlobal, GSTEXTURE *texture, const char *path,
                           int width, int height, int psm) {
    texture->Width  = width;
    texture->Height = height;
    texture->PSM    = psm;
    return gsKit_texture_raw(gsGlobal, texture, (char *)path) == 0;
}
