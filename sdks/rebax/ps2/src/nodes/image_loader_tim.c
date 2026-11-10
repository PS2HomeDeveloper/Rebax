/*
 * ============================================================
 * image_loader_tim.c
 * ============================================================
 * See image_loader.h for full documentation.
 *
 * TIM (PS1) - layout taken verbatim from ps1_tim_tool.py (its parse_tim
 * function is the authoritative reference - it is the exact inverse
 * of what the tool writes):
 *
 * File header (8 bytes):
 *   0   4B  TIM_ID (must == 0x00000010)
 *   4   4B  Flag: bits 0-1 = Pixel Mode (0=4bit, 1=8bit, 2=16bit,
 *           3=24bit), bit 3 = has CLUT (mask 0x8)
 *
 * If a CLUT exists, the CLUT section follows immediately after the file header
 * (sub-header 12 bytes):
 *   0   4B  full section length (includes this sub-header)
 *   4   2B  CLUT X (VRAM position - unused here)
 *   6   2B  CLUT Y
 *   8   2B  colors per palette
 *   10  2B  number of palettes
 *   then the colors: uint16 per color, 5551 format - **same literal GS bits**
 *   (confirmed by checking the packing functions of both tools: PS1 and PS2
 *   use exactly the same A(1)B(5)G(5)R(5) ordering) - direct copy, no conversion
 *
 * Image section (immediately after CLUT if present, otherwise right after header):
 *   0   4B  full section length (includes this sub-header)
 *   4   2B  Image X (unused here)
 *   6   2B  Image Y
 *   8   2B  width in half-word units (width_units) - **not pixels!**
 *   10  2B  height in pixels directly
 *   then raw pixel data
 *
 * Converting width_units to actual pixel width (official pixel_width_of
 * function from the tool, verbatim):
 *   4-bit:  width_units * 4
 *   8-bit:  width_units * 2
 *   16-bit: width_units (odd, unit = pixels)
 *   24-bit: (width_units * 2) / 3
 *
 * The pixel data itself (after real width is known): direct copy with
 * no bit conversion for all four formats - verified by comparing the
 * tool's real build functions: indexed (4/8-bit) use the exact nibble/byte
 * ordering the PS2 GS expects, and direct (16/24-bit) are plain raw bytes
 * with no conversion complexity.
 *
 * PS1 → GS format mapping:
 *   4-bit  → GS_PSM_T4  (0x14) + CLUT as GS_PSM_CT16
 *   8-bit  → GS_PSM_T8  (0x13) + CLUT as GS_PSM_CT16
 *   16-bit → GS_PSM_CT16 (0x02) no CLUT
 *   24-bit → GS_PSM_CT24 (0x01) no CLUT
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
    if (file_size < 8 + 12) { fclose(f); return 0; } /* Smaller than the smallest possible TIM file (header + empty image section) */
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
        size_t clut_size = (size_t)clut_colors_count * 2; /* uint16 per color */
        texture->ClutPSM = GS_PSM_CT16; /* Same literal 5551 bit layout - see comment above */
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
