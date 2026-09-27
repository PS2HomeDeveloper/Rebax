/*
 * ============================================================
 * image_loader_tim2.c
 * ============================================================
 * راجع image_loader.h للتوثيق الكامل.
 *
 * التخطيط الثنائي الدقيق مأخوذ حرفياً من أداة ps2_tim2_tool.py
 * المرجعية (نفس الأداة اللي تنتج ملفات .tm2 لمشروعك) - راجع دالتيها
 * build_tim2_file وbuild_picture_header بالضبط لتوثيق كل أوفست تحت:
 *
 * هيدر الملف (16 بايت، أول 4 بايت "TIM2"):
 *   0   4B  Magic "TIM2"
 *   4   1B  Version
 *   5   1B  Format (0 = Linear)
 *   6   2B  عدد الصور (LE)
 *   8   8B  حشو أصفار
 *
 * هيدر الصورة (48 بايت، يبدأ فوراً بعد هيدر الملف عند أوفست 16):
 *   0   4B  الحجم الكلي (مع المحاذاة)
 *   4   4B  حجم بيانات CLUT
 *   8   4B  حجم بيانات البكسل (+ mipmaps لو وُجدت)
 *   12  2B  حجم الهيدر (= 48 دائماً بهذي الأداة)
 *   14  2B  عدد ألوان CLUT
 *   16  1B  عدد مستويات Mipmap
 *   17  1B  نوع CLUT
 *   18  1B  نوع الصورة (img_type - إعلامي بس، PSM الحقيقي من GsTex0 تحت)
 *   19  1B  محجوز
 *   20  2B  العرض
 *   22  2B  الارتفاع
 *   24  8B  GsTex0 (تسجيل GS حقيقي - PSM بالبت 20-25، ClutPSM بالبت 51-54)
 *   32  8B  GsTex1 (فلترة - 0 = Nearest)
 *   40  4B  GsTexClut
 *   44  4B  محجوز
 *
 * بعدها مباشرة: [بيانات البكسل][بيانات CLUT][حشو محاذاة]
 *
 * نقرأ PSM مباشرة من GsTex0 (نفس ما تفعله الأداة بدالتها --info:
 * psm = (gs_tex0 >> 20) & 0x3F) - مو نخمّنه من نوع الصورة، فيدعم
 * تلقائياً كل الصيغ الخمس اللي الأداة تنتجها (32/24/16-bit، وindexed
 * 8/4-bit) بلا أي جدول تحويل يدوي.
 *
 * ملاحظة CLUT: تُنسخ كما هي (بلا فك تبديل) - ترتيب CSM1 المُبدَّل
 * اللي الأداة تكتبه هو بالضبط اللي شريحة GS الحقيقية تتوقعه لقوام
 * مفهرس (فك التبديل لازم بس لنسخة المحرر اللي تعرض على شاشة عادية،
 * راجع src/nodes_editor/image_loader.c).
 * ============================================================ */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h> /* memalign */

#include <gsKit.h>

#include "image_loader.h"
#include "image_loader_internal.h"

int image_loader_load_tim2(GSGLOBAL *gsGlobal, GSTEXTURE *texture, const char *path) {
    FILE *f = fopen(path, "rb");
    if (f == NULL) return 0;

    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return 0; }
    long file_size = ftell(f);
    if (file_size < 16 + 48) { fclose(f); return 0; } /* أصغر من أصغر ملف TIM2 ممكن */
    if (fseek(f, 0, SEEK_SET) != 0) { fclose(f); return 0; }

    unsigned char *data = malloc((size_t)file_size);
    if (data == NULL) { fclose(f); return 0; }

    if (fread(data, 1, (size_t)file_size, f) != (size_t)file_size) {
        free(data);
        fclose(f);
        return 0;
    }
    fclose(f);

    if (memcmp(data, "TIM2", 4) != 0) {
        free(data);
        return 0;
    }

    const size_t PIC_OFFSET = 16; /* هيدر الملف ثابت 16 بايت، أول صورة تبدأ فوراً بعده */

    unsigned int clut_size    = read_u32le(data + PIC_OFFSET + 4);
    unsigned int img_size     = read_u32le(data + PIC_OFFSET + 8);
    unsigned short hdr_size   = read_u16le(data + PIC_OFFSET + 12);
    unsigned short width      = read_u16le(data + PIC_OFFSET + 20);
    unsigned short height     = read_u16le(data + PIC_OFFSET + 22);
    unsigned long long gs_tex0 = read_u64le(data + PIC_OFFSET + 24);

    unsigned char psm  = (unsigned char)((gs_tex0 >> 20) & 0x3F);
    unsigned char cpsm = (unsigned char)((gs_tex0 >> 51) & 0xF);

    size_t img_start = PIC_OFFSET + hdr_size;
    if (img_start + (size_t)img_size + (size_t)clut_size > (size_t)file_size) {
        free(data); /* ملف تالف أو مقطوع - حجم البيانات المعلَن أكبر من الملف الفعلي */
        return 0;
    }

    texture->Width  = width;
    texture->Height = height;
    texture->PSM    = psm;
    texture->Filter = GS_FILTER_NEAREST;

    texture->Mem = memalign(128, img_size);
    if (texture->Mem == NULL) {
        free(data);
        return 0;
    }
    memcpy(texture->Mem, data + img_start, img_size);

    if (clut_size > 0) {
        texture->ClutPSM = cpsm;
        texture->Clut = memalign(128, clut_size);
        if (texture->Clut == NULL) {
            free(texture->Mem);
            texture->Mem = NULL;
            free(data);
            return 0;
        }
        memcpy(texture->Clut, data + img_start + img_size, clut_size);
    } else {
        texture->Clut = NULL;
        texture->ClutPSM = 0;
    }

    free(data);

    return finish_texture_upload(gsGlobal, texture);
}
