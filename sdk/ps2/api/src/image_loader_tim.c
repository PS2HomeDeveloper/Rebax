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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>

#include <gsKit.h>

#include "image_decoder.h"
#include "image_loader_internal.h"

#define TIM_ID              0x00000010
#define TIM_CF_CLUT_PRESENT 0x8
#define TIM_PMODE_4BIT      0
#define TIM_PMODE_8BIT      1
#define TIM_PMODE_16BIT     2
#define TIM_PMODE_24BIT     3

int image_loader_load_tim(GSGLOBAL *gsGlobal, GSTEXTURE *texture, const char *path) {
    FILE *f = fopen(path, "rb");
    if (f == NULL) return 0;

    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return 0; }
    long file_size = ftell(f);
    if (file_size < 8 + 12) { fclose(f); return 0; }
    if (fseek(f, 0, SEEK_SET) != 0) { fclose(f); return 0; }

    unsigned char *data = malloc((size_t)file_size);
    if (data == NULL) { fclose(f); return 0; }

    if (fread(data, 1, (size_t)file_size, f) != (size_t)file_size) {
        free(data);
        fclose(f);
        return 0;
    }
    fclose(f);

    unsigned int tim_id = read_u32le(data + 0);
    unsigned int flag   = read_u32le(data + 4);
    if (tim_id != TIM_ID) { free(data); return 0; }

    int pmode    = (int)(flag & 0x3);
    int has_clut = (flag & TIM_CF_CLUT_PRESENT) != 0;

    size_t off = 8;
    unsigned int clut_colors_count = 0;
    unsigned char *clut_colors_ptr = NULL;

    if (has_clut) {
        if (off + 12 > (size_t)file_size) { free(data); return 0; }
        unsigned int clut_len = read_u32le(data + off);
        unsigned short clut_w = read_u16le(data + off + 8);
        unsigned short clut_h = read_u16le(data + off + 10);
        clut_colors_count = (unsigned int)clut_w * (unsigned int)clut_h;
        clut_colors_ptr = data + off + 12;
        if (off + clut_len > (size_t)file_size) { free(data); return 0; }
        off += clut_len;
    }

    if (off + 12 > (size_t)file_size) { free(data); return 0; }
    unsigned int img_len       = read_u32le(data + off);
    unsigned short width_units = read_u16le(data + off + 8);
    unsigned short height      = read_u16le(data + off + 10);
    unsigned char *img_data_ptr = data + off + 12;
    if (off + img_len > (size_t)file_size) { free(data); return 0; }

    int width;
    unsigned char psm;
    switch (pmode) {
        case TIM_PMODE_4BIT:  width = width_units * 4; psm = GS_PSM_T4;  break;
        case TIM_PMODE_8BIT:  width = width_units * 2; psm = GS_PSM_T8;  break;
        case TIM_PMODE_16BIT: width = width_units;     psm = GS_PSM_CT16; break;
        case TIM_PMODE_24BIT: width = (width_units * 2) / 3; psm = GS_PSM_CT24; break;
        default: free(data); return 0;
    }

    size_t img_data_size = (size_t)img_len - 12;

    texture->Width  = width;
    texture->Height = height;
    texture->PSM    = psm;
    texture->Filter = GS_FILTER_NEAREST;

    texture->Mem = memalign(128, img_data_size);
    if (texture->Mem == NULL) { free(data); return 0; }
    memcpy(texture->Mem, img_data_ptr, img_data_size);

    if (has_clut && clut_colors_count > 0) {
        size_t clut_size = (size_t)clut_colors_count * 2;
        texture->ClutPSM = GS_PSM_CT16;
        texture->Clut = memalign(128, clut_size);
        if (texture->Clut == NULL) {
            free(texture->Mem);
            texture->Mem = NULL;
            free(data);
            return 0;
        }
        memcpy(texture->Clut, clut_colors_ptr, clut_size);
    } else {
        texture->Clut = NULL;
        texture->ClutPSM = 0;
    }

    free(data);

    return finish_texture_upload(gsGlobal, texture);
}
