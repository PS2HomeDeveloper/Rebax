/*
 * ============================================================
 * image_loader.c
 * ============================================================
 * الدالة العامة المتفرّعة بس - راجع image_loader.h لشرح ليش
 * التصدير الفعلي ما يستخدمها (يستدعي الدوال بملفات image_loader_*.c
 * المستقلة مباشرة بدلها، حسب الصيغ المستخدمة فعلياً بالمشروع).
 * ============================================================
 */

#include <string.h>
#include <strings.h> /* strcasecmp */

#include "image_loader.h"

/* true لو path تنتهي بـext (بلا حساسية لحالة الأحرف) - يشمل النقطة
 * بـext نفسها (مثال: ends_with(path, ".png")) */
static int ends_with(const char *path, const char *ext) {
    size_t path_len = strlen(path);
    size_t ext_len = strlen(ext);
    if (ext_len > path_len) return 0;
    return strcasecmp(path + (path_len - ext_len), ext) == 0;
}

int image_loader_load(GSGLOBAL *gsGlobal, GSTEXTURE *texture, const char *path,
                       int raw_width, int raw_height, int raw_psm) {
    if (ends_with(path, ".png")) {
        return image_loader_load_png(gsGlobal, texture, path);
    } else if (ends_with(path, ".jpg") || ends_with(path, ".jpeg")) {
        return image_loader_load_jpeg(gsGlobal, texture, path);
    } else if (ends_with(path, ".bmp")) {
        return image_loader_load_bmp(gsGlobal, texture, path);
    } else if (ends_with(path, ".tga")) {
        return image_loader_load_tga(gsGlobal, texture, path);
    } else if (ends_with(path, ".tif") || ends_with(path, ".tiff")) {
        return image_loader_load_tiff(gsGlobal, texture, path);
    } else if (ends_with(path, ".raw")) {
        return image_loader_load_raw(gsGlobal, texture, path, raw_width, raw_height, raw_psm);
    } else if (ends_with(path, ".tm2") || ends_with(path, ".tim2")) {
        return image_loader_load_tim2(gsGlobal, texture, path);
    } else if (ends_with(path, ".tim")) {
        return image_loader_load_tim(gsGlobal, texture, path);
    }

    return 0; /* صيغة غير مدعومة (GIF أو أي شيء آخر) */
}
