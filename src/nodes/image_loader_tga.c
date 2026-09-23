/*
 * ============================================================
 * image_loader_tga.c
 * ============================================================
 * راجع image_loader.h للتوثيق الكامل.
 * ============================================================
 */

#include <gsKit.h>
#include <gsToolkit.h>

#include "image_loader.h"

int image_loader_load_tga(GSGLOBAL *gsGlobal, GSTEXTURE *texture, const char *path) {
    return gsKit_texture_tga(gsGlobal, texture, (char *)path) == 0;
}
