/*
 * ============================================================
 * image_loader_jpeg.c
 * ============================================================
 * See image_loader.h for full documentation.
 * ============================================================
 */

#include <gsKit.h>
#include <gsToolkit.h>

#include "image_loader.h"

int image_loader_load_jpeg(GSGLOBAL *gsGlobal, GSTEXTURE *texture, const char *path) {
    return gsKit_texture_jpeg(gsGlobal, texture, (char *)path) == 0;
}
