/*
 * ============================================================
 * image_loader_tim.c
 * ============================================================
 * راجع image_loader.h للتوثيق الكامل.
 *
 * TIM (PS1) - التخطيط مأخوذ حرفياً من ps1_tim_tool.py (دالة parse_tim
 * الرسمية بالأداة نفسها، أقوى مرجع ممكن - هي القراءة المعاكسة تماماً
 * لما تبنيه الأداة):
 *
 * هيدر الملف (8 بايت):
 *   0   4B  TIM_ID (لازم == 0x00000010)
 *   4   4B  Flag: بت 0-1 = Pixel Mode (0=4bit, 1=8bit, 2=16bit,
 *           3=24bit)، بت 3 = يوجد CLUT (قناع 0x8)
 *
 * لو يوجد CLUT، قسم CLUT فوراً بعد هيدر الملف (هيدر فرعي 12 بايت):
 *   0   4B  طول القسم كامل (يشمل هالهيدر الفرعي)
 *   4   2B  CLUT X (موضع VRAM - غير مستخدم هنا)
 *   6   2B  CLUT Y
 *   8   2B  عدد ألوان كل لوحة
 *   10  2B  عدد اللوحات
 *   ثم الألوان: uint16 لكل لون، صيغة 5551 - **نفس بت GS الحرفي**
 *   (تحققت من دالتي packing الأداتين: PS1 وPS2 يستخدمان نفس ترتيب
 *   A(1)B(5)G(5)R(5) بالضبط) - نسخ مباشر بلا أي تحويل
 *
 * قسم الصورة (فوراً بعد CLUT لو وُجد، وإلا فوراً بعد هيدر الملف):
 *   0   4B  طول القسم كامل (يشمل هالهيدر الفرعي)
 *   4   2B  Image X (غير مستخدم هنا)
 *   6   2B  Image Y
 *   8   2B  العرض بوحدة نصف-الكلمة (width_units) - **ليست بكسل!**
 *   10  2B  الارتفاع بالبكسل مباشرة
 *   ثم بيانات البكسلات الخام
 *
 * تحويل width_units لعرض حقيقي بالبكسل (دالة pixel_width_of
 * الرسمية بالأداة، حرفياً):
 *   4-bit:  width_units * 4
 *   8-bit:  width_units * 2
 *   16-bit: width_units (فردي، وحدة = بكسل)
 *   24-bit: (width_units * 2) / 3
 *
 * بيانات البكسلات نفسها (بعد معرفة العرض الحقيقي): نسخ مباشر بلا
 * أي تحويل بتات لكل الصيغ الأربع - تحققت بمقارنة دوال البناء
 * الحقيقية بالأداة: الفهرسة (4/8-بت) بنفس ترتيب nibble/byte
 * المستخدم بـPS2 GS بالضبط، والمباشرة (16/24-بت) بايتات خام بسيطة
 * بلا أي تعقيد يستدعي تحويلاً.
 *
 * تحويل صيغة PS1 → GS المقابلة:
 *   4-bit  → GS_PSM_T4  (0x14) + CLUT بصيغة GS_PSM_CT16
 *   8-bit  → GS_PSM_T8  (0x13) + CLUT بصيغة GS_PSM_CT16
 *   16-bit → GS_PSM_CT16 (0x02) بلا CLUT
 *   24-bit → GS_PSM_CT24 (0x01) بلا CLUT
 * ============================================================ */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h> /* memalign */

#include <gsKit.h>

#include "image_loader.h"
#include "image_loader_internal.h"

#define TIM_ID              0x00000010
#define TIM_CF_CLUT_PRESENT 0x8
#define TIM_PMODE_4BIT      0
#define TIM_PMODE_8BIT      1
#define TIM_PMODE_16BIT     2
#define TIM_PMODE_24BIT     3

int image_loader_load_tim(GSGLOBAL *gsGlobal, GSTEXTURE *texture, const char *path) {
    FILE *f = fopen(path, "rb");
    if (f == NULL) return 0;

    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return 0; }
    long file_size = ftell(f);
    if (file_size < 8 + 12) { fclose(f); return 0; } /* أصغر من أصغر ملف TIM ممكن (هيدر + قسم صورة فاضي) */
    if (fseek(f, 0, SEEK_SET) != 0) { fclose(f); return 0; }

    unsigned char *data = malloc((size_t)file_size);
    if (data == NULL) { fclose(f); return 0; }

    if (fread(data, 1, (size_t)file_size, f) != (size_t)file_size) {
        free(data);
        fclose(f);
        return 0;
    }
    fclose(f);

    unsigned int tim_id = read_u32le(data + 0);
    unsigned int flag   = read_u32le(data + 4);
    if (tim_id != TIM_ID) { free(data); return 0; }

    int pmode    = (int)(flag & 0x3);
    int has_clut = (flag & TIM_CF_CLUT_PRESENT) != 0;

    size_t off = 8;
    unsigned int clut_colors_count = 0;
    unsigned char *clut_colors_ptr = NULL;

    if (has_clut) {
        if (off + 12 > (size_t)file_size) { free(data); return 0; }
        unsigned int clut_len = read_u32le(data + off);
        unsigned short clut_w = read_u16le(data + off + 8);
        unsigned short clut_h = read_u16le(data + off + 10);
        clut_colors_count = (unsigned int)clut_w * (unsigned int)clut_h;
        clut_colors_ptr = data + off + 12;
        if (off + clut_len > (size_t)file_size) { free(data); return 0; }
        off += clut_len;
    }

    if (off + 12 > (size_t)file_size) { free(data); return 0; }
    unsigned int img_len       = read_u32le(data + off);
    unsigned short width_units = read_u16le(data + off + 8);
    unsigned short height      = read_u16le(data + off + 10);
    unsigned char *img_data_ptr = data + off + 12;
    if (off + img_len > (size_t)file_size) { free(data); return 0; }

    int width;
    unsigned char psm;
    switch (pmode) {
        case TIM_PMODE_4BIT:  width = width_units * 4; psm = GS_PSM_T4;  break;
        case TIM_PMODE_8BIT:  width = width_units * 2; psm = GS_PSM_T8;  break;
        case TIM_PMODE_16BIT: width = width_units;     psm = GS_PSM_CT16; break;
        case TIM_PMODE_24BIT: width = (width_units * 2) / 3; psm = GS_PSM_CT24; break;
        default: free(data); return 0;
    }

    size_t img_data_size = (size_t)img_len - 12;

    texture->Width  = width;
    texture->Height = height;
    texture->PSM    = psm;
    texture->Filter = GS_FILTER_NEAREST;

    texture->Mem = memalign(128, img_data_size);
    if (texture->Mem == NULL) { free(data); return 0; }
    memcpy(texture->Mem, img_data_ptr, img_data_size);

    if (has_clut && clut_colors_count > 0) {
        size_t clut_size = (size_t)clut_colors_count * 2; /* uint16 لكل لون */
        texture->ClutPSM = GS_PSM_CT16; /* نفس بت 5551 الحرفي - راجع تعليق أعلى */
        texture->Clut = memalign(128, clut_size);
        if (texture->Clut == NULL) {
            free(texture->Mem);
            texture->Mem = NULL;
            free(data);
            return 0;
        }
        memcpy(texture->Clut, clut_colors_ptr, clut_size);
    } else {
        texture->Clut = NULL;
        texture->ClutPSM = 0;
    }

    free(data);

    return finish_texture_upload(gsGlobal, texture);
}
