/*
 * ============================================================
 * image_loader.h (nodes_editor)
 * ============================================================
 * Shared image loading unit for the editor itself - completely opposite
 * to its counterpart at src/nodes/image_loader.h (that uses gsKit, this
 * uses stb_image + manual decoding of original PS2/PS1 formats). Any
 * editor node that needs an image preview (Sprite2D currently, and any
 * future node) calls the same function.
 *
 * Supported formats: PNG/JPEG/BMP/TGA/TIFF (via stb_image),
 * TIM2/TM2 and TIM (manual decoding - the same real references used in
 * the PS2 version: ps2_tim2_tool.py and ps1_tim_tool.py), RAW (only
 * direct color formats - 32/24/16/16S bit; indexed RAW 8/4-bit are not
 * supported here because RAW files don't include a CLUT section - no
 * color source to decode from. Indexed TIM2/TIM are fully supported
 * because they carry the CLUT inside the file). Not supported at all:
 * GIF (name trap - see previous discussion).
 * ============================================================
 */

#ifndef IMAGE_LOADER_EDITOR_H
#define IMAGE_LOADER_EDITOR_H

/* Loads any supported format and returns raw RGBA8888 pixels (4 bytes per pixel,
 * row by row from the top) ready to pass directly to window_create_texture - pointer
 * allocated with malloc, caller is responsible for freeing it (regular free). NULL on
 * failure or unsupported format.
 *
 * raw_width/raw_height/raw_psm are used only when the file extension
 * \".raw\" (same GS_PSM_* constants used in the PS2 version - GS_PSM_32=0,
 * GS_PSM_24=1, GS_PSM_16=2, GS_PSM_16S=0x0A - see header comment
 * above for why indexed 8/4-bit are not supported for RAW
 * specifically) */
unsigned char *image_loader_editor_load(const char *path, int *out_width, int *out_height,
                                         int raw_width, int raw_height, int raw_psm);

#endif /* IMAGE_LOADER_EDITOR_H */
