/*
 * ============================================================
 * font.c
 * ============================================================
 * Loads the bundled Space Grotesk fonts via stb_truetype, and renders any
 * requested text as an RGBA pixel image (text is white; the fourth channel
 * represents coverage/alpha - ready to be colored to any color at draw time).
 *
 * Note: currently supports only Latin text/numbers (ASCII) - support for
 * Arabic or other languages requires deeper UTF-8 handling and will be
 * added later if needed.
 * ============================================================
 */

#include <stdlib.h>
#include <string.h>

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

#include "font.h"

#ifdef __ANDROID__
#include "asset_files.h"
static unsigned char *g_font_regular_data;
static unsigned char *g_font_bold_data;
#define FONT_REGULAR_DATA g_font_regular_data
#define FONT_BOLD_DATA g_font_bold_data
#else
#include "embedded_resources.h"
#define FONT_REGULAR_DATA _binary_embedded_resources_fonts_SpaceGrotesk_Regular_ttf_start
#define FONT_BOLD_DATA _binary_embedded_resources_fonts_SpaceGrotesk_Bold_ttf_start
#endif

static stbtt_fontinfo g_font_regular;
static stbtt_fontinfo g_font_bold;
static int g_initialized = 0;

int font_init(void) {
    if (g_initialized) {
        return 1;
    }

#ifdef __ANDROID__
    if (!g_font_regular_data) g_font_regular_data = asset_file_read("resources/fonts/SpaceGrotesk-Regular.ttf", NULL);
    if (!g_font_bold_data) g_font_bold_data = asset_file_read("resources/fonts/SpaceGrotesk-Bold.ttf", NULL);
    if (!g_font_regular_data || !g_font_bold_data) {
        return 0;
    }
#endif

    int ok_regular = stbtt_InitFont(
        &g_font_regular,
        FONT_REGULAR_DATA,
        stbtt_GetFontOffsetForIndex(FONT_REGULAR_DATA, 0)
    );
    int ok_bold = stbtt_InitFont(
        &g_font_bold,
        FONT_BOLD_DATA,
        stbtt_GetFontOffsetForIndex(FONT_BOLD_DATA, 0)
    );

    if (!ok_regular || !ok_bold) {
        return 0;
    }

    g_initialized = 1;
    return 1;
}

static font_text_image_t font_render_text_internal(const char *text, font_weight_t weight,
                                                    int pixel_size,
                                                    const unsigned char *rgb_by_byte) {
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

    /* First pass: compute total width by summing each glyph advance + kerning between pairs */
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

    /* Second pass: actually draw each glyph at its position into the final image */
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
                    dst[0] = rgb_by_byte ? rgb_by_byte[i * 3] : 255;
                    dst[1] = rgb_by_byte ? rgb_by_byte[i * 3 + 1] : 255;
                    dst[2] = rgb_by_byte ? rgb_by_byte[i * 3 + 2] : 255;
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

font_text_image_t font_render_text(const char *text, font_weight_t weight, int pixel_size) {
    return font_render_text_internal(text, weight, pixel_size, NULL);
}

font_text_image_t font_render_text_colored(const char *text, font_weight_t weight,
                                            int pixel_size, const unsigned char *rgb_by_byte) {
    return font_render_text_internal(text, weight, pixel_size, rgb_by_byte);
}

void font_free_text_image(font_text_image_t *img) {
    if (img != NULL && img->pixels != NULL) {
        free(img->pixels);
        img->pixels = NULL;
        img->width = img->height = 0;
    }
}

void font_shutdown(void) {
    /* No need to free any memory here - the font data are direct pointers into
     * the executable's memory (embedded via objcopy), and not memory
     * dynamically allocated by stb_truetype */
    g_initialized = 0;
}
