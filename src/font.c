/*
 * ============================================================
 * font.c
 * ============================================================
 * يحمّل خطي Space Grotesk المضمّنين عبر stb_truetype، ويرسم أي
 * نص مطلوب كصورة بكسلات RGBA (النص أبيض، القناة الرابعة تمثّل
 * درجة التغطية/الشفافية - جاهزة لتلوينها بأي لون وقت الرسم لاحقاً).
 *
 * ملاحظة: يدعم حالياً النصوص اللاتينية/الأرقام (ASCII) فقط - دعم
 * العربية أو أي لغة أخرى يحتاج تعامل أعمق مع ترميز UTF-8، يُضاف
 * لاحقاً عند الحاجة.
 * ============================================================
 */

#include <stdlib.h>
#include <string.h>

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

#include "embedded_resources.h"
#include "font.h"

static stbtt_fontinfo g_font_regular;
static stbtt_fontinfo g_font_bold;
static int g_initialized = 0;

int font_init(void) {
    if (g_initialized) {
        return 1;
    }

    int ok_regular = stbtt_InitFont(
        &g_font_regular,
        _binary_embedded_fonts_SpaceGrotesk_Regular_ttf_start,
        stbtt_GetFontOffsetForIndex(_binary_embedded_fonts_SpaceGrotesk_Regular_ttf_start, 0)
    );
    int ok_bold = stbtt_InitFont(
        &g_font_bold,
        _binary_embedded_fonts_SpaceGrotesk_Bold_ttf_start,
        stbtt_GetFontOffsetForIndex(_binary_embedded_fonts_SpaceGrotesk_Bold_ttf_start, 0)
    );

    if (!ok_regular || !ok_bold) {
        return 0;
    }

    g_initialized = 1;
    return 1;
}

font_text_image_t font_render_text(const char *text, font_weight_t weight, int pixel_size) {
    font_text_image_t out = {0};

    if (!g_initialized || text == NULL || pixel_size <= 0) {
        return out;
    }

    stbtt_fontinfo *font = (weight == FONT_WEIGHT_BOLD) ? &g_font_bold : &g_font_regular;
    float scale = stbtt_ScaleForPixelHeight(font, (float)pixel_size);

    int ascent, descent, line_gap;
    stbtt_GetFontVMetrics(font, &ascent, &descent, &line_gap);
    (void)line_gap;
    int baseline = (int)(ascent * scale);
    int total_height = (int)((ascent - descent) * scale) + 1;

    int len = (int)strlen(text);

    /* التمريرة الأولى: نحسب العرض الكلي بجمع مسافة كل حرف + الحشو
     * بين الأزواج (kerning) */
    int total_width = 0;
    for (int i = 0; i < len; i++) {
        int advance, lsb;
        stbtt_GetCodepointHMetrics(font, (unsigned char)text[i], &advance, &lsb);
        total_width += (int)(advance * scale);
        if (i + 1 < len) {
            total_width += (int)(stbtt_GetCodepointKernAdvance(
                font, (unsigned char)text[i], (unsigned char)text[i + 1]) * scale);
        }
    }
    if (total_width <= 0) {
        total_width = 1;
    }

    out.width  = total_width;
    out.height = total_height;
    out.pixels = (unsigned char *)calloc((size_t)out.width * (size_t)out.height * 4, 1);
    if (out.pixels == NULL) {
        out.width = out.height = 0;
        return out;
    }

    /* التمريرة الثانية: نرسم كل حرف فعلياً في مكانه من الصورة النهائية */
    int pen_x = 0;
    for (int i = 0; i < len; i++) {
        int advance, lsb;
        int gw, gh, gx, gy;
        unsigned char *bitmap = stbtt_GetCodepointBitmap(
            font, 0, scale, (unsigned char)text[i], &gw, &gh, &gx, &gy
        );

        stbtt_GetCodepointHMetrics(font, (unsigned char)text[i], &advance, &lsb);

        if (bitmap != NULL) {
            int draw_x = pen_x + gx;
            int draw_y = baseline + gy;
            for (int y = 0; y < gh; y++) {
                for (int x = 0; x < gw; x++) {
                    int px = draw_x + x;
                    int py = draw_y + y;
                    if (px < 0 || px >= out.width || py < 0 || py >= out.height) {
                        continue;
                    }
                    unsigned char alpha = bitmap[y * gw + x];
                    unsigned char *dst = &out.pixels[(py * out.width + px) * 4];
                    dst[0] = 255;
                    dst[1] = 255;
                    dst[2] = 255;
                    dst[3] = alpha;
                }
            }
            stbtt_FreeBitmap(bitmap, NULL);
        }

        pen_x += (int)(advance * scale);
        if (i + 1 < len) {
            pen_x += (int)(stbtt_GetCodepointKernAdvance(
                font, (unsigned char)text[i], (unsigned char)text[i + 1]) * scale);
        }
    }

    return out;
}

void font_free_text_image(font_text_image_t *img) {
    if (img != NULL && img->pixels != NULL) {
        free(img->pixels);
        img->pixels = NULL;
        img->width = img->height = 0;
    }
}

void font_shutdown(void) {
    /* لا حاجة لتحرير أي ذاكرة هنا - بيانات الخطوط مؤشرات مباشرة
     * لذاكرة الملف التنفيذي نفسه (مضمّنة عبر objcopy)، وليست
     * ذاكرة مخصصة ديناميكياً بواسطة stb_truetype */
    g_initialized = 0;
}
