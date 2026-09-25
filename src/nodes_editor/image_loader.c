/*
 * ============================================================
 * image_loader.c (nodes_editor)
 * ============================================================
 * راجع image_loader.h لتوثيق التصميم الكامل.
 *
 * ملاحظة تشغيل مهمة (TIM2): بيانات البكسل تُقرأ هنا بترتيب خطي
 * (raster) مباشر - هذا يفترض إن ملفات .tm2 المستخدمة بالمشروع
 * أُنتجت **بدون** خيار --swizzle بأداة ps2_tim2_tool.py (نفس
 * الافتراض المستخدم بنسخة PS2 src/nodes/image_loader.c، وهو نفس
 * افتراض دالة extract_tm2 الرسمية بالأداة نفسها - راجع تعليق
 * ps2_tim2_tool.py حول --swizzle: "استخدم فقط إن كان loader
 * المشروع يتوقع بيانات مُرتَّبة مسبقاً"، ومحركنا لا يتوقع هذا).
 *
 * لكن CLUT الصور المفهرسة 8-بت (بعكس بيانات البكسل) **دائماً**
 * مُرتَّبة بنظام CSM1 بغض النظر عن --swizzle (إلزامي بالأداة -
 * راجع تعليق convert_8bit الرسمي: "CLUT Swizzle إلزامي لـ PS2") -
 * لازم نعيد ترتيبها هنا قبل الاستخدام. عملية إعادة الترتيب هذي ذاتية
 * العكس (نفس الدالة تُستخدم للتشفير وفك التشفير - تحققت من
 * _unswizzle_clut8 الرسمية بالأداة: نفس جدول التبديل [0,2,1,3]
 * بالضبط). CLUT الصور 4-بت لا تحتاج أي ترتيب (الأداة توثّق هذا
 * صراحة: "16 لون، لا يحتاج Swizzle").
 * ============================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h> /* strcasecmp */

#include "image_loader.h"
#include "stb_image.h" /* بلا STB_IMAGE_IMPLEMENTATION هنا - icon_atlas.c عرّفها مرة وحدة بالمشروع كامل */

/* ثوابت PSM (نفس قيم GS_PSM_* الحقيقية بـps2sdk، منسوخة هنا بالاسم
 * بس - هذا الملف aarch64 عادي، ما يقدر يتضمّن gsKit.h إطلاقاً) */
#define PSM_32   0x00
#define PSM_24   0x01
#define PSM_16   0x02
#define PSM_16S  0x0A
#define PSM_T8   0x13
#define PSM_T4   0x14

static int ends_with(const char *path, const char *ext) {
    size_t path_len = strlen(path);
    size_t ext_len = strlen(ext);
    if (ext_len > path_len) return 0;
    return strcasecmp(path + (path_len - ext_len), ext) == 0;
}

static unsigned int read_u32le(const unsigned char *p) {
    return (unsigned int)p[0] | ((unsigned int)p[1] << 8)
         | ((unsigned int)p[2] << 16) | ((unsigned int)p[3] << 24);
}

static unsigned short read_u16le(const unsigned char *p) {
    return (unsigned short)(p[0] | (p[1] << 8));
}

static unsigned long long read_u64le(const unsigned char *p) {
    unsigned long long lo = read_u32le(p);
    unsigned long long hi = read_u32le(p + 4);
    return lo | (hi << 32);
}

/* round(ps2_alpha * 255 / 128) مقصوص لـ255 - عكس دالة ps2_alpha
 * الرسمية بالأداة بالضبط (raw*128/255 بالاتجاه المعاكس) */
static unsigned char unscale_ps2_alpha(unsigned char ps2_alpha_val) {
    int a = (int)((ps2_alpha_val * 255 + 64) / 128); /* +64 للتقريب لأقرب عدد صحيح */
    if (a > 255) a = 255;
    return (unsigned char)a;
}

/* فك حزمة 5551 (ترتيب A1 B5 G5 R5 - نفس تعليق image_loader.h
 * فوق). توسيع 5-بت لـ8-بت بتكرار البتات (r5<<3)|(r5>>2) - نفس
 * الصيغة الحرفية المستخدمة بكل من ps2_tim2_tool.py وps1_tim_tool.py
 * (تحققت من الكود المصدري: r = (r5 << 3) | (r5 >> 2)) - مو
 * round(r5*255/31) التناسبي، الاثنان يختلفان بمقدار 1 أحياناً
 * (مثال: r5=24 → تكرار البتات=198، تناسبي=197) */
static void unpack_5551(unsigned short val, unsigned char *out_rgba) {
    unsigned int r5 = val & 0x1F;
    unsigned int g5 = (val >> 5) & 0x1F;
    unsigned int b5 = (val >> 10) & 0x1F;
    unsigned int a1 = (val >> 15) & 0x1;

    out_rgba[0] = (unsigned char)((r5 << 3) | (r5 >> 2));
    out_rgba[1] = (unsigned char)((g5 << 3) | (g5 >> 2));
    out_rgba[2] = (unsigned char)((b5 << 3) | (b5 >> 2));
    out_rgba[3] = a1 ? 255 : 0;
}

/* نفس clut8_swizzle الرسمية بالأداة بالضبط - ذاتية العكس (نفس
 * الدالة للتشفير وفك التشفير، تحققت من _unswizzle_clut8 الرسمية:
 * جدول التبديل [0,2,1,3] مطابق) - تُستخدم هنا للفك بس */
static void unswizzle_clut8(const unsigned char *in_rgba256, unsigned char *out_rgba256) {
    static const int stripe_map[4] = { 0, 2, 1, 3 };
    for (int i = 0; i < 256; i++) {
        int block = i / 32;
        int inner = i % 32;
        int stripe = inner / 8;
        int pos = inner % 8;
        int new_stripe = stripe_map[stripe];
        int j = block * 32 + new_stripe * 8 + pos;
        memcpy(out_rgba256 + i * 4, in_rgba256 + j * 4, 4);
    }
}

static unsigned char *load_generic(const char *path, int *out_width, int *out_height) {
    int w, h, channels_in_file;
    unsigned char *pixels = stbi_load(path, &w, &h, &channels_in_file, 4);
    if (pixels == NULL) return NULL;
    *out_width = w;
    *out_height = h;
    return pixels;
}

static unsigned char *load_raw(const char *path, int width, int height, int psm) {
    if (width <= 0 || height <= 0) return NULL;

    FILE *f = fopen(path, "rb");
    if (f == NULL) return NULL;

    size_t pixel_count = (size_t)width * (size_t)height;
    unsigned char *out = malloc(pixel_count * 4);
    if (out == NULL) { fclose(f); return NULL; }

    int ok = 1;
    if (psm == PSM_32) {
        unsigned char *raw = malloc(pixel_count * 4);
        if (raw == NULL || fread(raw, 1, pixel_count * 4, f) != pixel_count * 4) ok = 0;
        else memcpy(out, raw, pixel_count * 4);
        free(raw);
    } else if (psm == PSM_24) {
        unsigned char *raw = malloc(pixel_count * 3);
        if (raw == NULL || fread(raw, 1, pixel_count * 3, f) != pixel_count * 3) ok = 0;
        else {
            for (size_t i = 0; i < pixel_count; i++) {
                out[i * 4 + 0] = raw[i * 3 + 0];
                out[i * 4 + 1] = raw[i * 3 + 1];
                out[i * 4 + 2] = raw[i * 3 + 2];
                out[i * 4 + 3] = 255;
            }
        }
        free(raw);
    } else if (psm == PSM_16 || psm == PSM_16S) {
        unsigned char *raw = malloc(pixel_count * 2);
        if (raw == NULL || fread(raw, 1, pixel_count * 2, f) != pixel_count * 2) ok = 0;
        else {
            for (size_t i = 0; i < pixel_count; i++) {
                unsigned short val = read_u16le(raw + i * 2);
                unpack_5551(val, out + i * 4);
            }
        }
        free(raw);
    } else {
        ok = 0; /* صيغ مفهرسة (T4/T8) غير مدعومة لـRAW - راجع تعليق الهيدر */
    }

    fclose(f);
    if (!ok) { free(out); return NULL; }
    return out;
}

static unsigned char *load_tim2(const char *path, int *out_width, int *out_height) {
    FILE *f = fopen(path, "rb");
    if (f == NULL) return NULL;

    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return NULL; }
    long file_size = ftell(f);
    if (file_size < 16 + 48) { fclose(f); return NULL; }
    if (fseek(f, 0, SEEK_SET) != 0) { fclose(f); return NULL; }

    unsigned char *data = malloc((size_t)file_size);
    if (data == NULL) { fclose(f); return NULL; }
    if (fread(data, 1, (size_t)file_size, f) != (size_t)file_size) {
        free(data); fclose(f); return NULL;
    }
    fclose(f);

    if (memcmp(data, "TIM2", 4) != 0) { free(data); return NULL; }

    const size_t PIC_OFFSET = 16;
    unsigned int clut_size    = read_u32le(data + PIC_OFFSET + 4);
    unsigned int img_size     = read_u32le(data + PIC_OFFSET + 8);
    unsigned short hdr_size   = read_u16le(data + PIC_OFFSET + 12);
    unsigned short width      = read_u16le(data + PIC_OFFSET + 20);
    unsigned short height     = read_u16le(data + PIC_OFFSET + 22);
    unsigned long long gs_tex0 = read_u64le(data + PIC_OFFSET + 24);
    unsigned char psm = (unsigned char)((gs_tex0 >> 20) & 0x3F);

    size_t img_start = PIC_OFFSET + hdr_size;
    if (img_start + (size_t)img_size + (size_t)clut_size > (size_t)file_size) {
        free(data);
        return NULL;
    }
    const unsigned char *img_data = data + img_start;
    const unsigned char *clut_data = data + img_start + img_size;

    size_t pixel_count = (size_t)width * (size_t)height;
    unsigned char *out = malloc(pixel_count * 4);
    if (out == NULL) { free(data); return NULL; }

    int ok = 1;
    if (psm == PSM_32) {
        for (size_t i = 0; i < pixel_count; i++) {
            out[i * 4 + 0] = img_data[i * 4 + 0];
            out[i * 4 + 1] = img_data[i * 4 + 1];
            out[i * 4 + 2] = img_data[i * 4 + 2];
            out[i * 4 + 3] = unscale_ps2_alpha(img_data[i * 4 + 3]);
        }
    } else if (psm == PSM_24) {
        for (size_t i = 0; i < pixel_count; i++) {
            out[i * 4 + 0] = img_data[i * 3 + 0];
            out[i * 4 + 1] = img_data[i * 3 + 1];
            out[i * 4 + 2] = img_data[i * 3 + 2];
            out[i * 4 + 3] = 255;
        }
    } else if (psm == PSM_16 || psm == PSM_16S) {
        for (size_t i = 0; i < pixel_count; i++) {
            unsigned short val = read_u16le(img_data + i * 2);
            unpack_5551(val, out + i * 4);
        }
    } else if (psm == PSM_T8) {
        /* CLUT مخزَّنة RGBA32 (256 لون، 4 بايت لكل لون) - ألفا بمقياس
         * PS2 (راجع unscale_ps2_alpha) - لازم فك ترتيب CSM1 أولاً */
        unsigned char palette_raw[256 * 4];
        unsigned char palette[256 * 4];
        memcpy(palette_raw, clut_data, 256 * 4);
        unswizzle_clut8(palette_raw, palette);
        for (int i = 0; i < 256; i++) {
            palette[i * 4 + 3] = unscale_ps2_alpha(palette[i * 4 + 3]);
        }
        for (size_t i = 0; i < pixel_count; i++) {
            unsigned char idx = img_data[i];
            memcpy(out + i * 4, palette + (size_t)idx * 4, 4);
        }
    } else if (psm == PSM_T4) {
        /* CLUT 16 لون RGBA32 - بلا أي ترتيب خاص (راجع تعليق الهيدر) */
        unsigned char palette[16 * 4];
        memcpy(palette, clut_data, 16 * 4);
        for (int i = 0; i < 16; i++) {
            palette[i * 4 + 3] = unscale_ps2_alpha(palette[i * 4 + 3]);
        }
        for (size_t i = 0; i < pixel_count; i += 2) {
            unsigned char byte = img_data[i / 2];
            unsigned char lo = byte & 0x0F;
            unsigned char hi = (byte >> 4) & 0x0F;
            memcpy(out + i * 4, palette + (size_t)lo * 4, 4);
            if (i + 1 < pixel_count) memcpy(out + (i + 1) * 4, palette + (size_t)hi * 4, 4);
        }
    } else {
        ok = 0;
    }

    free(data);
    if (!ok) { free(out); return NULL; }
    *out_width = width;
    *out_height = height;
    return out;
}

/* ثوابت صيغة PS1 (نفس ps1_tim_tool.py) */
#define TIM_ID              0x00000010
#define TIM_CF_CLUT_PRESENT 0x8
#define TIM_PMODE_4BIT      0
#define TIM_PMODE_8BIT      1
#define TIM_PMODE_16BIT     2
#define TIM_PMODE_24BIT     3

static unsigned char *load_tim(const char *path, int *out_width, int *out_height) {
    FILE *f = fopen(path, "rb");
    if (f == NULL) return NULL;

    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return NULL; }
    long file_size = ftell(f);
    if (file_size < 8 + 12) { fclose(f); return NULL; }
    if (fseek(f, 0, SEEK_SET) != 0) { fclose(f); return NULL; }

    unsigned char *data = malloc((size_t)file_size);
    if (data == NULL) { fclose(f); return NULL; }
    if (fread(data, 1, (size_t)file_size, f) != (size_t)file_size) {
        free(data); fclose(f); return NULL;
    }
    fclose(f);

    unsigned int tim_id = read_u32le(data + 0);
    unsigned int flag   = read_u32le(data + 4);
    if (tim_id != TIM_ID) { free(data); return NULL; }

    int pmode    = (int)(flag & 0x3);
    int has_clut = (flag & TIM_CF_CLUT_PRESENT) != 0;

    size_t off = 8;
    unsigned int clut_colors_count = 0;
    const unsigned char *clut_colors_ptr = NULL;

    if (has_clut) {
        if (off + 12 > (size_t)file_size) { free(data); return NULL; }
        unsigned int clut_len = read_u32le(data + off);
        unsigned short clut_w = read_u16le(data + off + 8);
        unsigned short clut_h = read_u16le(data + off + 10);
        clut_colors_count = (unsigned int)clut_w * (unsigned int)clut_h;
        clut_colors_ptr = data + off + 12;
        if (off + clut_len > (size_t)file_size) { free(data); return NULL; }
        off += clut_len;
    }

    if (off + 12 > (size_t)file_size) { free(data); return NULL; }
    unsigned short width_units = read_u16le(data + off + 8);
    unsigned short height      = read_u16le(data + off + 10);
    const unsigned char *img_data = data + off + 12;

    int width;
    switch (pmode) {
        case TIM_PMODE_4BIT:  width = width_units * 4; break;
        case TIM_PMODE_8BIT:  width = width_units * 2; break;
        case TIM_PMODE_16BIT: width = width_units; break;
        case TIM_PMODE_24BIT: width = (width_units * 2) / 3; break;
        default: free(data); return NULL;
    }

    size_t pixel_count = (size_t)width * (size_t)height;
    unsigned char *out = malloc(pixel_count * 4);
    if (out == NULL) { free(data); return NULL; }

    int ok = 1;
    if (pmode == TIM_PMODE_16BIT) {
        for (size_t i = 0; i < pixel_count; i++) {
            unsigned short val = read_u16le(img_data + i * 2);
            unpack_5551(val, out + i * 4);
        }
    } else if (pmode == TIM_PMODE_24BIT) {
        for (size_t i = 0; i < pixel_count; i++) {
            out[i * 4 + 0] = img_data[i * 3 + 0];
            out[i * 4 + 1] = img_data[i * 3 + 1];
            out[i * 4 + 2] = img_data[i * 3 + 2];
            out[i * 4 + 3] = 255;
        }
    } else if ((pmode == TIM_PMODE_8BIT || pmode == TIM_PMODE_4BIT) && has_clut) {
        /* CLUT صيغة 5551 خام (بلا أي ترتيب خاص - PS1 ما فيها هذا
         * المفهوم إطلاقاً، بعكس CLUT الـ8-بت بـTIM2/PS2) */
        unsigned int palette_count = (pmode == TIM_PMODE_8BIT) ? 256u : 16u;
        if (clut_colors_count < palette_count) { ok = 0; }
        else {
            unsigned char *palette = malloc((size_t)palette_count * 4);
            for (unsigned int i = 0; i < palette_count; i++) {
                unsigned short val = read_u16le(clut_colors_ptr + i * 2);
                unpack_5551(val, palette + i * 4);
            }
            if (pmode == TIM_PMODE_8BIT) {
                for (size_t i = 0; i < pixel_count; i++) {
                    unsigned char idx = img_data[i];
                    memcpy(out + i * 4, palette + (size_t)idx * 4, 4);
                }
            } else {
                for (size_t i = 0; i < pixel_count; i += 2) {
                    unsigned char byte = img_data[i / 2];
                    unsigned char lo = byte & 0x0F;
                    unsigned char hi = (byte >> 4) & 0x0F;
                    memcpy(out + i * 4, palette + (size_t)lo * 4, 4);
                    if (i + 1 < pixel_count) memcpy(out + (i + 1) * 4, palette + (size_t)hi * 4, 4);
                }
            }
            free(palette);
        }
    } else {
        ok = 0;
    }

    free(data);
    if (!ok) { free(out); return NULL; }
    *out_width = width;
    *out_height = height;
    return out;
}

unsigned char *image_loader_editor_load(const char *path, int *out_width, int *out_height,
                                         int raw_width, int raw_height, int raw_psm) {
    if (ends_with(path, ".png") || ends_with(path, ".jpg") || ends_with(path, ".jpeg") ||
        ends_with(path, ".bmp") || ends_with(path, ".tga") ||
        ends_with(path, ".tif") || ends_with(path, ".tiff")) {
        return load_generic(path, out_width, out_height);
    } else if (ends_with(path, ".raw")) {
        unsigned char *result = load_raw(path, raw_width, raw_height, raw_psm);
        if (result != NULL) { *out_width = raw_width; *out_height = raw_height; }
        return result;
    } else if (ends_with(path, ".tm2") || ends_with(path, ".tim2")) {
        return load_tim2(path, out_width, out_height);
    } else if (ends_with(path, ".tim")) {
        return load_tim(path, out_width, out_height);
    }

    return NULL; /* صيغة غير مدعومة (GIF أو أي شيء آخر) */
}
