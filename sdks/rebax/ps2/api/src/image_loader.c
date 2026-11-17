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

/* The generic dispatching function only - see image_loader.h for why
 * the actual export doesn't use it (the exporter invokes the
 * functions in the image_loader_*.c files directly instead, depending
 * on which formats are actually used in the project). */

#include <string.h>
#include <strings.h> /* strcasecmp */

#include "image_decoder.h"

#if defined(__has_include)
#if __has_include("image_loader_config.h")
#include "image_loader_config.h"
#endif
#endif

#ifndef REBAX_IMAGE_LOADER_PNG
#define REBAX_IMAGE_LOADER_PNG 1
#define REBAX_IMAGE_LOADER_JPEG 1
#define REBAX_IMAGE_LOADER_BMP 1
#define REBAX_IMAGE_LOADER_TGA 1
#define REBAX_IMAGE_LOADER_TIFF 1
#define REBAX_IMAGE_LOADER_RAW 1
#define REBAX_IMAGE_LOADER_TIM2 1
#define REBAX_IMAGE_LOADER_TIM 1
#endif

/* true if path ends with ext (case-insensitive) - ext includes the dot
 * itself (example: ends_with(path, ".png")) */
static int ends_with(const char *path, const char *ext) {
    size_t path_len = strlen(path);
    size_t ext_len = strlen(ext);
    if (ext_len > path_len) return 0;
    return strcasecmp(path + (path_len - ext_len), ext) == 0;
}

int image_loader_load(GSGLOBAL *gsGlobal, GSTEXTURE *texture, const char *path,
                       int raw_width, int raw_height, int raw_psm) {
    if (REBAX_IMAGE_LOADER_PNG && ends_with(path, ".png")) {
        return image_loader_load_png(gsGlobal, texture, path);
    } else if (REBAX_IMAGE_LOADER_JPEG && (ends_with(path, ".jpg") || ends_with(path, ".jpeg"))) {
        return image_loader_load_jpeg(gsGlobal, texture, path);
    } else if (REBAX_IMAGE_LOADER_BMP && ends_with(path, ".bmp")) {
        return image_loader_load_bmp(gsGlobal, texture, path);
    } else if (REBAX_IMAGE_LOADER_TGA && ends_with(path, ".tga")) {
        return image_loader_load_tga(gsGlobal, texture, path);
    } else if (REBAX_IMAGE_LOADER_TIFF && (ends_with(path, ".tif") || ends_with(path, ".tiff"))) {
        return image_loader_load_tiff(gsGlobal, texture, path);
    } else if (REBAX_IMAGE_LOADER_RAW && ends_with(path, ".raw")) {
        return image_loader_load_raw(gsGlobal, texture, path, raw_width, raw_height, raw_psm);
    } else if (REBAX_IMAGE_LOADER_TIM2 && (ends_with(path, ".tm2") || ends_with(path, ".tim2"))) {
        return image_loader_load_tim2(gsGlobal, texture, path);
    } else if (REBAX_IMAGE_LOADER_TIM && ends_with(path, ".tim")) {
        return image_loader_load_tim(gsGlobal, texture, path);
    }

    return 0; /* Unsupported format (GIF or anything else) */
}
