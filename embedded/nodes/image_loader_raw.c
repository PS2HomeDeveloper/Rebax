/*
 * ============================================================
 * image_loader_raw.c
 * ============================================================
 * راجع image_loader.h للتوثيق الكامل.
 * ============================================================
 */

#include <gsKit.h>
#include <gsToolkit.h>

#include "image_loader.h"

int image_loader_load_raw(GSGLOBAL *gsGlobal, GSTEXTURE *texture, const char *path,
                           int width, int height, int psm) {
    /* بلا هيدر إطلاقاً - نضبط الأبعاد/الصيغة يدوياً قبل الاستدعاء،
     * بالضبط زي مثال gsKit الرسمي (examples/textures/textures.c) */
    texture->Width  = width;
    texture->Height = height;
    texture->PSM    = psm;
    return gsKit_texture_raw(gsGlobal, texture, (char *)path) == 0;
}
