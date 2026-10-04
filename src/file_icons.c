#include <stddef.h>
#include <string.h>
#include <ctype.h>

#include "file_icons.h"
#include "icon_atlas.h" /* ICON_* constants */

static int ext_equals(const char *a, const char *b) {
    while (*a != '\0' && *b != '\0') {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) return 0;
        a++;
        b++;
    }
    return *a == *b;
}

int file_icons_is_raster_image_ext(const char *ext) {
    static const char *exts[] = { "png", "jpg", "jpeg", "bmp", "tga", "tif", "tiff", "raw", "tm2", "tim2", "tim" };
    for (size_t i = 0; i < sizeof(exts) / sizeof(exts[0]); i++) {
        if (ext_equals(ext, exts[i])) return 1;
    }
    return 0;
}

int file_icons_for_name(const char *file_name) {
    const char *dot = strrchr(file_name, '.');
    if (dot == NULL) return ICON_file;
    const char *ext = dot + 1;
    if (ext_equals(ext, "c"))   return ICON_file_c;
    if (ext_equals(ext, "h"))   return ICON_file_h;
    if (ext_equals(ext, "cpp")) return ICON_file_cpp;
    if (ext_equals(ext, "chh")) return ICON_file_chh;
    if (ext_equals(ext, "rscene")) return ICON_scene;
    if (file_icons_is_raster_image_ext(ext)) return ICON_Broken_Image;
    if (ext_equals(ext, "wav") || ext_equals(ext, "mp3") || ext_equals(ext, "ogg"))
        return ICON_Audio_file;
    return ICON_file;
}
