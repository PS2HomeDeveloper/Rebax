/*
 * ============================================================
 * image_loader_common.c
 * ============================================================
 * راجع image_loader_internal.h للتوثيق الكامل.
 * ============================================================
 */

#include <stdlib.h>

#include <gsKit.h>

#include "image_loader_internal.h"

unsigned int read_u32le(const unsigned char *p) {
    return (unsigned int)p[0] | ((unsigned int)p[1] << 8)
         | ((unsigned int)p[2] << 16) | ((unsigned int)p[3] << 24);
}

unsigned short read_u16le(const unsigned char *p) {
    return (unsigned short)(p[0] | (p[1] << 8));
}

unsigned long long read_u64le(const unsigned char *p) {
    unsigned long long lo = read_u32le(p);
    unsigned long long hi = read_u32le(p + 4);
    return lo | (hi << 32);
}

int finish_texture_upload(GSGLOBAL *gsGlobal, GSTEXTURE *texture) {
    texture->Vram = gsKit_vram_alloc(gsGlobal,
        gsKit_texture_size(texture->Width, texture->Height, texture->PSM),
        GSKIT_ALLOC_USERBUFFER);
    if (texture->Vram == GSKIT_ALLOC_ERROR) {
        return 0;
    }

    if (texture->Clut != NULL) {
        if (texture->PSM == GS_PSM_T4) {
            texture->VramClut = gsKit_vram_alloc(gsGlobal,
                gsKit_texture_size(8, 2, GS_PSM_CT32), GSKIT_ALLOC_USERBUFFER);
        } else {
            texture->VramClut = gsKit_vram_alloc(gsGlobal,
                gsKit_texture_size(16, 16, GS_PSM_CT32), GSKIT_ALLOC_USERBUFFER);
        }
        if (texture->VramClut == GSKIT_ALLOC_ERROR) {
            return 0;
        }
    }

    gsKit_texture_upload(gsGlobal, texture);

    free(texture->Mem);
    texture->Mem = NULL;
    if (texture->Clut != NULL) {
        free(texture->Clut);
        texture->Clut = NULL;
    }

    return 1;
}
