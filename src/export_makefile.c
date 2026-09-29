/* Deterministic native PS2 Makefile generation. */
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <stdlib.h>
#include "export_internal.h"

void export_makefile_write(const char *build_dir, const char *src_dir_name) {
    char path[1300];
    snprintf(path, sizeof(path), "%s/Makefile", build_dir);
    FILE *f = fopen(path, "w");
    if (f == NULL) return;

    fprintf(f, "# مولَّد تلقائياً وقت التصدير من Rebax_Engine - لا تعدّله يدوياً\n\n");
    fprintf(f, "EE_BIN = %s.elf\n", g_exe_name);
    fprintf(f, "EE_OBJS = scene_loader_main.o scene_runtime.o scene_data.o");

    char source_names[256][128];
    size_t source_count = 0;
    DIR *dir = opendir(build_dir);
    if (dir != NULL) {
        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL && source_count < 256) {
            size_t len = strlen(entry->d_name);
            if (len > 2 && strcmp(entry->d_name + len - 2, ".c") == 0
                && strcmp(entry->d_name, "scene_loader_main.c") != 0
                && strcmp(entry->d_name, "scene_runtime.c") != 0
                && strcmp(entry->d_name, "scene_data.c") != 0) {
                strncpy(source_names[source_count], entry->d_name,
                        sizeof(source_names[source_count]) - 1);
                source_names[source_count][sizeof(source_names[source_count]) - 1] = '\0';
                source_count++;
            }
        }
        closedir(dir);
    }
    for (size_t i = 1; i < source_count; i++) {
        char key[128];
        memcpy(key, source_names[i], sizeof(key));
        size_t j = i;
        while (j > 0 && strcmp(key, source_names[j - 1]) < 0) {
            memcpy(source_names[j], source_names[j - 1], sizeof(source_names[j]));
            j--;
        }
        memcpy(source_names[j], key, sizeof(source_names[j]));
    }
    for (size_t i = 0; i < source_count; i++) {
        char obj_name[128];
        strncpy(obj_name, source_names[i], sizeof(obj_name) - 1);
        obj_name[sizeof(obj_name) - 1] = '\0';
        char *dot = strrchr(obj_name, '.');
        if (dot) strcpy(dot, ".o");
        fprintf(f, " %s", obj_name);
    }
    fprintf(f, "\n\n");

    fprintf(f, "EE_INCS := -I%s\n", src_dir_name);

    /* لا -ldebug افتراضياً بعد اليوم - scene_loader_main.c الحالي
     * (حلقة لعبة gsKit حقيقية) ما يستخدم init_scr/scr_printf إطلاقاً.
     * أي عقدة مستقبلية تحتاج طباعة تشخيصية حقيقية وقت التشغيل تصرّح
     * عنها بماركر @PS2_EXPORT_LIBS الخاص بها هي - نفس الآلية العامة،
     * صفر افتراض بالمصدِّر نفسه لمكتبة PS2SDK غير أساسية بعينها.
     *
     * --start-group/--end-group يلف كل المكتبات الساكنة المجمَّعة -
     * الربط الساكن العادي يفحص كل أرشيف مرة وحدة بترتيبه بالسطر
     * (يسار→يمين)، فلو مكتبة لاحقة تحتاج رمزاً من مكتبة سابقة (زي
     * gsKit_texture_finish بـgskit_toolkit يحتاج gsKit_texture_size
     * من gskit الأساسية، بغض النظر عن ترتيبهما بالماركر)، يفشل
     * الربط. اللف بمجموعة start/end يخلي الرابط يعيد فحص كل
     * الأرشيفات بالمجموعة حتى تُحل كل الرموز - يحل هذا الصنف كامل
     * من مشاكل الترتيب دفعة وحدة، مهما كان ترتيب تصريح أي عقدة
     * مستقبلية لمكتباتها بملفها هي */
    fprintf(f, "EE_LIBS := -Wl,--start-group\n");

    /* بقية المكتبات/المسارات - عامة بالكامل، بلا أي معرفة هنا باسم
     * أي عقدة أو مكتبة بعينها. كل ملف عقدة (أو ملف مشترك زي
     * image_loader_common.c) يصرّح عن احتياجه بتعليق
     * @PS2_EXPORT_LIBS/@PS2_EXPORT_INCLUDES بملفه هو، ونحن هنا
     * بس نجمع ما صرَّحت عنه الملفات اللي فعلاً انتسخت لهذا التصدير
     * (راجع scan_copied_file_for_export_flags) - عقدة مستقبلية
     * تحتاج مكتبة ps2sdk جديدة تصرّح بملفها هي، بلا أي تعديل هنا */
    for (int i = 0; i < g_extra_incs_count; i++) {
        fprintf(f, "EE_INCS += %s\n", g_extra_incs[i]);
    }
    for (int i = 0; i < g_extra_libs_count; i++) {
        fprintf(f, "EE_LIBS += %s\n", g_extra_libs[i]);
    }
    fprintf(f, "EE_LIBS += -Wl,--end-group\n");
    fprintf(f, "\n");

fprintf(f, ".DEFAULT_GOAL := all\n\n");

if (g_is_release) {
    fprintf(f, "all: $(EE_BIN)\n\t$(EE_STRIP) --strip-all $(EE_BIN)\n");
} else {
    fprintf(f, "all: $(EE_BIN)\n");
}

fprintf(f, "\n");
fprintf(f, "include $(PS2SDK)/samples/Makefile.pref\n");
fprintf(f, "include $(PS2SDK)/samples/Makefile.eeglobal\n\n");

    fclose(f);
}

