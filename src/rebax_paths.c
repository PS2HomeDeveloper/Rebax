/*
 * ============================================================
 * rebax_paths.c
 * ============================================================
 * راجع rebax_paths.h للتوثيق الكامل والفلسفة العامة.
 * ============================================================
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <SDL.h>

#include "rebax_paths.h"
#include "rebax_fs.h"
#include "embedded_resources.h"

#ifndef REBAX_EXPORT_TEMPLATE_EMBEDDED
#define REBAX_EXPORT_TEMPLATE_EMBEDDED 1
#endif

/* -Wformat-truncation: نفس التبرير المذكور بـps2_exporter.c - كل
 * المسارات هنا قصيرة عملياً مقارنة بحجم المصفوفات */
#pragma GCC diagnostic ignored "-Wformat-truncation"

#define REBAX_PATH_MAX 1536

static char g_root_dir[REBAX_PATH_MAX];
static char g_toolchains_dir[REBAX_PATH_MAX];
static char g_toolchain_dir[REBAX_PATH_MAX];
static char g_make_path[REBAX_PATH_MAX];
static char g_node_sources_dir[REBAX_PATH_MAX];
static char g_node_resources_dir[REBAX_PATH_MAX];
static char g_export_resources_dir[REBAX_PATH_MAX];
static char g_temp_export_dir[REBAX_PATH_MAX];
static char g_settings_dir[REBAX_PATH_MAX];
static int  g_paths_ready = 0;

/* ينشئ كل مجلد بالمسار تباعاً (نفس تأثير "mkdir -p") - بسيط ومحمول
 * بلا اعتماد على أي أمر خارجي هنا بالذات (خلافاً لبقية المشروع اللي
 * يشغّل أوامر شل لعمليات التصدير الثقيلة - هذا استدعاء نظام مباشر
 * رخيص، يُستدعى كثيراً) */
static void mkdir_recursive(const char *path) {
    char buf[REBAX_PATH_MAX];
    strncpy(buf, path, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    for (char *p = buf + 1; *p != '\0'; p++) {
        if (*p == '/') {
            *p = '\0';
            mkdir(buf, 0755);
            *p = '/';
        }
    }
    mkdir(buf, 0755);
}

/* يحسب المسار الجذر المناسب للمنصة الحالية - مرة وحدة، مخزَّن
 * بمتغيّرات ساكنة. كل فرع محايد تماماً عن بيئة أي مطوّر بعينه -
 * البرنامج مفتوح المصدر، لازم يشتغل صح على أي جهاز */
static void compute_paths(void) {
    if (g_paths_ready) return;

#if defined(_WIN32)
    /* Windows: %LOCALAPPDATA%\Rebax */
    const char *local_appdata = getenv("LOCALAPPDATA");
    if (local_appdata == NULL) local_appdata = "."; /* احتياط بعيد - نادراً ما يحصل */
    snprintf(g_root_dir, sizeof(g_root_dir), "%s\\Rebax", local_appdata);
    snprintf(g_toolchains_dir, sizeof(g_toolchains_dir), "%s\\Engine\\toolchains", g_root_dir);
    snprintf(g_toolchain_dir, sizeof(g_toolchain_dir), "%s\\ps2dev", g_toolchains_dir);
    snprintf(g_make_path, sizeof(g_make_path), "%s\\make.exe", g_toolchains_dir);
    snprintf(g_export_resources_dir, sizeof(g_export_resources_dir), "%s\\Engine\\export_resources", g_root_dir);
    snprintf(g_node_resources_dir, sizeof(g_node_resources_dir), "%s\\Engine\\resources\\nodes", g_root_dir);
    snprintf(g_node_sources_dir, sizeof(g_node_sources_dir), "%s\\node_sources", g_node_resources_dir);
    snprintf(g_temp_export_dir, sizeof(g_temp_export_dir), "%s\\Temp\\export", g_root_dir);
    snprintf(g_settings_dir, sizeof(g_settings_dir), "%s\\Settings", g_root_dir);

#elif defined(__ANDROID__)
    const char *android_root = SDL_AndroidGetInternalStoragePath();
    if (android_root == NULL) {
        fprintf(stderr, "[rebax] FAILED: Android internal storage path is unavailable.\n");
        abort();
    }

    snprintf(g_root_dir, sizeof(g_root_dir), "%s/Rebax", android_root);
    snprintf(g_toolchains_dir, sizeof(g_toolchains_dir), "%s/Engine/toolchains", g_root_dir);
    snprintf(g_toolchain_dir, sizeof(g_toolchain_dir), "%s/ps2dev", g_toolchains_dir);
    snprintf(g_make_path, sizeof(g_make_path), "%s/make", g_toolchains_dir);
    snprintf(g_export_resources_dir, sizeof(g_export_resources_dir), "%s/Engine/export_resources", g_root_dir);
    snprintf(g_node_resources_dir, sizeof(g_node_resources_dir), "%s/Engine/resources/nodes", g_root_dir);
    snprintf(g_node_sources_dir, sizeof(g_node_sources_dir), "%s/node_sources", g_node_resources_dir);
    snprintf(g_temp_export_dir, sizeof(g_temp_export_dir), "%s/Temp/export", g_root_dir);
    snprintf(g_settings_dir, sizeof(g_settings_dir), "%s/Settings", g_root_dir);

#elif defined(__linux__)
    /* Linux: ~/.local/share/Rebax */
    const char *home = getenv("HOME");
    if (home == NULL) home = "."; /* احتياط بعيد */
    snprintf(g_root_dir, sizeof(g_root_dir), "%s/.local/share/Rebax", home);
    snprintf(g_toolchains_dir, sizeof(g_toolchains_dir), "%s/Engine/toolchains", g_root_dir);
    snprintf(g_toolchain_dir, sizeof(g_toolchain_dir), "%s/ps2dev", g_toolchains_dir);
    snprintf(g_make_path, sizeof(g_make_path), "%s/make", g_toolchains_dir);
    snprintf(g_export_resources_dir, sizeof(g_export_resources_dir), "%s/Engine/export_resources", g_root_dir);
    snprintf(g_node_resources_dir, sizeof(g_node_resources_dir), "%s/Engine/resources/nodes", g_root_dir);
    snprintf(g_node_sources_dir, sizeof(g_node_sources_dir), "%s/node_sources", g_node_resources_dir);
    snprintf(g_temp_export_dir, sizeof(g_temp_export_dir), "%s/Temp/export", g_root_dir);
    snprintf(g_settings_dir, sizeof(g_settings_dir), "%s/Settings", g_root_dir);

#elif defined(__APPLE__)
    /* macOS - مستقبلاً. نفس الأسلوب بالضبط: مسار جذر قياسي واحد
     * (عادة ~/Library/Application Support/Rebax)، ونفس الشجرة تحته
     * بالضبط كـLinux/Windows فوق - يحتاج تأكيد فعلي على جهاز macOS
     * حقيقي وقتها، غير مطبَّق الآن */
    #error "macOS غير مدعوم بعد - راجع التعليق فوق وقت إضافته"

#else
    #error "منصة غير معروفة - أضف فرعاً جديداً هنا بنفس أسلوب الفروع فوق"
#endif

    mkdir_recursive(g_temp_export_dir);
    mkdir_recursive(g_settings_dir);
    mkdir_recursive(g_export_resources_dir);
    mkdir_recursive(g_toolchains_dir);
    mkdir_recursive(g_node_resources_dir);
#if REBAX_EXPORT_TEMPLATE_EMBEDDED
    mkdir_recursive(g_toolchain_dir);
    mkdir_recursive(g_node_sources_dir);
#endif

    g_paths_ready = 1;
}

const char *rebax_root_dir(void)      { compute_paths(); return g_root_dir; }
const char *rebax_toolchains_dir(void) { compute_paths(); return g_toolchains_dir; }
const char *rebax_toolchain_dir(void) { compute_paths(); return g_toolchain_dir; }
const char *rebax_make_path(void) { compute_paths(); return g_make_path; }
const char *rebax_node_sources_dir(void) { compute_paths(); return g_node_sources_dir; }
const char *rebax_export_resources_dir(void) { compute_paths(); return g_export_resources_dir; }
const char *rebax_temp_export_dir(void)  { compute_paths(); return g_temp_export_dir; }
const char *rebax_settings_dir(void)     { compute_paths(); return g_settings_dir; }

/* ------------------------------------------------------------
 * استخراج ps2dev.tar.xz وnode_sources.tar.xz المُضمَّنين بالملف
 * التنفيذي - مرة وحدة بحياة تثبيت ريباكس هذا (ملف علامة .extracted_ok
 * يمنع إعادة الاستخراج كل تشغيل). غير حاجز - نفس أسلوب shell_step
 * بـps2_exporter.c بالضبط (نسخة مستقلة صغيرة هنا، الوحدتان منفصلتان
 * عمداً - ps2_exporter لم يعد يستخرج شيئاً بنفسه، يقرأ فقط من هنا) */
typedef enum { SETUP_STATE_NONE, SETUP_STATE_EXTRACT_ENGINE_TOOLS, SETUP_STATE_EXTRACT_PS2DEV, SETUP_STATE_EXTRACT_MAKE, SETUP_STATE_EXTRACT_NODE_SOURCES, SETUP_STATE_DONE, SETUP_STATE_FAILED } setup_state_t;
static volatile setup_state_t g_setup_state=SETUP_STATE_NONE;
static SDL_Thread *g_setup_thread=NULL;
static int write_blob(const unsigned char *start,size_t size,const char *path) {
    if (!start || !size) return 0;
    FILE *f=fopen(path,"wb");
    if(!f) return 0;
    int ok=fwrite(start,1,size,f)==size; fclose(f);
#ifndef _WIN32
    if(ok) chmod(path,0755);
#endif
    return ok;
}
int rebax_paths_is_setup_needed(void) {
    compute_paths(); struct stat st; char marker[REBAX_PATH_MAX];
    snprintf(marker,sizeof(marker),"%s/.engine_tools_ok",g_toolchains_dir);
    if(stat(marker,&st)!=0) return 1;
    snprintf(marker,sizeof(marker),"%s/.extracted_ok",g_node_sources_dir);
    if(stat(marker,&st)!=0) return 1;
#if REBAX_EXPORT_TEMPLATE_EMBEDDED
    snprintf(marker,sizeof(marker),"%s/.extracted_ok",g_toolchain_dir);
    if(stat(marker,&st)!=0) return 1;
    /* قد يكون make موجوداً مسبقاً داخل Engine/toolchains في نسخة المستخدم؛
     * لا نطلب مورداً مضمناً إذا كان الملف المحلي موجوداً. */
    if(embedded_make_size()==0 && stat(g_make_path,&st)!=0) return 1;
    return 0;
#else
    return 0;
#endif
}
void rebax_paths_setup_start(void) {
    compute_paths();
    g_setup_thread=NULL;
    if(!rebax_paths_is_setup_needed()){g_setup_state=SETUP_STATE_DONE;return;}
    printf("[rebax] first run - preparing engine tools...\n");
    g_setup_state=SETUP_STATE_EXTRACT_ENGINE_TOOLS;
}
static void setup_step(void) {
    char archive[REBAX_PATH_MAX], temp[REBAX_PATH_MAX], marker[REBAX_PATH_MAX];
    switch (g_setup_state) {
    case SETUP_STATE_NONE:
    case SETUP_STATE_DONE:
    case SETUP_STATE_FAILED:
        return;
    case SETUP_STATE_EXTRACT_ENGINE_TOOLS: {
        char tool_file[REBAX_PATH_MAX];
        printf("[rebax] installing engine tools...\n");
        snprintf(tool_file, sizeof(tool_file), "%s/text.txt", g_toolchains_dir);
        if (!write_blob(_binary_embedded_engine_tools_text_txt_start,
                        embedded_engine_tools_size(), tool_file)) {
            g_setup_state = SETUP_STATE_FAILED;
            return;
        }
        snprintf(marker, sizeof(marker), "%s/.engine_tools_ok", g_toolchains_dir);
        FILE *marker_file = fopen(marker, "wb");
        if (marker_file) fclose(marker_file);
#if REBAX_EXPORT_TEMPLATE_EMBEDDED
        g_setup_state = SETUP_STATE_EXTRACT_PS2DEV;
#else
        g_setup_state = SETUP_STATE_EXTRACT_NODE_SOURCES;
#endif
        return;
    }
#if REBAX_EXPORT_TEMPLATE_EMBEDDED
    case SETUP_STATE_EXTRACT_PS2DEV:
        snprintf(archive, sizeof(archive), "%s/ps2dev.tar.xz", g_root_dir);
        printf("[rebax] extracting ps2dev toolchain...\n");
        if (!write_blob(_binary_embedded_export_tools_ps2dev_tar_xz_start,
                        embedded_ps2dev_size(), archive)
            || !rebax_fs_extract_tar_xz(archive, g_toolchains_dir)) {
            printf("[rebax] FAILED: could not extract ps2dev toolchain.\n");
            remove(archive);
            g_setup_state = SETUP_STATE_FAILED;
            return;
        }
        remove(archive);
        snprintf(marker, sizeof(marker), "%s/.extracted_ok", g_toolchain_dir);
        { FILE *f = fopen(marker, "wb"); if (f) fclose(f); }
        g_setup_state = SETUP_STATE_EXTRACT_MAKE;
        return;
    case SETUP_STATE_EXTRACT_MAKE:
        printf("[rebax] installing bundled make...\n");
        if (embedded_make_size() > 0
            && !write_blob(embedded_make_start(), embedded_make_size(), g_make_path)) {
            printf("[rebax] FAILED: could not install embedded make.\n");
            g_setup_state = SETUP_STATE_FAILED;
            return;
        }
        g_setup_state = SETUP_STATE_EXTRACT_NODE_SOURCES;
        return;
#endif
    case SETUP_STATE_EXTRACT_NODE_SOURCES:
        printf("[rebax] extracting node sources...\n");
        snprintf(archive, sizeof(archive), "%s/node_sources.tar.xz", g_root_dir);
        snprintf(temp, sizeof(temp), "%s/.node_sources_extract", g_root_dir);
        rebax_fs_remove_recursive(temp);
        if (!write_blob(_binary_embedded_engine_resources_nodes_node_sources_tar_xz_start,
                        embedded_node_sources_size(), archive)
            || !rebax_fs_extract_tar_xz(archive, temp)) {
            printf("[rebax] FAILED: could not extract node sources.\n");
            remove(archive);
            rebax_fs_remove_recursive(temp);
            g_setup_state = SETUP_STATE_FAILED;
            return;
        }
        remove(archive);
        rebax_fs_remove_recursive(g_node_sources_dir);
        {
            char from[REBAX_PATH_MAX];
            snprintf(from, sizeof(from), "%s/node_sources", temp);
            if (!rebax_fs_move(from, g_node_sources_dir)) {
                printf("[rebax] FAILED: could not place extracted node sources.\n");
                rebax_fs_remove_recursive(temp);
                g_setup_state = SETUP_STATE_FAILED;
                return;
            }
        }
        rebax_fs_remove_recursive(temp);
        snprintf(marker, sizeof(marker), "%s/.extracted_ok", g_node_sources_dir);
        { FILE *f = fopen(marker, "wb"); if (f) fclose(f); }
        g_setup_state = SETUP_STATE_DONE;
        return;
    }
}

static int setup_worker(void *unused) {
    (void)unused;
    while (g_setup_state != SETUP_STATE_DONE && g_setup_state != SETUP_STATE_FAILED) {
        setup_step();
        SDL_Delay(1);
    }
    return 0;
}

void rebax_paths_setup_update(void) {
    if (g_setup_thread == NULL &&
        g_setup_state != SETUP_STATE_DONE &&
        g_setup_state != SETUP_STATE_FAILED) {
        g_setup_thread = SDL_CreateThread(setup_worker, "rebax_setup", NULL);
        if (g_setup_thread == NULL) {
            fprintf(stderr, "[rebax] FAILED: could not start setup worker: %s\n", SDL_GetError());
            g_setup_state = SETUP_STATE_FAILED;
        }
    }
    if (g_setup_thread != NULL &&
        (g_setup_state == SETUP_STATE_DONE || g_setup_state == SETUP_STATE_FAILED)) {
        SDL_WaitThread(g_setup_thread, NULL);
        g_setup_thread = NULL;
    }
}

int rebax_paths_setup_done(void) {
    return g_setup_state == SETUP_STATE_DONE || g_setup_state == SETUP_STATE_FAILED
           || g_setup_state == SETUP_STATE_NONE;
}

int rebax_paths_setup_failed(void) {
    return g_setup_state == SETUP_STATE_FAILED;
}

int rebax_export_template_is_required(void) {
#if REBAX_EXPORT_TEMPLATE_EMBEDDED
    return 0;
#else
    return 1;
#endif
}

int rebax_export_template_is_installed(void) {
    struct stat st;
    compute_paths();
    if (stat(g_toolchain_dir, &st) != 0 || !S_ISDIR(st.st_mode)) return 0;
    if (stat(g_make_path, &st) != 0) return 0;
    if (stat(g_export_resources_dir, &st) != 0 || !S_ISDIR(st.st_mode)) return 0;
    return 1;
}

int rebax_export_template_install(const char *archive_path) {
    if (archive_path == NULL || archive_path[0] == '\0') return 0;
    compute_paths();
    char temp[REBAX_PATH_MAX];
    snprintf(temp, sizeof(temp), "%s/.template_install", g_root_dir);
    rebax_fs_remove_recursive(temp);
    if (!rebax_fs_extract_tar_xz(archive_path, temp)) {
        rebax_fs_remove_recursive(temp);
        return 0;
    }

    char tools_src[REBAX_PATH_MAX], resources_src[REBAX_PATH_MAX];
    snprintf(tools_src, sizeof(tools_src), "%s/tools", temp);
    snprintf(resources_src, sizeof(resources_src), "%s/resources", temp);
    struct stat st;
    if (stat(tools_src, &st) != 0 || !S_ISDIR(st.st_mode)
        || stat(resources_src, &st) != 0 || !S_ISDIR(st.st_mode)) {
        rebax_fs_remove_recursive(temp);
        return 0;
    }
    char ps2dev_src[REBAX_PATH_MAX], make_src[REBAX_PATH_MAX];
    snprintf(ps2dev_src, sizeof(ps2dev_src), "%s/ps2dev", tools_src);
#if defined(_WIN32)
    snprintf(make_src, sizeof(make_src), "%s/make.exe", tools_src);
#else
    snprintf(make_src, sizeof(make_src), "%s/make", tools_src);
#endif
    if (!rebax_fs_move(ps2dev_src, g_toolchain_dir)
        || !rebax_fs_move(make_src, g_make_path)
        || !rebax_fs_move(resources_src, g_export_resources_dir)) {
        rebax_fs_remove_recursive(temp);
        return 0;
    }
    rebax_fs_remove_recursive(temp);
    return rebax_export_template_is_installed();
}

int rebax_export_template_open_download_url(void) {
#ifndef REBAX_TOOLCHAIN_URL
#define REBAX_TOOLCHAIN_URL "https://github.com/PS2HomeDeveloper/Rebax-Toolchains/releases/download/1.0.0/rebax-toolchains-linux-x86_64.tar.xz"
#endif
#if defined(_WIN32)
    char command[2048];
    snprintf(command, sizeof(command), "start \"\" \"%s\"", REBAX_TOOLCHAIN_URL);
#elif defined(__ANDROID__)
    char command[2048];
    snprintf(command, sizeof(command), "am start -a android.intent.action.VIEW -d \"%s\"", REBAX_TOOLCHAIN_URL);
#else
    char command[2048];
    snprintf(command, sizeof(command), "xdg-open \"%s\"", REBAX_TOOLCHAIN_URL);
#endif
    return system(command) == 0;
}
