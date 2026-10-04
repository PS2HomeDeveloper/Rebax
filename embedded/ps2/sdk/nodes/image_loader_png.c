/*
 * ============================================================
 * image_loader_png.c
 * ============================================================
 * See image_loader.h for full documentation.
 * ============================================================
 */

#include <gsKit.h>
#include <gsToolkit.h>

#include "image_loader.h"

int image_loader_load_png(GSGLOBAL *gsGlobal, GSTEXTURE *texture, const char *path) {
    return gsKit_texture_png(gsGlobal, texture, (char *)path) == 0;
}
