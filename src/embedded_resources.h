/*
 * ============================================================
 * embedded_resources.h
 * ============================================================
 * Declarations for resources embedded in the executable.
 * ============================================================
 */

#ifndef EMBEDDED_RESOURCES_H
#define EMBEDDED_RESOURCES_H

#include <stddef.h>

#ifndef REBAX_EXPORT_TEMPLATE_EMBEDDED
#define REBAX_EXPORT_TEMPLATE_EMBEDDED 1
#endif

#if REBAX_EXPORT_TEMPLATE_EMBEDDED

/* Embedded export toolchain. */
extern const unsigned char _binary_embedded_export_tools_ps2dev_tar_xz_start[];
extern const unsigned char _binary_embedded_export_tools_ps2dev_tar_xz_end[];

#if defined(__GNUC__) || defined(__clang__)
#define REBAX_WEAK __attribute__((weak))
#else
#define REBAX_WEAK
#endif
extern const unsigned char _binary_embedded_export_tools_make_start[] REBAX_WEAK;
extern const unsigned char _binary_embedded_export_tools_make_end[] REBAX_WEAK;
extern const unsigned char _binary_embedded_export_tools_make_exe_start[] REBAX_WEAK;
extern const unsigned char _binary_embedded_export_tools_make_exe_end[] REBAX_WEAK;

static inline size_t embedded_make_size(void) {
    if (_binary_embedded_export_tools_make_start && _binary_embedded_export_tools_make_end)
        return (size_t)(_binary_embedded_export_tools_make_end - _binary_embedded_export_tools_make_start);
    if (_binary_embedded_export_tools_make_exe_start && _binary_embedded_export_tools_make_exe_end)
        return (size_t)(_binary_embedded_export_tools_make_exe_end - _binary_embedded_export_tools_make_exe_start);
    return 0;
}

static inline const unsigned char *embedded_make_start(void) {
    if (_binary_embedded_export_tools_make_start && _binary_embedded_export_tools_make_end)
        return _binary_embedded_export_tools_make_start;
    if (_binary_embedded_export_tools_make_exe_start && _binary_embedded_export_tools_make_exe_end)
        return _binary_embedded_export_tools_make_exe_start;
    return NULL;
}

static inline size_t embedded_ps2dev_size(void) {
    return (size_t)(_binary_embedded_export_tools_ps2dev_tar_xz_end
                   - _binary_embedded_export_tools_ps2dev_tar_xz_start);
}

/* Embedded node source archive. */
extern const unsigned char _binary_embedded_export_resources_node_sources_tar_xz_start[];
extern const unsigned char _binary_embedded_export_resources_node_sources_tar_xz_end[];

static inline size_t embedded_node_sources_size(void) {
    return (size_t)(_binary_embedded_export_resources_node_sources_tar_xz_end
                   - _binary_embedded_export_resources_node_sources_tar_xz_start);
}

#else

static inline size_t embedded_make_size(void) { return 0; }
static inline const unsigned char *embedded_make_start(void) { return NULL; }
static inline size_t embedded_ps2dev_size(void) { return 0; }
static inline size_t embedded_node_sources_size(void) { return 0; }

#endif /* REBAX_EXPORT_TEMPLATE_EMBEDDED */

/* Engine UI assets remain embedded in both modes. */
extern const unsigned char _binary_embedded_engine_fonts_SpaceGrotesk_Regular_ttf_start[];
extern const unsigned char _binary_embedded_engine_fonts_SpaceGrotesk_Regular_ttf_end[];

static inline size_t embedded_font_regular_size(void) {
    return (size_t)(_binary_embedded_engine_fonts_SpaceGrotesk_Regular_ttf_end
                   - _binary_embedded_engine_fonts_SpaceGrotesk_Regular_ttf_start);
}

extern const unsigned char _binary_embedded_engine_fonts_SpaceGrotesk_Bold_ttf_start[];
extern const unsigned char _binary_embedded_engine_fonts_SpaceGrotesk_Bold_ttf_end[];

static inline size_t embedded_font_bold_size(void) {
    return (size_t)(_binary_embedded_engine_fonts_SpaceGrotesk_Bold_ttf_end
                   - _binary_embedded_engine_fonts_SpaceGrotesk_Bold_ttf_start);
}

extern const unsigned char _binary_embedded_engine_images_icons_icons_png_start[];
extern const unsigned char _binary_embedded_engine_images_icons_icons_png_end[];

static inline size_t embedded_icon_atlas_size(void) {
    return (size_t)(_binary_embedded_engine_images_icons_icons_png_end
                   - _binary_embedded_engine_images_icons_icons_png_start);
}

#endif /* EMBEDDED_RESOURCES_H */
