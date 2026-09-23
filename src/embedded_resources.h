/*
 * ============================================================
 * embedded_resources.h
 * ============================================================
 * إعلانات الرموز الخاصة بالموارد المضمّنة داخل الملف التنفيذي
 * عبر آلية objcopy التلقائية في ملف البناء. رمز كل مورد مبني من
 * مساره الكامل (بما فيه المجلدات) بعد استبدال أي محرف غير صالح
 * لاسم متغير سي - مثل / و . و - - بشرطة سفلية.
 * ------------------------------------------------------------
 * كل مورد جديد يُضاف مستقبلاً داخل embedded/ يُعلَن هنا بنفس النمط.
 * الأيقونات لا تُعلَن هنا فردياً - أطلسها المجمَّع (icons.png) وحده
 * مورد مضمّن، وتُقرأ خلاياه عبر icon_atlas.c + icon_names.h المولَّد.
 * ============================================================
 */

#ifndef EMBEDDED_RESOURCES_H
#define EMBEDDED_RESOURCES_H

#include <stddef.h>

/* ------------------------------------------------------------
 * بيئة ps2dev كاملة (مضغوطة tar.xz) - المترجم mips64r5900el-ps2-elf-gcc
 * وكل أدواته ومكتباته اللازمة للتصدير النهائي للعبة PS2
 * embedded/tools/ps2dev.tar.xz
 * ------------------------------------------------------------ */
extern const unsigned char _binary_embedded_tools_ps2dev_tar_xz_start[];
extern const unsigned char _binary_embedded_tools_ps2dev_tar_xz_end[];

#if defined(__GNUC__) || defined(__clang__)
#define REBAX_WEAK __attribute__((weak))
#else
#define REBAX_WEAK
#endif
extern const unsigned char _binary_embedded_tools_make_start[] REBAX_WEAK;
extern const unsigned char _binary_embedded_tools_make_end[] REBAX_WEAK;
extern const unsigned char _binary_embedded_tools_make_exe_start[] REBAX_WEAK;
extern const unsigned char _binary_embedded_tools_make_exe_end[] REBAX_WEAK;
static inline size_t embedded_make_size(void) {
    if (_binary_embedded_tools_make_start && _binary_embedded_tools_make_end)
        return (size_t)(_binary_embedded_tools_make_end-_binary_embedded_tools_make_start);
    if (_binary_embedded_tools_make_exe_start && _binary_embedded_tools_make_exe_end)
        return (size_t)(_binary_embedded_tools_make_exe_end-_binary_embedded_tools_make_exe_start);
    return 0;
}
static inline const unsigned char *embedded_make_start(void) {
    if (_binary_embedded_tools_make_start && _binary_embedded_tools_make_end) return _binary_embedded_tools_make_start;
    if (_binary_embedded_tools_make_exe_start && _binary_embedded_tools_make_exe_end) return _binary_embedded_tools_make_exe_start;
    return NULL;
}

static inline size_t embedded_ps2dev_size(void) {
    return (size_t)(_binary_embedded_tools_ps2dev_tar_xz_end
                   - _binary_embedded_tools_ps2dev_tar_xz_start);
}

/* ------------------------------------------------------------
 * سورس src/nodes الحقيقي كما هو (.c + .h، بلا أي ترجمة) - يُستخرج
 * وقت التصدير الفعلي، وتُترجَم منه فقط ملفات أنواع العقد المستخدمة
 * فعلياً بمشاهد التصدير، بمترجم PS2 الحقيقي بتلك اللحظة (راجع شرح
 * gen-node-archive بالـMakefile)
 * embedded/export/node_sources.tar.xz
 * ------------------------------------------------------------ */
extern const unsigned char _binary_embedded_export_node_sources_tar_xz_start[];
extern const unsigned char _binary_embedded_export_node_sources_tar_xz_end[];

static inline size_t embedded_node_sources_size(void) {
    return (size_t)(_binary_embedded_export_node_sources_tar_xz_end
                   - _binary_embedded_export_node_sources_tar_xz_start);
}

/* ------------------------------------------------------------
 * خطوط الواجهة (Space Grotesk) - عادي وBold، تُستخدم من font.c
 * embedded/fonts/SpaceGrotesk-Regular.ttf و -Bold.ttf
 * ------------------------------------------------------------ */
extern const unsigned char _binary_embedded_fonts_SpaceGrotesk_Regular_ttf_start[];
extern const unsigned char _binary_embedded_fonts_SpaceGrotesk_Regular_ttf_end[];

static inline size_t embedded_font_regular_size(void) {
    return (size_t)(_binary_embedded_fonts_SpaceGrotesk_Regular_ttf_end
                   - _binary_embedded_fonts_SpaceGrotesk_Regular_ttf_start);
}

extern const unsigned char _binary_embedded_fonts_SpaceGrotesk_Bold_ttf_start[];
extern const unsigned char _binary_embedded_fonts_SpaceGrotesk_Bold_ttf_end[];

static inline size_t embedded_font_bold_size(void) {
    return (size_t)(_binary_embedded_fonts_SpaceGrotesk_Bold_ttf_end
                   - _binary_embedded_fonts_SpaceGrotesk_Bold_ttf_start);
}

/* ------------------------------------------------------------
 * أطلس أيقونات المحرك بالكامل (256x256، شبكة 16x16 خانة) - يجمع
 * كل أيقونة بالمحرك (عقد + واجهة) بملف واحد. تُستخدم من icon_atlas.c
 * embedded/images/icons/icons.png
 * ------------------------------------------------------------ */
extern const unsigned char _binary_embedded_images_icons_icons_png_start[];
extern const unsigned char _binary_embedded_images_icons_icons_png_end[];

static inline size_t embedded_icon_atlas_size(void) {
    return (size_t)(_binary_embedded_images_icons_icons_png_end
                   - _binary_embedded_images_icons_icons_png_start);
}

#endif /* EMBEDDED_RESOURCES_H */
