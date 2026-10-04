/*
 * ============================================================
 * image_loader.h
 * ============================================================
 * Single shared image-loading unit for any PS2 node that needs to load
 * and display an image via gsKit - Sprite2D currently, and any future
 * node (image-button, animated background...) will call the same
 * functions without duplicating format-detection logic in each node
 * file.
 *
 * Supported formats: PNG, JPEG/JPG, BMP, TGA, TIFF/TIF (via the
 * provided gsKit), RAW (no header - dimensions/format supplied by the
 * caller), TIM2/TM2 and TIM (complete manual decoding built on two real
 * references: ps2_tim2_tool.py and ps1_tim_tool.py - the same tools
 * that generate these files for your project).
 * Not supported at all: GIF (a naming trap - unrelated to the GS
 * libraries inside the PS2's processor).
 *
 * ------------------------------------------------------------
 * Why a separate function per format (image_loader_load_png,
 * _jpeg, _tim2...) instead of a single dispatcher? - Important
 * exporter detail, not an implementation style point:
 *
 * During the actual export (upcoming step), the goal is to produce a
 * PS2 executable that contains no loading logic for formats the project
 * doesn't use at all - if the developer used only PNG and TIM2, the
 * JPEG/BMP/TGA/TIFF/RAW logic must not appear in the final binary.
 *
 * This is achieved by: compiling src/nodes with -ffunction-sections
 * during export (each function in its own .o section), then the
 * generated exporter code (upcoming step, it examines the actual
 * "Image Path" extension of each node's properties in the exported
 * scenes) invokes the specific function directly (e.g.
 * image_loader_load_png only for .png files), instead of the generic
 * dispatcher image_loader_load. If we always called the generic
 * dispatcher, its compiled code contains concrete calls to all
 * functions (all if/else branches), becoming a reference for every
 * section - so --gc-sections (remove unused sections) cannot eliminate
 * them, because the call exists in the compiled code regardless of
 * which branch runs at runtime. The generic dispatcher below remains
 * useful for non-export uses (local testing, dev tools) - but the
 * actual exporter must bypass it.
 * ============================================================
 */

#ifndef IMAGE_LOADER_H
#define IMAGE_LOADER_H

#include <gsKit.h>

int image_loader_load_png(GSGLOBAL *gsGlobal, GSTEXTURE *texture, const char *path);
int image_loader_load_jpeg(GSGLOBAL *gsGlobal, GSTEXTURE *texture, const char *path);
int image_loader_load_bmp(GSGLOBAL *gsGlobal, GSTEXTURE *texture, const char *path);
int image_loader_load_tga(GSGLOBAL *gsGlobal, GSTEXTURE *texture, const char *path);
int image_loader_load_tiff(GSGLOBAL *gsGlobal, GSTEXTURE *texture, const char *path);

/* No header at all - width/height/psm must be known by the caller from
 * elsewhere (node properties, for example), exactly like the official
 * gsKit example (examples/textures/textures.c) */
int image_loader_load_raw(GSGLOBAL *gsGlobal, GSTEXTURE *texture, const char *path,
                           int width, int height, int psm);

/* Full manual decoding - the binary layout is taken verbatim from
 * ps2_tim2_tool.py (the build_tim2_file and build_picture_block
 * functions) - it supports all five formats the tool produces
 * (direct 32/24/16-bit, and indexed 8/4-bit) automatically, because
 * the PSM is read from the GsTex0 stored in the file itself, not
 * hardcoded */
int image_loader_load_tim2(GSGLOBAL *gsGlobal, GSTEXTURE *texture, const char *path);

/* Full manual decoding of the PS1 TIM format - the layout is taken
 * verbatim from ps1_tim_tool.py (the tool's parse_tim function and the
 * four conversion builders convert_4bit/8bit/16bit/24bit). The four
 * formats (4/8/16/24-bit) are mapped to the corresponding GS formats
 * (T4/T8/CT16/CT24) by direct byte copying with no bit manipulation -
 * verified by comparing the actual encoding routines in both tools
 * literally: the 5551 format and the nibble indexing order are
 * identical between PS1 and PS2 GS, and the direct formats (16/24-bit)
 * are plain raw bytes. */
int image_loader_load_tim(GSGLOBAL *gsGlobal, GSTEXTURE *texture, const char *path);

/* The generic dispatcher function - it detects the format from the
 * file extension and calls the appropriate function above. **The
 * actual exporter does not use this** - see the explanation above. It
 * remains useful for any non-export usage (local testing, tools) */
int image_loader_load(GSGLOBAL *gsGlobal, GSTEXTURE *texture, const char *path,
                       int raw_width, int raw_height, int raw_psm);

#endif /* IMAGE_LOADER_H */
