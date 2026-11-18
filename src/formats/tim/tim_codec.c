/*
 *  ____  _____ ____    _    __  __
 * |  _ \| ____| __ )  / \   \ \/ /
 * | |_) | _| | _ \ / _ \   \  /
 * |  _ <| |___| |_) / ___ \  /  \
 * |_| \_\_____|____/_/   \_\/_/\_\
 *
 * Rebax Engine
 * Copyright (c) 2026 PS2HomeDeveloper
 *
 * Licensed under the MIT License.
 */
#include "formats/tim/tim_internal.h"
#include <stdlib.h>
#include <string.h>
#include <strings.h>
void rebax_tim_encode_options_default(rebax_tim_encode_options_t *o) {
    if (!o)
        return;
    memset(o, 0, sizeof(*o));
    o->premultiply_alpha = 1;
    o->clut_format = 32;
    o->palette_count = 1;
}
int rebax_tim_decode_file_level(const char *path, int level, rebax_decoded_image_t *out) {
    unsigned char *d = NULL;
    size_t n = 0;
    int ok = 0;
    if (!out || !path)
        return 0;
    memset(out, 0, sizeof(*out));
    if (!tim_read_file(path, &d, &n))
        return 0;
    if (n >= 4 && !memcmp(d, "TIM2", 4))
        ok = tim2_decode_mem(d, n, level, out);
    else if (n >= 4 && tim_rd32(d) == 0x10u)
        ok = tim1_decode_mem(d, n, out);
    free(d);
    return ok;
}
int rebax_tim_decode_file(const char *path, rebax_decoded_image_t *out) {
    return rebax_tim_decode_file_level(path, 0, out);
}
void rebax_tim_decoded_free(rebax_decoded_image_t *i) {
    if (i) {
        free(i->rgba);
        memset(i, 0, sizeof(*i));
    }
}
int rebax_tim_encode_file(const char *path, const unsigned char *rgba, int w, int h, rebax_tim1_format_t f,
                          const rebax_tim_encode_options_t *o) {
    tim_buffer_t b = {0};
    int ok = tim1_encode_mem(rgba, w, h, f, o, &b);
    if (ok)
        ok = tim_write_file(path, b.p, b.n);
    free(b.p);
    return ok;
}
int rebax_tim2_encode_file(const char *path, const unsigned char *rgba, int w, int h, rebax_tim2_format_t f,
                           const rebax_tim_encode_options_t *o) {
    tim_buffer_t b = {0};
    int ok = tim2_encode_mem(rgba, w, h, f, o, &b);
    if (ok)
        ok = tim_write_file(path, b.p, b.n);
    free(b.p);
    return ok;
}
int rebax_tim_write_ps1_16(const char *path, const unsigned char *rgba, int w, int h) {
    rebax_tim_encode_options_t o;
    rebax_tim_encode_options_default(&o);
    return rebax_tim_encode_file(path, rgba, w, h, REBAX_TIM1_16BIT, &o);
}
int rebax_tim1_format_from_file(const char *path) {
    unsigned char *d;
    size_t n;
    if (!tim_read_file(path, &d, &n))
        return -1;
    if (n < 8 || tim_rd32(d) != 0x10u) {
        free(d);
        return -1;
    }
    free(d);
    return 0;
}
int rebax_tim2_format_from_file(const char *path) {
    unsigned char *d;
    size_t n;
    unsigned p;
    if (!tim_read_file(path, &d, &n))
        return -1;
    if (n < 40 || memcmp(d, "TIM2", 4)) {
        free(d);
        return -1;
    }
    p = (unsigned)((tim_rd64(d + 40) >> 20) & 63);
    free(d);
    switch (p) {
    case TIM2_PSM_CT32:
        return REBAX_TIM2_32BIT;
    case TIM2_PSM_CT24:
        return REBAX_TIM2_24BIT;
    case TIM2_PSM_CT16:
        return REBAX_TIM2_16BIT;
    case TIM2_PSM_CT16S:
        return REBAX_TIM2_16S;
    case TIM2_PSM_T8:
        return REBAX_TIM2_8BIT;
    case TIM2_PSM_T4:
        return REBAX_TIM2_4BIT;
    case TIM2_PSM_T8H:
        return REBAX_TIM2_8H;
    case TIM2_PSM_T4HL:
        return REBAX_TIM2_4HL;
    case TIM2_PSM_T4HH:
        return REBAX_TIM2_4HH;
    default:
        return -1;
    }
}
int rebax_image_format_supported(rebax_image_platform_t p, const char *e) {
    if (!e)
        return 0;
    if (p == REBAX_IMAGE_PLATFORM_PS1)
        return !strcasecmp(e, ".tim") || !strcasecmp(e, "tim");
    if (p == REBAX_IMAGE_PLATFORM_PS2)
        return !strcasecmp(e, ".tm2") || !strcasecmp(e, "tm2") || !strcasecmp(e, ".tim2") ||
               !strcasecmp(e, "tim2") || !strcasecmp(e, ".raw") || !strcasecmp(e, "raw") ||
               !strcasecmp(e, ".tex") || !strcasecmp(e, "tex") || !strcasecmp(e, ".png") ||
               !strcasecmp(e, "png");
    return !strcasecmp(e, ".png") || !strcasecmp(e, "png") || !strcasecmp(e, ".jpg") ||
           !strcasecmp(e, "jpg") || !strcasecmp(e, ".jpeg") || !strcasecmp(e, "jpeg") ||
           !strcasecmp(e, ".bmp") || !strcasecmp(e, "bmp") || !strcasecmp(e, ".tga") ||
           !strcasecmp(e, "tga") || !strcasecmp(e, ".tif") || !strcasecmp(e, "tif") ||
           !strcasecmp(e, ".tiff") || !strcasecmp(e, "tiff");
}
int rebax_image_format_rank(rebax_image_platform_t p, const char *e) {
    if (!rebax_image_format_supported(p, e))
        return -1;
    if (p == REBAX_IMAGE_PLATFORM_PS2) {
        if (!strcasecmp(e, ".tm2") || !strcasecmp(e, "tm2"))
            return 0;
        if (!strcasecmp(e, ".raw") || !strcasecmp(e, "raw"))
            return 1;
        if (!strcasecmp(e, ".tex") || !strcasecmp(e, "tex"))
            return 2;
    }
    return 3;
}
const char *rebax_image_best_format(rebax_image_platform_t p) {
    return p == REBAX_IMAGE_PLATFORM_PS1 ? ".tim" : p == REBAX_IMAGE_PLATFORM_PS2 ? ".tm2" : ".png";
}
