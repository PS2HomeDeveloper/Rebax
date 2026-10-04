/* Embedded build/runtime resource declarations. */
#ifndef EMBEDDED_RESOURCES_H
#define EMBEDDED_RESOURCES_H

#include <stddef.h>

extern const unsigned char _binary_embedded_toolchains_make_start[];
extern const unsigned char _binary_embedded_toolchains_make_end[];
extern const unsigned char _binary_embedded_ps2_toolchains_ps2dev_tar_xz_start[];
extern const unsigned char _binary_embedded_ps2_toolchains_ps2dev_tar_xz_end[];
extern const unsigned char _binary_embedded_ps2_sdk_nodes_tar_xz_start[];
extern const unsigned char _binary_embedded_ps2_sdk_nodes_tar_xz_end[];
extern const unsigned char _binary_embedded_resources_fonts_SpaceGrotesk_Regular_ttf_start[];
extern const unsigned char _binary_embedded_resources_fonts_SpaceGrotesk_Regular_ttf_end[];
extern const unsigned char _binary_embedded_resources_fonts_SpaceGrotesk_Bold_ttf_start[];
extern const unsigned char _binary_embedded_resources_fonts_SpaceGrotesk_Bold_ttf_end[];

#define EMBEDDED_SIZE(symbol) ((size_t)(_binary_##symbol##_end - _binary_##symbol##_start))
static inline size_t embedded_make_size(void) { return EMBEDDED_SIZE(embedded_toolchains_make); }
static inline size_t embedded_ps2dev_archive_size(void) { return EMBEDDED_SIZE(embedded_ps2_toolchains_ps2dev_tar_xz); }
static inline size_t embedded_node_archive_size(void) { return EMBEDDED_SIZE(embedded_ps2_sdk_nodes_tar_xz); }
static inline size_t embedded_font_regular_size(void) { return EMBEDDED_SIZE(embedded_resources_fonts_SpaceGrotesk_Regular_ttf); }
static inline size_t embedded_font_bold_size(void) { return EMBEDDED_SIZE(embedded_resources_fonts_SpaceGrotesk_Bold_ttf); }
#undef EMBEDDED_SIZE

#endif
