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

#ifndef IMAGE_LOADER_H
#define IMAGE_LOADER_H

#include <gsKit.h>

int image_loader_load_png(GSGLOBAL *gsGlobal, GSTEXTURE *texture, const char *path);
int image_loader_load_jpeg(GSGLOBAL *gsGlobal, GSTEXTURE *texture, const char *path);
int image_loader_load_bmp(GSGLOBAL *gsGlobal, GSTEXTURE *texture, const char *path);
int image_loader_load_tga(GSGLOBAL *gsGlobal, GSTEXTURE *texture, const char *path);
int image_loader_load_tiff(GSGLOBAL *gsGlobal, GSTEXTURE *texture, const char *path);

int image_loader_load_raw(GSGLOBAL *gsGlobal, GSTEXTURE *texture, const char *path,
                           int width, int height, int psm);

int image_loader_load_tim2(GSGLOBAL *gsGlobal, GSTEXTURE *texture, const char *path);
int image_loader_load_tim2_32(GSGLOBAL *gsGlobal, GSTEXTURE *texture, const char *path);
int image_loader_load_tim2_24(GSGLOBAL *gsGlobal, GSTEXTURE *texture, const char *path);
int image_loader_load_tim2_16(GSGLOBAL *gsGlobal, GSTEXTURE *texture, const char *path);
int image_loader_load_tim2_16s(GSGLOBAL *gsGlobal, GSTEXTURE *texture, const char *path);
int image_loader_load_tim2_8(GSGLOBAL *gsGlobal, GSTEXTURE *texture, const char *path);
int image_loader_load_tim2_4(GSGLOBAL *gsGlobal, GSTEXTURE *texture, const char *path);
int image_loader_load_tim2_8h(GSGLOBAL *gsGlobal, GSTEXTURE *texture, const char *path);
int image_loader_load_tim2_4hl(GSGLOBAL *gsGlobal, GSTEXTURE *texture, const char *path);
int image_loader_load_tim2_4hh(GSGLOBAL *gsGlobal, GSTEXTURE *texture, const char *path);

int image_loader_load_tim(GSGLOBAL *gsGlobal, GSTEXTURE *texture, const char *path);

int image_loader_load(GSGLOBAL *gsGlobal, GSTEXTURE *texture, const char *path,
                       int raw_width, int raw_height, int raw_psm);

#endif
