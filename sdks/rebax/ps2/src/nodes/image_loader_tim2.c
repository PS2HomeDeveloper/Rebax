/*
 * ============================================================
 * image_loader_tim2.c
 * ============================================================
 * See image_loader.h for full documentation.
 *
 * The binary layout is taken verbatim from the reference ps2_tim2_tool.py
 * (the same tool that produces .tm2 files for your project) - see its
 * build_tim2_file and build_picture_header functions for exact offsets below:
 *
 * File header (16 bytes, first 4 bytes "TIM2"):
 *   0   4B  Magic "TIM2"
 *   4   1B  Version
 *   5   1B  Format (0 = Linear)
 *   6   2B  number of images (LE)
 *   8   8B  zero padding
 *
 * Picture header (48 bytes, starts immediately after file header at offset 16):
 *   0   4B  total size (with alignment)
 *   4   4B  CLUT data size
 *   8   4B  pixel data size (+ mipmaps if present)
 *   12  2B  header size (= 48 always for this tool)
 *   14  2B  number of CLUT colors
 *   16  1B  number of mipmap levels
 *   17  1B  CLUT type
 *   18  1B  image type (img_type - informational only, real PSM in GsTex0 below)
 *   19  1B  reserved
 *   20  2B  width
 *   22  2B  height
 *   24  8B  GsTex0 (real GS register - PSM in bits 20-25, ClutPSM in bits 51-54)
 *   32  8B  GsTex1 (filtering - 0 = Nearest)
 *   40  4B  GsTexClut
 *   44  4B  reserved
 *
 * Then immediately: [pixel data][CLUT data][alignment padding]
 *
 * We read PSM directly from GsTex0 (same as the tool's --info:
 * psm = (gs_tex0 >> 20) & 0x3F) - not guessed from image type, so this
 * automatically supports all five formats the tool emits (32/24/16-bit,
 * and indexed 8/4-bit) without any manual mapping table.
 *
 * CLUT note: copied as-is (no byte-swapping) - the CSM1-swapped ordering
 * the tool writes is exactly what a real GS hardware expects for indexed
 * textures (byte-swapping is only needed for the editor build that displays
 * them on a normal screen, see src/nodes_editor/image_loader.c).
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
    if (file_size < 16 + 48) { fclose(f); return 0; } /* Smaller than the smallest possible TIM2 file */
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

    const size_t PIC_OFFSET = 16; /* File header fixed 16 bytes, first image starts immediately after it */

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
        free(data); /* Corrupt/truncated file - declared data size larger than actual file */
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
