/* Node selection and dependency discovery for native exports. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include "export_internal.h"
#include "rebax_fs.h"
#pragma GCC diagnostic ignored "-Wformat-truncation"
#define MAX_EXTRA_FLAGS EXPORT_MAX_EXTRA_FLAGS

/* ------------------------------------------------------------
 * يمسح g_nodes_src_dir بحثاً عن أسماء الملفات المطابقة لأنواع
 * العقد المستخدمة (سطر "@NODE ... name=<X>" بكل ملف .c) - وينسخ
 * الملف (+ .h المطابق لو موجود) لمجلد بناء التصدير. يرجع 1 لو
 * كل الأنواع المستخدمة انطابقت بملف فعلي، 0 لو نوع واحد ع الأقل
 * ما له ملف (خطأ حقيقي - عقدة بمشروع بلا سورس تصدير لها) */

/* المكتبات/مسارات include الإضافية (غير الأساسية) اللي ملفات العقد
 * المنسوخة نفسها تصرّح عنها بتعليق @PS2_EXPORT_LIBS/@PS2_EXPORT_INCLUDES
 * بأي مكان بملفها - بلا أي معرفة بالمكتبة أو اسم العقدة بكود المصدّر
 * نفسه. هذا يعني: عقدة مستقبلية تحتاج مكتبة ps2sdk جديدة (صوت، شبكة...)
 * تصرّح عن حاجتها بملفها هي بس - المصدّر ما يحتاج أي تعديل إطلاقاً */

static void add_unique_flag(char list[][160], int *count, const char *value) {
    for (int i = 0; i < *count; i++) {
        if (strcmp(list[i], value) == 0) return;
    }
    if (*count >= MAX_EXTRA_FLAGS) return;
    strncpy(list[*count], value, 159);
    list[*count][159] = '\0';
    (*count)++;
}

/* يقرأ سطراً بصيغة "... @PS2_EXPORT_LIBS: <flags> ..." (تعليق C
 * عادي، اللي بعد ":" يُؤخذ حرفياً كما هو لحد أول "*" (لو تعليق
 * كتلة) أو نهاية السطر) ويضيفه للمصفوفة المطابقة - بلا أي افتراض
 * عن أي عقدة أو مكتبة بعينها */
static void scan_line_for_marker(const char *line, const char *marker,
                                  char list[][160], int *count) {
    const char *p = strstr(line, marker);
    if (p == NULL) return;
    p += strlen(marker);
    while (*p == ' ' || *p == '\t') p++;

    char value[160];
    strncpy(value, p, sizeof(value) - 1);
    value[sizeof(value) - 1] = '\0';

    char *end_comment = strstr(value, "*/");
    if (end_comment != NULL) *end_comment = '\0';

    size_t vlen = strlen(value);
    while (vlen > 0 && (value[vlen - 1] == ' ' || value[vlen - 1] == '\t'
                         || value[vlen - 1] == '\n' || value[vlen - 1] == '\r')) {
        value[--vlen] = '\0';
    }
    if (vlen > 0) add_unique_flag(list, count, value);
}

/* يفحص ملف مصدر واحد (أي ملف نسخناه للتصدير) بحثاً عن تعليقات
 * @PS2_EXPORT_LIBS/@PS2_EXPORT_INCLUDES، ويسجّلها. يُستدعى على كل
 * ملف .c/.h نُسخ فعلياً - بلا استثناء، بلا معرفة مسبقة بمحتواه */
static void scan_copied_file_for_export_flags(const char *path) {
    long size = 0;
    char *content = export_read_whole_file(path, &size);
    if (content == NULL) return;

    char *line = strtok(content, "\n");
    while (line != NULL) {
        scan_line_for_marker(line, "@PS2_EXPORT_LIBS:", g_extra_libs, &g_extra_libs_count);
        scan_line_for_marker(line, "@PS2_EXPORT_INCLUDES:", g_extra_incs, &g_extra_incs_count);
        if (strstr(line, "#include \"image_loader.h\"") != NULL) {
            g_needs_image_loader = 1;
        }
        if (strstr(line, "#include \"engine_context.h\"") != NULL) {
            g_needs_engine_context_stub = 1;
        }
        line = strtok(NULL, "\n");
    }
    free(content);
}

int export_nodes_copy_matched(const char *dest_src_dir) {
    DIR *dir = opendir(g_nodes_src_dir);
    if (dir == NULL) {
        log_pushf("[exporter] failed to open node resources: %s", g_nodes_src_dir);
        return 0;
    }

    int matched_count = 0;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        size_t len = strlen(entry->d_name);
        if (len < 3 || strcmp(entry->d_name + len - 2, ".c") != 0) continue;

        char full_path[1536];
        snprintf(full_path, sizeof(full_path), "%s/%s", g_nodes_src_dir, entry->d_name);

        long size = 0;
        char *content = export_read_whole_file(full_path, &size);
        if (content == NULL) continue;

        char *at_node = strstr(content, "@NODE");
        if (at_node == NULL) { free(content); continue; }

        char *name_kv = strstr(at_node, "name=");
        if (name_kv == NULL) { free(content); continue; }
        name_kv += 5;

        char type_name[64];
        int i = 0;
        while (name_kv[i] != '\0' && name_kv[i] != ' ' && name_kv[i] != '\n'
               && name_kv[i] != '*' && i < 63) {
            type_name[i] = name_kv[i];
            i++;
        }
        type_name[i] = '\0';
        free(content);

        int is_used = 0;
        for (int t = 0; t < g_used_types_count; t++) {
            if (strcmp(g_used_types[t], type_name) == 0) { is_used = 1; break; }
        }
        if (!is_used) continue;

        /* نسخ الملف .c نفسه + .h المطابق لو موجود (بلا فشل لو مافيه -
         * بعض العقد بلا هيدر خاص، تكتفي بـnode_interface.h المشترك) */
        char base_name[64];
        strncpy(base_name, entry->d_name, sizeof(base_name) - 1);
        base_name[sizeof(base_name) - 1] = '\0';
        char *ext_dot = strrchr(base_name, '.');
        if (ext_dot) *ext_dot = '\0';

        char src_c[1600], src_h[1600], dst_c[1600], dst_h[1600];
        snprintf(src_c, sizeof(src_c), "%s/%s.c", g_nodes_src_dir, base_name);
        snprintf(src_h, sizeof(src_h), "%s/%s.h", g_nodes_src_dir, base_name);
        snprintf(dst_c, sizeof(dst_c), "%s/%s.c", dest_src_dir, base_name);
        snprintf(dst_h, sizeof(dst_h), "%s/%s.h", dest_src_dir, base_name);

        if (!rebax_fs_copy_file(src_c, dst_c)) { closedir(dir); return 0; }
        rebax_fs_copy_file(src_h, dst_h);

        scan_copied_file_for_export_flags(src_c);
        scan_copied_file_for_export_flags(src_h); /* بلا ضرر لو .h غير موجود أصلاً - read_whole_file يرجع NULL بهدوء */

        log_pushf("[exporter] included node type: %s (%s.c)", type_name, base_name);
        matched_count++;
    }
    closedir(dir);

    /* node_interface.h مشترك دائماً - كل ملفات العقد تتضمّنه */
    char node_iface_src[1600];
    snprintf(node_iface_src, sizeof(node_iface_src), "%s/node_interface.h", g_nodes_src_dir);
    { char dst[1600]; snprintf(dst,sizeof(dst),"%s/node_interface.h",dest_src_dir); if(!rebax_fs_copy_file(node_iface_src,dst)) return 0; }
    scan_copied_file_for_export_flags(node_iface_src);

    /* engine_context.h - مستقلة تماماً عن g_needs_image_loader (كانت
     * بالخطأ مربوطة به سابقاً بقائمة IMAGE_LOADER_FILES تحت، من
     * تصميم قديم كان Sprite2D يمرّ عبر image_loader.h - لما استغنى
     * عنها، هذا النسخ صار ميتاً بصمت رغم إن g_needs_engine_context_stub
     * يبقى يتفعّل بشكل صحيح ومستقل. شرط منفصل هنا يضمن عدم تكرار
     * هذا الخطأ مع أي تبعية مستقبلية ثانية) */
    if (g_needs_engine_context_stub) {
        char ectx_src[1600];
        snprintf(ectx_src, sizeof(ectx_src), "%s/engine_context.h", g_nodes_src_dir);
        { char dst[1600]; snprintf(dst,sizeof(dst),"%s/engine_context.h",dest_src_dir); rebax_fs_copy_file(ectx_src,dst); }

        /* تحقق فعلي (لا افتراض) - يطبع بوضوح هل النسخة نجحت، حتى ما
         * نضطر نخمّن مرة ثانية لو صار خطأ شبيه مستقبلاً */
        char ectx_dst[1600];
        struct stat st;
        snprintf(ectx_dst, sizeof(ectx_dst), "%s/engine_context.h", dest_src_dir);
        if (stat(ectx_dst, &st) == 0) {
            log_pushf("[exporter] verified: engine_context.h present at %s (%ld bytes)",
                      ectx_dst, (long)st.st_size);
        } else {
            log_pushf("[exporter] WARNING: engine_context.h missing after copy attempt "
                      "(source: %s)", ectx_src);
        }
    }

    /* أي عقدة مطابقة كانت تتضمّن "image_loader.h" فعلياً (اكتُشف
     * أثناء المسح فوق - بلا أي معرفة باسم Sprite2D تحديداً هنا) -
     * ننسخ وحدة تحميل الصور كاملة: المشتركة + الدالة العامة المتفرّعة
     * image_loader_load نفسها (يستدعيها sprite2d.c مباشرة) + *كل*
     * الصيغ السبع - لأن الدالة العامة تشاور على السبع كلها بكودها
     * (راجع تعليق التصميم بimage_loader.h: تحسين "استدعاء الصيغة
     * المحدَّدة مباشرة بدل الدالة العامة" خطوة قادمة منفصلة، غير
     * مطبَّقة بعد - هذا الهيكل الأولي يقبل حجم زائد مؤقت بدل كسر
     * الربط). لا تشمل engine_context.h - نُسخت أعلى فوق باستقلالية */
    if (g_needs_image_loader) {
        static const char *IMAGE_LOADER_FILES[] = {
            "image_loader.h", "image_loader_internal.h",
            "image_loader.c", "image_loader_common.c",
            "image_loader_png.c", "image_loader_jpeg.c", "image_loader_bmp.c",
            "image_loader_tga.c", "image_loader_tiff.c", "image_loader_raw.c",
            "image_loader_tim2.c", "image_loader_tim.c",
        };
        for (size_t f = 0; f < sizeof(IMAGE_LOADER_FILES) / sizeof(IMAGE_LOADER_FILES[0]); f++) {
            char src_path[1600];
            snprintf(src_path, sizeof(src_path), "%s/%s", g_nodes_src_dir, IMAGE_LOADER_FILES[f]);
            { char dst[1600]; snprintf(dst,sizeof(dst),"%s/%s",dest_src_dir,IMAGE_LOADER_FILES[f]); if(!rebax_fs_copy_file(src_path,dst)) return 0; }
            scan_copied_file_for_export_flags(src_path);
        }
        log_push("[exporter] included full image loader subsystem (all formats - "
                 "per-format dead-code elimination is a separate future step).");
    }

    return matched_count == g_used_types_count;
}


