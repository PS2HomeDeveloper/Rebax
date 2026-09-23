/*
 * ============================================================
 * image_loader_tiff.c
 * ============================================================
 * راجع image_loader.h للتوثيق الكامل.
 * ============================================================
 */

#include <gsKit.h>
#include <gsToolkit.h>

#include "image_loader.h"

int image_loader_load_tiff(GSGLOBAL *gsGlobal, GSTEXTURE *texture, const char *path) {
    return gsKit_texture_tiff(gsGlobal, texture, (char *)path) == 0;
}
