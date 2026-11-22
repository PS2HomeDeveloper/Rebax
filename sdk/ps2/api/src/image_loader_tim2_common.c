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

int image_loader_load_tim2_format(GSGLOBAL *gsGlobal, GSTEXTURE *texture, const char *path,
                                  unsigned char expected_psm) {
    FILE *f = fopen(path, "rb");
    if (f == NULL)
        return 0;

    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return 0;
    }
    long file_size = ftell(f);
    if (file_size < 16 + 48) {
        fclose(f);
        return 0;
    }
    if (fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return 0;
    }

    unsigned char *data = malloc((size_t)file_size);
    if (data == NULL) {
        fclose(f);
        return 0;
    }

    if (fread(data, 1, (size_t)file_size, f) != (size_t)file_size) {
        free(data);
        fclose(f);
        return 0;
    }
    fclose(f);

    if (memcmp(data, "TIM2", 4) != 0) {
        free(data);
        return 0;
    }

    const size_t PIC_OFFSET = 16;

    unsigned int clut_size = read_u32le(data + PIC_OFFSET + 4);
    unsigned int img_size = read_u32le(data + PIC_OFFSET + 8);
    unsigned short hdr_size = read_u16le(data + PIC_OFFSET + 12);
    unsigned short width = read_u16le(data + PIC_OFFSET + 20);
    unsigned short height = read_u16le(data + PIC_OFFSET + 22);
    unsigned long long gs_tex0 = read_u64le(data + PIC_OFFSET + 24);

    unsigned char psm = (unsigned char)((gs_tex0 >> 20) & 0x3F);
    if (expected_psm != 0xff && psm != expected_psm) {
        free(data);
        return 0;
    }
    unsigned char cpsm = (unsigned char)((gs_tex0 >> 51) & 0xF);

    size_t img_start = PIC_OFFSET + hdr_size;
    if (img_start + (size_t)img_size + (size_t)clut_size > (size_t)file_size) {
        free(data);
        return 0;
    }

    texture->Width = width;
    texture->Height = height;
    texture->PSM = psm;
    texture->Filter = GS_FILTER_NEAREST;

    texture->Mem = memalign(128, img_size);
    if (texture->Mem == NULL) {
        free(data);
        return 0;
    }
    memcpy(texture->Mem, data + img_start, img_size);

    if (clut_size > 0) {
        texture->ClutPSM = cpsm;
        texture->Clut = memalign(128, clut_size);
        if (texture->Clut == NULL) {
            free(texture->Mem);
            texture->Mem = NULL;
            free(data);
            return 0;
        }
        memcpy(texture->Clut, data + img_start + img_size, clut_size);
    } else {
        texture->Clut = NULL;
        texture->ClutPSM = 0;
    }

    free(data);

    return finish_texture_upload(gsGlobal, texture);
}

int image_loader_load_tim2(GSGLOBAL *gsGlobal, GSTEXTURE *texture, const char *path) {
    return image_loader_load_tim2_format(gsGlobal, texture, path, 0xff);
}
