/*
 * ============================================================
 * image_loader_raw.c
 * ============================================================
 * See image_loader.h for full documentation.
 * ============================================================
 */

#include <gsKit.h>
#include <gsToolkit.h>

#include "image_loader.h"

int image_loader_load_raw(GSGLOBAL *gsGlobal, GSTEXTURE *texture, const char *path,
                           int width, int height, int psm) {
    /* No header at all - set the dimensions/format manually before calling,
     * exactly like the official gsKit example (examples/textures/textures.c) */
    texture->Width  = width;
    texture->Height = height;
    texture->PSM    = psm;
    return gsKit_texture_raw(gsGlobal, texture, (char *)path) == 0;
}
