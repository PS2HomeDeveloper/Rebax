/*
 * ============================================================
 * image_loader_internal.h
 * ============================================================
 * Helper functions shared by some of the image_loader_*.c files only -
 * not part of the public interface (image_loader.h), and not something
 * any node needs to know. They're split here (instead of duplicating
 * them in every format file) because TIM2 and TIM both need them
 * exactly: reading little-endian integers from raw bytes, and
 * finishing uploading a manually-decoded texture to GS memory.
 * ============================================================
 */

#ifndef IMAGE_LOADER_INTERNAL_H
#define IMAGE_LOADER_INTERNAL_H

#include <gsKit.h>

/* Reads a little-endian integer from raw bytes - the same byte order
 * used in TIM2/TIM files (and any other native PS2/PS1 format in the
 * future) */
unsigned int read_u32le(const unsigned char *p);
unsigned short read_u16le(const unsigned char *p);
unsigned long long read_u64le(const unsigned char *p);

/* Actually completes uploading a manually-decoded texture (TIM2/TIM)
 * to GS memory - the same literal sequence that gsKit does internally
 * (gsKit_texture_finish in the original gsToolkit.c), but implemented
 * here ourselves in terms of only the confirmed public functions
 * (gsKit_vram_alloc, gsKit_texture_size, gsKit_texture_upload - all
 * declared in the public gsTexture.h/gsCore.h). We intentionally do
 * not call gsKit_texture_finish itself: that is an internal function in
 * the gsKit library (extern declared inside gsToolkit.c itself, with no
 * public header declaration) - although it does exist in the built
 * library (verified in CMakeLists.txt), relying on it directly would
 * tie our code to an internal implementation detail that is not part
 * of gsKit's stable public interface. */
int finish_texture_upload(GSGLOBAL *gsGlobal, GSTEXTURE *texture);

#endif /* IMAGE_LOADER_INTERNAL_H */
