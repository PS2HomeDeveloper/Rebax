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
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <stdlib.h>
#include <string.h>

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

#include "editor/ui/font.h"
#include "editor/ui/text_shaping.h"

#include "filesystem/resources.h"

enum { FACE_REGULAR, FACE_BOLD, FACE_MONO, FACE_ARABIC, FACE_ARABIC_BOLD, FACE_COUNT };

typedef struct {
    unsigned char *data;
    stbtt_fontinfo info;
    int ok;
} face_t;

static const char *const g_face_files[FACE_COUNT] = {
    "fonts/SpaceGrotesk-Regular.ttf",
    "fonts/SpaceGrotesk-Bold.ttf",
    "fonts/JetBrainsMono-Regular.ttf",
    "fonts/NotoSansArabic-Regular.ttf",
    "fonts/NotoSansArabic-Bold.ttf"
};

static face_t g_faces[FACE_COUNT];
static int g_initialized = 0;

int font_init(void) {
    if (g_initialized) return 1;
    for (int i = 0; i < FACE_COUNT; i++) {
        if (g_faces[i].ok) continue;
        if (!g_faces[i].data) g_faces[i].data = resource_file_read(g_face_files[i], NULL);
        if (g_faces[i].data &&
            stbtt_InitFont(&g_faces[i].info, g_faces[i].data, stbtt_GetFontOffsetForIndex(g_faces[i].data, 0))) {
            g_faces[i].ok = 1;
        }
    }
    if (!g_faces[FACE_REGULAR].ok || !g_faces[FACE_BOLD].ok) return 0;
    g_initialized = 1;
    return 1;
}

#define FONT_SIZE_BOOST 4
static float g_scale = 1.0f;
void font_set_scale(float scale) { g_scale = scale < 0.5f ? 0.5f : scale; }
static int scaled_px(int pixel_size) { return (int)((pixel_size + FONT_SIZE_BOOST) * g_scale + 0.5f); }

static int primary_face(font_weight_t weight) {
    if (weight == FONT_WEIGHT_BOLD) return FACE_BOLD;
    if (weight == FONT_WEIGHT_MONO && g_faces[FACE_MONO].ok) return FACE_MONO;
    return FACE_REGULAR;
}

static int fallback_face(font_weight_t weight) {
    int f = weight == FONT_WEIGHT_BOLD ? FACE_ARABIC_BOLD : FACE_ARABIC;
    if (!g_faces[f].ok) f = FACE_ARABIC;
    return g_faces[f].ok ? f : -1;
}

typedef struct {
    int face;
    int glyph;
    int src;
    int advance;
    int kern;
} glyph_t;

typedef struct {
    glyph_t *glyphs;
    int count;
    int width;
    int ascent;
    int descent;
    int fallback_used;
} layout_t;

static void layout_free(layout_t *l) {
    free(l->glyphs);
    l->glyphs = NULL;
    l->count = 0;
}

static int layout_build(const char *text, font_weight_t weight, int pixel_size, layout_t *out) {
    memset(out, 0, sizeof(*out));
    int len = (int)strlen(text);
    text_cp_t stack_cps[256];
    text_cp_t *cps = len <= 256 ? stack_cps : (text_cp_t *)malloc((size_t)len * sizeof(text_cp_t));
    if (cps == NULL) return 0;
    int n = text_decode_utf8(text, len, cps);
    if (text_has_rtl(cps, n)) n = text_shape_and_reorder(cps, n);

    int pf = primary_face(weight);
    int ff = fallback_face(weight);
    stbtt_fontinfo *pinfo = &g_faces[pf].info;
    float pscale = stbtt_ScaleForPixelHeight(pinfo, (float)pixel_size);
    float em_px = pscale / stbtt_ScaleForMappingEmToPixels(pinfo, 1.0f);
    float fscale = ff >= 0 ? stbtt_ScaleForMappingEmToPixels(&g_faces[ff].info, em_px) : 0.0f;

    out->glyphs = (glyph_t *)calloc((size_t)(n > 0 ? n : 1), sizeof(glyph_t));
    if (out->glyphs == NULL) { if (cps != stack_cps) free(cps); return 0; }

    int pa, pd, pg;
    stbtt_GetFontVMetrics(pinfo, &pa, &pd, &pg);
    out->ascent = (int)(pa * pscale);
    out->descent = (int)(-pd * pscale);

    for (int i = 0; i < n; i++) {
        int cp = cps[i].cp;
        int face = pf;
        int g = stbtt_FindGlyphIndex(pinfo, cp);
        if (g == 0 && ff >= 0 && cp >= 0x80) {
            int g2 = stbtt_FindGlyphIndex(&g_faces[ff].info, cp);
            if (g2 != 0) { face = ff; g = g2; }
        }
        float sc = face == pf ? pscale : fscale;
        int adv, lsb;
        stbtt_GetGlyphHMetrics(&g_faces[face].info, g, &adv, &lsb);
        out->glyphs[i].face = face;
        out->glyphs[i].glyph = g;
        out->glyphs[i].src = cps[i].src;
        out->glyphs[i].advance = (int)(adv * sc);
        if (face != pf) {
            int a, d, lg;
            stbtt_GetFontVMetrics(&g_faces[face].info, &a, &d, &lg);
            if ((int)(a * fscale) > out->ascent) out->ascent = (int)(a * fscale);
            if ((int)(-d * fscale) > out->descent) out->descent = (int)(-d * fscale);
            out->fallback_used = 1;
        }
        if (i > 0 && out->glyphs[i - 1].face == face && face == pf && cp < 0x0590) {
            out->glyphs[i - 1].kern = (int)(stbtt_GetGlyphKernAdvance(pinfo, out->glyphs[i - 1].glyph, g) * pscale);
        }
        out->width += out->glyphs[i].advance + out->glyphs[i].kern;
    }
    out->count = n;
    if (cps != stack_cps) free(cps);
    return 1;
}

static float face_scale(const layout_t *l, int face, int pf, float pscale, float fscale) {
    (void)l;
    return face == pf ? pscale : fscale;
}

static font_text_image_t font_render_text_internal(const char *text, font_weight_t weight,
                                                    int pixel_size,
                                                    const unsigned char *rgb_by_byte) {
    font_text_image_t out = {0};
    if (!g_initialized || text == NULL || pixel_size <= 0) return out;
    pixel_size = scaled_px(pixel_size);

    layout_t layout;
    if (!layout_build(text, weight, pixel_size, &layout)) return out;

    int pf = primary_face(weight);
    int ff = fallback_face(weight);
    stbtt_fontinfo *pinfo = &g_faces[pf].info;
    float pscale = stbtt_ScaleForPixelHeight(pinfo, (float)pixel_size);
    float em_px = pscale / stbtt_ScaleForMappingEmToPixels(pinfo, 1.0f);
    float fscale = ff >= 0 ? stbtt_ScaleForMappingEmToPixels(&g_faces[ff].info, em_px) : 0.0f;

    int baseline, total_height;
    if (layout.fallback_used) {
        baseline = layout.ascent;
        total_height = layout.ascent + layout.descent + 1;
    } else {
        int a, d, g;
        stbtt_GetFontVMetrics(pinfo, &a, &d, &g);
        baseline = (int)(a * pscale);
        total_height = (int)((a - d) * pscale) + 1;
    }
    int total_width = layout.width > 0 ? layout.width : 1;

    out.width = total_width;
    out.height = total_height;
    out.pixels = (unsigned char *)calloc((size_t)out.width * (size_t)out.height * 4, 1);
    if (out.pixels == NULL) {
        out.width = out.height = 0;
        layout_free(&layout);
        return out;
    }

    int pen_x = 0;
    for (int i = 0; i < layout.count; i++) {
        glyph_t *g = &layout.glyphs[i];
        float sc = face_scale(&layout, g->face, pf, pscale, fscale);
        int gw, gh, gx, gy;
        unsigned char *bitmap = stbtt_GetGlyphBitmap(&g_faces[g->face].info, sc, sc, g->glyph, &gw, &gh, &gx, &gy);
        if (bitmap != NULL) {
            int draw_x = pen_x + gx;
            int draw_y = baseline + gy;
            for (int y = 0; y < gh; y++) {
                for (int x = 0; x < gw; x++) {
                    int px = draw_x + x;
                    int py = draw_y + y;
                    if (px < 0 || px >= out.width || py < 0 || py >= out.height) continue;
                    unsigned char alpha = bitmap[y * gw + x];
                    unsigned char *dst = &out.pixels[(py * out.width + px) * 4];
                    if (alpha < dst[3]) continue;
                    dst[0] = rgb_by_byte ? rgb_by_byte[g->src * 3] : 255;
                    dst[1] = rgb_by_byte ? rgb_by_byte[g->src * 3 + 1] : 255;
                    dst[2] = rgb_by_byte ? rgb_by_byte[g->src * 3 + 2] : 255;
                    dst[3] = alpha;
                }
            }
            stbtt_FreeBitmap(bitmap, NULL);
        }
        pen_x += g->advance + g->kern;
    }
    layout_free(&layout);

    out.px_w = out.width;
    out.px_h = out.height;
    out.width = (int)(out.px_w / g_scale + 0.999f);
    out.height = (int)(out.px_h / g_scale + 0.999f);
    return out;
}

font_text_image_t font_render_text(const char *text, font_weight_t weight, int pixel_size) {
    return font_render_text_internal(text, weight, pixel_size, NULL);
}

font_text_image_t font_render_text_colored(const char *text, font_weight_t weight,
                                            int pixel_size, const unsigned char *rgb_by_byte) {
    return font_render_text_internal(text, weight, pixel_size, rgb_by_byte);
}

int font_measure_text(const char *text, font_weight_t weight, int pixel_size) {
    if (!g_initialized || text == NULL || pixel_size <= 0) return 0;
    layout_t layout;
    if (!layout_build(text, weight, scaled_px(pixel_size), &layout)) return 0;
    int total = layout.width;
    layout_free(&layout);
    return (int)(total / g_scale + 0.5f);
}

void font_free_text_image(font_text_image_t *img) {
    if (img != NULL && img->pixels != NULL) {
        free(img->pixels);
        img->pixels = NULL;
        img->width = img->height = 0;
    }
}

void font_shutdown(void) {
    g_initialized = 0;
}
