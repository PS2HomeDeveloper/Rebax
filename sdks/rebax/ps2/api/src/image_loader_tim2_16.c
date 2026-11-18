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
#include "image_loader.h"
#include "image_loader_tim2_internal.h"
int image_loader_load_tim2_16(GSGLOBAL *gsGlobal, GSTEXTURE *texture, const char *path) {
    return image_loader_load_tim2_format(gsGlobal, texture, path, 0x02);
}
