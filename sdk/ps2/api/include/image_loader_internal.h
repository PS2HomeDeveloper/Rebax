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

#ifndef IMAGE_LOADER_INTERNAL_H
#define IMAGE_LOADER_INTERNAL_H

#include <gsKit.h>

unsigned int read_u32le(const unsigned char *p);
unsigned short read_u16le(const unsigned char *p);
unsigned long long read_u64le(const unsigned char *p);

int finish_texture_upload(GSGLOBAL *gsGlobal, GSTEXTURE *texture);

#endif
