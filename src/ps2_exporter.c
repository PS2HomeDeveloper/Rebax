/*
 * Rebax PS2 Exporter
 *
 * The exporter converts the project's .rscene files into native PS2 C code
 * during the export process.
 *
 * The PS2 executable does NOT receive or parse the original .rscene files.
 * Rebax reads the scenes on the host side, resolves the used node types,
 * converts their properties and assets into the appropriate PS2-side data
 * and code, and generates the temporary source files required to build
 * the final ELF.
 *
 * Export flow:
 *
 *   Project .rscene files
 *          |
 *          v
 *   Rebax exporter
 *          |
 *          +--> Resolve used node types
 *          |
 *          +--> Read scene properties
 *          |
 *          +--> Convert project assets
 *          |
 *          +--> Generate project-specific C source
 *          |
 *          v
 *   Temporary PS2 source tree
 *          |
 *          v
 *   PS2Dev toolchain
 *          |
 *          v
 *   Final PS2 ELF
 *
 * The goal is to make the generated PS2 program as close as possible to
 * a manually written PS2 program. Engine/editor concepts that are only
 * required during development should be resolved by the exporter whenever
 * possible instead of being carried into the final PS2 executable.
 *
 * The exporter is responsible for project-specific generation.
 * Generic PS2 runtime code and node implementations remain in their
 * respective source files and are copied/assembled according to the
 * nodes actually used by the project.
 *
 * The temporary export directory is:
 *
 *   Rebax/Temp/export/
 *
 * It contains only the files required for the current export operation
 * and is cleaned after the export is completed.
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <ctype.h>
#include <stdarg.h>

#include "ps2_exporter.h"
#include "current_project.h"
#include "rebax_paths.h"
#include "rebax_fs.h"

/* -Wformat-truncation: كل التحذيرات هنا نظرية بحتة (GCC يفترض أسوأ
 * حالة لطول أي char[] مصدر بغض النظر عن محتواه الفعلي وقت التشغيل)
 * - كل المسارات هنا فعلياً قصيرة جداً مقارنة بحجم المصفوفات (من
 * readlink/current_project_get_path، لا من مدخل مستخدم غير محدود)،
 * وأي قطع نظري هنا يظهر كفشل بناء واضح بسجل المخرجات، مو خطأ صامت
 * خطير - إيقاف هذا التحذير هنا فقط أوضح من تضخيم عشرات المصفوفات
 * بلا داعٍ فعلي */
#pragma GCC diagnostic ignored "-Wformat-truncation"

/* ------------------------------------------------------------
 * طابور أسطر المخرجات - سطر بكل عنصر، دائري بحجم ثابت. القراءة
 * تسلسلية (ps2_export_poll_next_line) بلا أي حاجة لـthreads (كل
 * شيء هنا أحادي الخيط، يُستطلَع كل إطار من الواجهة) */
#define LOG_LINE_MAX     200
#define LOG_QUEUE_SIZE   1024

static char g_log_lines[LOG_QUEUE_SIZE][LOG_LINE_MAX];
static int  g_log_head = 0;   /* أول سطر لسه ما استُهلك */
static int  g_log_tail = 0;   /* أول خانة فاضية للكتابة القادمة */
static int  g_log_count = 0;

static void log_push(const char *text) {
    /* يُطبَع فوراً لمخرجات البرنامج نفسه (التيرمنال اللي شغّلته منه) -
     * مستقل تماماً عن أي مشكلة برسم النافذة، ومفيد للتشخيص أثناء
     * عمليات طويلة (فك أرشيف ps2dev الضخم مثلاً) */
    printf("%s\n", text);
    fflush(stdout);

    /* لو الطابور امتلأ، نضحّي بأقدم سطر (بدل توقف التصدير) - سجل
     * تشخيصي، مو بيانات حرجة يلزم الاحتفاظ بكل حرف منها للأبد */
    if (g_log_count == LOG_QUEUE_SIZE) {
        g_log_head = (g_log_head + 1) % LOG_QUEUE_SIZE;
        g_log_count--;
    }
    strncpy(g_log_lines[g_log_tail], text, LOG_LINE_MAX - 1);
    g_log_lines[g_log_tail][LOG_LINE_MAX - 1] = '\0';
    g_log_tail = (g_log_tail + 1) % LOG_QUEUE_SIZE;
    g_log_count++;
}

static void log_pushf(const char *fmt, ...) {
    char buf[LOG_LINE_MAX];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    log_push(buf);
}

const char *ps2_export_poll_next_line(void) {
    if (g_log_count == 0) return NULL;
    const char *line = g_log_lines[g_log_head];
    g_log_head = (g_log_head + 1) % LOG_QUEUE_SIZE;
    g_log_count--;
    return line;
}

/* ------------------------------------------------------------
 * تشغيل خطوة شل واحدة (أمر واحد) بشكل غير حاجز - يُقرأ ناتجها
 * (stdout+stderr مدموجين، "2>&1" بنهاية كل أمر) سطراً سطراً كل
 * استدعاء poll، بلا أي انتظار. هذا الأسلوب الوحيد المستخدَم لكل
 * عملية بطيئة بهذا الملف (فك أرشيف، أمر make) - بلا أي threads
 * ------------------------------------------------------------ */
typedef struct {
    FILE *pipe;
    int fd;
    int active;
    char partial[LOG_LINE_MAX]; /* سطر لسه ما اكتمل بالقراءة الحالية */
    int partial_len;
} shell_step_t;

static shell_step_t g_step;

static int shell_step_start(shell_step_t *step, const char *command) {
    step->pipe = popen(command, "r");
    if (step->pipe == NULL) {
        return 0;
    }
    step->fd = fileno(step->pipe);
    int flags = fcntl(step->fd, F_GETFL, 0);
    fcntl(step->fd, F_SETFL, flags | O_NONBLOCK);
    step->active = 1;
    step->partial_len = 0;
    step->partial[0] = '\0';
    return 1;
}

/* يرجع: 1 = لسه شغالة (بلا نتيجة نهائية بعد)، 0 = خلصت (out_ok
 * يحمل النجاح/الفشل حسب exit code) */
static int shell_step_poll(shell_step_t *step, int *out_ok) {
    if (!step->active) {
        *out_ok = 0;
        return 0;
    }

    char chunk[512];
    ssize_t n;
    while ((n = read(step->fd, chunk, sizeof(chunk) - 1)) > 0) {
        chunk[n] = '\0';
        for (ssize_t i = 0; i < n; i++) {
            char c = chunk[i];
            if (c == '\n') {
                step->partial[step->partial_len] = '\0';
                log_push(step->partial);
                step->partial_len = 0;
            } else if (step->partial_len < LOG_LINE_MAX - 1) {
                step->partial[step->partial_len++] = c;
            }
        }
    }

    if (n == 0) {
        /* نهاية الأنبوب فعلياً - العملية خلصت */
        if (step->partial_len > 0) {
            step->partial[step->partial_len] = '\0';
            log_push(step->partial);
            step->partial_len = 0;
        }
        int status = pclose(step->pipe);
        step->active = 0;
        *out_ok = (status == 0);
        return 0;
    }

    /* n < 0: إما EAGAIN/EWOULDBLOCK (بلا بيانات جاهزة الآن - طبيعي
     * بقراءة غير حاجزة، نرجع "لسه شغالة") أو خطأ حقيقي آخر */
    if (errno != EAGAIN && errno != EWOULDBLOCK) {
        log_pushf("[exporter] shell read error: %s", strerror(errno));
        pclose(step->pipe);
        step->active = 0;
        *out_ok = 0;
        return 0;
    }

    return 1; /* لسه شغالة */
}

static void shell_step_cancel(shell_step_t *step) {
    if (step->active) {
        pclose(step->pipe);
        step->active = 0;
    }
}

/* ------------------------------------------------------------
 * حالة التصدير الجاري
 * ------------------------------------------------------------ */
typedef enum {
    STATE_IDLE,
    STATE_PREPARE_BUILD,   /* فوري - بلا أمر شل */
    STATE_BUILD,
    STATE_STRIP,           /* بس لو Release */
    STATE_COPY_OUTPUT,     /* فوري */
    STATE_SUCCESS,
    STATE_FAILED
} export_state_t;

static export_state_t g_state = STATE_IDLE;

static char g_exe_name[128];
static int  g_is_release;
static char g_output_dir[1024];

static char g_ps2dev_root[1536];    /* جاهز مسبقاً - rebax_toolchain_dir() */
static char g_build_dir[1536];      /* rebax_temp_export_dir() - مساحة عمل مؤقتة، تُنظَّف أول كل تصدير */
static char g_nodes_src_dir[1536];  /* rebax_node_sources_dir() - جاهز مسبقاً */

/* أنواع العقد المستخدمة فعلياً بمشاهد المشروع (بلا تكرار) */
#define MAX_USED_TYPES 64
static char g_used_types[MAX_USED_TYPES][64];
static int  g_used_types_count = 0;

/* مسارات الصور المطلقة (قيم خصائص is_asset_path فعلية) المستخدَمة
 * بمشاهد المشروع - بلا تكرار (نفس الصورة تُحوَّل مرة وحدة حتى لو
 * استخدمتها عدة عقد/مشاهد) - راجع convert_project_images أسفل */
#define MAX_IMAGE_PATHS 64
static char g_image_paths[MAX_IMAGE_PATHS][1024];
static int  g_image_paths_count = 0;

/* ------------------------------------------------------------
 * أدوات صغيرة مساعدة
 * ------------------------------------------------------------ */

static void add_unique(char list[][64], int *count, int max, const char *value) {
    for (int i = 0; i < *count; i++) {
        if (strcmp(list[i], value) == 0) return;
    }
    if (*count >= max) return; /* حد أقصى - تجاهل بصمت بدل تعطّل */
    strncpy(list[*count], value, 63);
    list[*count][63] = '\0';
    (*count)++;
}

static void add_unique_image_path(const char *value) {
    for (int i = 0; i < g_image_paths_count; i++) {
        if (strcmp(g_image_paths[i], value) == 0) return;
    }
    if (g_image_paths_count >= MAX_IMAGE_PATHS) return;
    strncpy(g_image_paths[g_image_paths_count], value, 1023);
    g_image_paths[g_image_paths_count][1023] = '\0';
    g_image_paths_count++;
}

/* يقرأ محتوى ملف كامل بذاكرة مخصَّصة (malloc) - المتصل يحرره.
 * NULL عند الفشل */
static char *read_whole_file(const char *path, long *out_size) {
    FILE *f = fopen(path, "rb");
    if (f == NULL) return NULL;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (size < 0) { fclose(f); return NULL; }

    char *buf = malloc((size_t)size + 1);
    if (buf == NULL) { fclose(f); return NULL; }

    size_t read_bytes = fread(buf, 1, (size_t)size, f);
    fclose(f);
    buf[read_bytes] = '\0';
    if (out_size) *out_size = (long)read_bytes;
    return buf;
}

/* يمسح مجلداً بحثاً عن ملفات ".rscene" بأي عمق - يستدعي callback
 * لكل ملف موجود (مسار كامل) */
static void walk_rscene_files(const char *dir_path, void (*on_file)(const char *path)) {
    DIR *dir = opendir(dir_path);
    if (dir == NULL) return;

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;

        char full_path[1536];
        snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, entry->d_name);

        struct stat st;
        if (stat(full_path, &st) != 0) continue;

        if (S_ISDIR(st.st_mode)) {
            walk_rscene_files(full_path, on_file);
        } else {
            size_t len = strlen(entry->d_name);
            if (len > 7 && strcmp(entry->d_name + len - 7, ".rscene") == 0) {
                on_file(full_path);
            }
        }
    }
    closedir(dir);
}

/* ------------------------------------------------------------
 * تحليل مشاهد المشروع وتوليد بيانات نمطية (scene_data.c) بدل نص
 * خام يُفسَّر وقت التشغيل - راجع تعليق التصميم بأعلى الملف. لكل
 * نسخة عقدة بأي مشهد: نقرأ ترتيب/نوع خصائصها الحقيقي من
 * node_registry_get() (نفس الشجرة اللي يستخدمها بانل الخصائص أصلاً)،
 * نأخذ قيمتها الفعلية من .rscene أو الافتراضية لو ما تغيّرت، ونكتبها
 * كقيمة C حرفية مكتوبة النوع (float/int/سلسلة) - الجسر الفعلي لتطبيقها
 * وقت التشغيل (property_offsets) موجود أصلاً بـnode_interface.h،
 * القارئ العام (node_instantiate بـscene_runtime.c) يستخدمه مباشرة
 * ------------------------------------------------------------ */

#include "node_registry.h"
#include "stb_image.h" /* التطبيق الفعلي معرَّف مرة وحدة بـicon_atlas.c فقط - هنا نستخدم الإعلانات بس (stbi_load) لتحويل صور المشروع وقت التصدير */

#define MAX_PROPS_PER_NODE 32

typedef struct {
    char key[64];
    char value[160];
} rscene_kv_t;

typedef struct {
    char type[64];
    char name[128];
    rscene_kv_t props[MAX_PROPS_PER_NODE];
    int prop_count;
} rscene_node_t;

#define MAX_NODES_PER_SCENE 128
static rscene_node_t g_parsed_nodes[MAX_NODES_PER_SCENE];
static int g_parsed_node_count = 0;

/* يحلل محتوى ملف .rscene كامل إلى مصفوفة g_parsed_nodes - يتبع
 * صيغة scene_tree_panel_serialize بالضبط ("[node N]" يبدأ كل كتلة،
 * type=/name=/parent=/prop:key=value بعدها) */
static void parse_rscene_content(char *content) {
    g_parsed_node_count = 0;
    rscene_node_t *cur = NULL;

    char *line = strtok(content, "\n");
    while (line != NULL) {
        if (strncmp(line, "[node ", 6) == 0) {
            if (g_parsed_node_count < MAX_NODES_PER_SCENE) {
                cur = &g_parsed_nodes[g_parsed_node_count++];
                cur->type[0] = '\0';
                cur->name[0] = '\0';
                cur->prop_count = 0;
            } else {
                cur = NULL;
            }
        } else if (cur != NULL) {
            if (strncmp(line, "type=", 5) == 0) {
                strncpy(cur->type, line + 5, sizeof(cur->type) - 1);
            } else if (strncmp(line, "name=", 5) == 0) {
                strncpy(cur->name, line + 5, sizeof(cur->name) - 1);
            } else if (strncmp(line, "prop:", 5) == 0) {
                const char *eq = strchr(line + 5, '=');
                if (eq != NULL && cur->prop_count < MAX_PROPS_PER_NODE) {
                    rscene_kv_t *kv = &cur->props[cur->prop_count++];
                    size_t key_len = (size_t)(eq - (line + 5));
                    if (key_len >= sizeof(kv->key)) key_len = sizeof(kv->key) - 1;
                    memcpy(kv->key, line + 5, key_len);
                    kv->key[key_len] = '\0';
                    strncpy(kv->value, eq + 1, sizeof(kv->value) - 1);
                    kv->value[sizeof(kv->value) - 1] = '\0';
                }
            }
        }
        line = strtok(NULL, "\n");
    }
}

static const rscene_kv_t *find_prop(const rscene_node_t *node, const char *key) {
    for (int i = 0; i < node->prop_count; i++) {
        if (strcmp(node->props[i].key, key) == 0) return &node->props[i];
    }
    return NULL;
}

static const node_registry_entry_t *find_registry_entry_by_name(const char *name) {
    int count = node_registry_count();
    for (int i = 0; i < count; i++) {
        const node_registry_entry_t *entry = node_registry_get_by_index(i);
        if (entry != NULL && strcmp(entry->name, name) == 0) return entry;
    }
    return NULL;
}

/* اسم متغيّر node_interface_t اللي كل ملف عقدة يصرّح عنه - نفس
 * الاصطلاح المستخدَم فعلياً بكل ملفات src/nodes (sprite2d.c →
 * sprite2d_interface، element_2d.c → element2d_interface): اسم
 * النوع بأحرف صغيرة بلا أي فاصل مُضاف */
static void interface_symbol_name(const char *type_name, char *out, size_t out_size) {
    size_t i = 0;
    for (; type_name[i] != '\0' && i < out_size - 11; i++) {
        out[i] = (char)tolower((unsigned char)type_name[i]);
    }
    out[i] = '\0';
    strncat(out, "_interface", out_size - strlen(out) - 1);
}

/* دقة شاشة بلاي ستيشن 2 القياسية (NTSC غير متشابكة) - محور العالم
 * بالمحرك (0,0) يقابل بالضبط منتصف هذي الدقة. تحويل عام بلا أي
 * معرفة بنوع العقدة - أي خاصية مستقبلية بأي عقدة اسمها "Position X"/
 * "Position Y" بالضبط تستفيد تلقائياً بلا أي تعديل هنا */
#define PS2_SCREEN_WIDTH  640.0f
#define PS2_SCREEN_HEIGHT 448.0f

/* يكتب قيمة خاصية واحدة كحرفية C مكتوبة النوع - النص (سلاسل) يُهرَّب
 * تحسباً لأي علامة اقتباس/شرطة مائلة عكسية بمسار ملف. property_name
 * يقرر تحويل إحداثيات المحرك (محور العالم بمنتصف الشاشة) لإحداثيات
 * بكسل PS2 الحقيقية (الزاوية العليا اليسرى) - بس لخاصيتي الموضع
 * بالضبط، بلا أي تحويل لأي خاصية ثانية (الحجم/التحجيم تُكتب كما هي
 * حرفياً، القيمة النهائية الجاهزة كودها المشترك يفسّرها كمركز عقدة
 * لا كزاوية - راجع sprite2d_draw) */
static void write_c_literal(FILE *f, node_property_type_t type, const char *string_value,
                             const node_property_t *default_prop, const char *property_name) {
    switch (type) {
        case NODE_PROPERTY_TYPE_FLOAT: {
            float v = (string_value != NULL) ? (float)atof(string_value)
                      : (default_prop != NULL ? default_prop->default_value.f : 0.0f);
            if (property_name != NULL && strcmp(property_name, "Position X") == 0) {
                v += PS2_SCREEN_WIDTH / 2.0f;
            } else if (property_name != NULL && strcmp(property_name, "Position Y") == 0) {
                v += PS2_SCREEN_HEIGHT / 2.0f;
            }
            fprintf(f, "{ .f = %.6ff }", v);
            break;
        }
        case NODE_PROPERTY_TYPE_INT: {
            int v = (string_value != NULL) ? atoi(string_value)
                     : (default_prop != NULL ? default_prop->default_value.i : 0);
            fprintf(f, "{ .i = %d }", v);
            break;
        }
        case NODE_PROPERTY_TYPE_STRING: {
            const char *s = string_value != NULL ? string_value
                             : (default_prop != NULL && default_prop->default_value.s != NULL
                                ? default_prop->default_value.s : "");
            fputs("{ .s = \"", f);
            for (const char *p = s; *p != '\0'; p++) {
                if (*p == '"' || *p == '\\') fputc('\\', f);
                fputc(*p, f);
            }
            fputs("\" }", f);
            break;
        }
    }
}

/* المجموعة الفريدة من أسماء أنواع فعلياً كُتبت لها extern declarations
 * (بلا تكرار الإعلان لنفس النوع أكثر من مرة بملف scene_data.c) */
static char g_declared_interfaces[MAX_USED_TYPES][64];
static int g_declared_interfaces_count = 0;

static int ensure_interface_declared(FILE *f, const char *type_name) {
    char symbol[75];
    interface_symbol_name(type_name, symbol, sizeof(symbol));
    for (int i = 0; i < g_declared_interfaces_count; i++) {
        if (strcmp(g_declared_interfaces[i], symbol) == 0) return 1;
    }
    if (g_declared_interfaces_count >= MAX_USED_TYPES) return 0;
    fprintf(f, "extern const node_interface_t %s;\n", symbol);
    strncpy(g_declared_interfaces[g_declared_interfaces_count], symbol, 63);
    g_declared_interfaces_count++;
    return 1;
}

/* الملف الرئيسي المولَّد (scene_data.c) - عمودان: extern declarations
 * + مصفوفات القيم (تُكتب أول بأول بينما نحلل)، ثم أخيراً جدول
 * المشاهد g_scenes[] (نكتبه لملف منفصل مؤقت ونلحقه بالنهاية، لأنه
 * يحتاج يشاور على كل مصفوفات التبويبات اللي لسه ما كُتبت وقت
 * معالجة أول ملف) */
static FILE *g_codegen_main = NULL;   /* extern decls + مصفوفات القيم وعقد كل مشهد */
static FILE *g_codegen_table = NULL;  /* أسطر جدول g_scenes[] فقط - تُلحق بالنهاية */

static void generate_scene_from_file(const char *path) {
    if (g_codegen_main == NULL || g_codegen_table == NULL) return;

    long size = 0;
    char *content = read_whole_file(path, &size);
    if (content == NULL) return;

    const char *slash = strrchr(path, '/');
    const char *base = (slash != NULL) ? slash + 1 : path;
    char scene_name[128];
    strncpy(scene_name, base, sizeof(scene_name) - 1);
    scene_name[sizeof(scene_name) - 1] = '\0';
    char *dot = strrchr(scene_name, '.');
    if (dot != NULL) *dot = '\0';
    /* اسم صالح كمعرّف C - أي محرف غير أبجدي رقمي يصير "_" */
    for (char *p = scene_name; *p != '\0'; p++) {
        if (!((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || (*p >= '0' && *p <= '9'))) {
            *p = '_';
        }
    }

    parse_rscene_content(content);

    if (g_parsed_node_count == 0) {
        free(content);
        return;
    }

    int valid_node_indices[MAX_NODES_PER_SCENE];
    int valid_count = 0;

    for (int i = 0; i < g_parsed_node_count; i++) {
        rscene_node_t *node = &g_parsed_nodes[i];
        const node_registry_entry_t *entry = find_registry_entry_by_name(node->type);
        if (entry == NULL) {
            log_pushf("[exporter] WARNING: node type '%s' in scene '%s' not found in registry - skipped.",
                      node->type, scene_name);
            continue;
        }

        add_unique(g_used_types, &g_used_types_count, MAX_USED_TYPES, node->type);

        ensure_interface_declared(g_codegen_main, node->type);

        if (entry->property_count > 0) {
            fprintf(g_codegen_main, "static const node_property_value_t %s_%d_props[] = {\n",
                    scene_name, i);
            for (int p = 0; p < entry->property_count; p++) {
                const node_property_t *prop = &entry->properties[p];
                const rscene_kv_t *kv = find_prop(node, prop->name);
                fprintf(g_codegen_main, "    ");
                write_c_literal(g_codegen_main, prop->type, kv != NULL ? kv->value : NULL, prop, prop->name);
                fprintf(g_codegen_main, ", /* %s */\n", prop->name);

                /* خاصية مسار أصل (صورة حالياً) بقيمة فعلية - تُجمَع
                 * لتحويلها لبكسلات خام مُضمَّنة (راجع convert_project_images) */
                if (prop->is_asset_path && kv != NULL && kv->value[0] != '\0') {
                    add_unique_image_path(kv->value);
                }
            }
            fprintf(g_codegen_main, "};\n\n");
        }

        valid_node_indices[valid_count++] = i;
    }

    if (valid_count == 0) {
        free(content);
        return;
    }

    fprintf(g_codegen_main, "static const scene_node_entry_t %s_nodes[] = {\n", scene_name);
    for (int v = 0; v < valid_count; v++) {
        int i = valid_node_indices[v];
        rscene_node_t *node = &g_parsed_nodes[i];
        const node_registry_entry_t *entry = find_registry_entry_by_name(node->type);
        char symbol[75];
        interface_symbol_name(node->type, symbol, sizeof(symbol));
        if (entry->property_count > 0) {
            fprintf(g_codegen_main, "    { &%s, %s_%d_props },\n", symbol, scene_name, i);
        } else {
            fprintf(g_codegen_main, "    { &%s, NULL },\n", symbol);
        }
    }
    fprintf(g_codegen_main, "};\n\n");

    fprintf(g_codegen_table, "    { \"%s\", %s_nodes, %d },\n", scene_name, scene_name, valid_count);

    free(content);
}

/* ------------------------------------------------------------
 * قوالب ثابتة (لا تتغيّر حسب المشروع) - تُكتب كما هي كل تصدير:
 *
 * scene_runtime.h - يعرّف node_property_value_t/scene_node_entry_t/
 * scene_table_entry_t (المستخدمة من scene_data.c المولَّد فوق) +
 * توقيع node_instantiate.
 *
 * scene_runtime.c - الجسر العام الوحيد: يطبّق قيم node_property_value_t
 * على عقدة فعلية بالذاكرة عبر property_offsets (بلا أي معرفة بتفاصيل
 * أي نوع عقدة على حدة)، ثم ينادي iface->init عليها.
 *
 * scene_loader_main.c - نقطة الدخول الحقيقية: يهيّئ GS (عبر
 * engine_get_gs_global)، ينشئ كل عقدة فعلياً (malloc + init حقيقي)،
 * ثم حلقة لعبة حقيقية: update ثم draw كل إطار لكل عقدة، بنفس تسلسل
 * gsKit الرسمي (gsKit_clear → رسم → gsKit_sync_flip → gsKit_queue_exec)
 * ------------------------------------------------------------ */
static const char *SCENE_RUNTIME_HEADER =
    "#ifndef SCENE_RUNTIME_H\n"
    "#define SCENE_RUNTIME_H\n"
    "\n"
    "#include \"node_interface.h\"\n"
    "\n"
    "typedef union {\n"
    "    float f;\n"
    "    int i;\n"
    "    const char *s;\n"
    "} node_property_value_t;\n"
    "\n"
    "typedef struct {\n"
    "    const node_interface_t *iface;\n"
    "    const node_property_value_t *values; /* طولها iface->property_count - NULL لو صفر */\n"
    "} scene_node_entry_t;\n"
    "\n"
    "typedef struct {\n"
    "    const char *name;\n"
    "    const scene_node_entry_t *nodes;\n"
    "    int node_count;\n"
    "} scene_table_entry_t;\n"
    "\n"
    "extern const scene_table_entry_t g_scenes[];\n"
    "extern const int g_scene_count;\n"
    "\n"
    "void *node_instantiate(const node_interface_t *iface, const node_property_value_t *values);\n"
    "\n"
    "#endif\n";

static const char *SCENE_RUNTIME_SOURCE =
    "#include <stdlib.h>\n"
    "#include <string.h>\n"
    "#include \"scene_runtime.h\"\n"
    "\n"
    "void *node_instantiate(const node_interface_t *iface, const node_property_value_t *values) {\n"
    "    void *instance = malloc(iface->instance_size);\n"
    "    if (instance == NULL) return NULL;\n"
    "    memset(instance, 0, iface->instance_size);\n"
    "\n"
    "    for (int i = 0; i < iface->property_count; i++) {\n"
    "        size_t off = iface->property_offsets[i];\n"
    "        char *field = (char *)instance + off;\n"
    "        switch (iface->properties[i].type) {\n"
    "            case NODE_PROPERTY_TYPE_FLOAT: *(float *)field = values[i].f; break;\n"
    "            case NODE_PROPERTY_TYPE_INT:   *(int *)field   = values[i].i; break;\n"
    "            case NODE_PROPERTY_TYPE_STRING: *(const char **)field = values[i].s; break;\n"
    "        }\n"
    "    }\n"
    "\n"
    "    if (iface->init != NULL) iface->init(instance);\n"
    "    return instance;\n"
    "}\n";

/* تنفيذ حقيقي لـengine_get_gs_global - يهيّئ gsKit مرة وحدة (نفس
 * تسلسل التهيئة الرسمي بالضبط - راجع examples/textures/textures.c
 * المرفَق: gsKit_init_global → إعداد PSM/PSMZ → dmaKit_init +
 * dmaKit_chan_init → gsKit_init_screen → gsKit_mode_switch) ويرجّع
 * نفس المؤشر لكل استدعاء تالٍ. يُكتب فقط لو عقدة تحتاج GS فعلياً
 * (تتضمّن engine_context.h) - لا علاقة لها بـscene_runtime.c العام */
static const char *ENGINE_CONTEXT_IMPL_SOURCE =
    "/* مولَّد تلقائياً وقت التصدير - تهيئة GS حقيقية، مرة وحدة */\n"
    "#include <stddef.h>\n"
    "#include <gsKit.h>\n"
    "#include <dmaKit.h>\n"
    "#include \"engine_context.h\"\n"
    "\n"
    "static GSGLOBAL *g_gs_global = NULL;\n"
    "static int g_initialized = 0;\n"
    "\n"
    "GSGLOBAL *engine_get_gs_global(void) {\n"
    "    if (g_initialized) return g_gs_global;\n"
    "    g_initialized = 1;\n"
    "\n"
    "    g_gs_global = gsKit_init_global();\n"
    "    g_gs_global->PSM = GS_PSM_CT24;\n"
    "    g_gs_global->PSMZ = GS_PSMZ_16S;\n"
    "\n"
    "    dmaKit_init(D_CTRL_RELE_OFF, D_CTRL_MFD_OFF, D_CTRL_STS_UNSPEC,\n"
    "                D_CTRL_STD_OFF, D_CTRL_RCYC_8, 1 << DMA_CHANNEL_GIF);\n"
    "    dmaKit_chan_init(DMA_CHANNEL_GIF);\n"
    "\n"
    "    gsKit_init_screen(g_gs_global);\n"
    "    gsKit_mode_switch(g_gs_global, GS_PERSISTENT);\n"
    "\n"
    "    return g_gs_global;\n"
    "}\n";

/* نقطة الدخول الحقيقية - تهيّئ كل عقد كل المشاهد مرة وحدة (node_instantiate
 * الجسر العام)، ثم تدخل حلقة اللعبة الفعلية: تحديث ثم رسم كل عقدة كل
 * إطار، بنفس تسلسل textures.c الرسمي (gsKit_clear → رسم → gsKit_sync_flip
 * → gsKit_queue_exec). دلتا الوقت ثابتة مؤقتاً (1/60) لحد ما نضيف قياس
 * وقت حقيقي - خطوة قادمة منفصلة، ما تكسر أي منطق موجود لما تُضاف */
static const char *SCENE_LOADER_TEMPLATE =
    "#include <gsKit.h>\n"
    "#include \"scene_runtime.h\"\n"
    "#include \"engine_context.h\"\n"
    "\n"
    "#define MAX_ACTIVE_NODES 256\n"
    "\n"
    "int main(void) {\n"
    "    GSGLOBAL *gsGlobal = engine_get_gs_global();\n"
    "\n"
    "    static void *active_instances[MAX_ACTIVE_NODES];\n"
    "    static const node_interface_t *active_ifaces[MAX_ACTIVE_NODES];\n"
    "    int active_count = 0;\n"
    "\n"
    "    for (int s = 0; s < g_scene_count; s++) {\n"
    "        const scene_table_entry_t *scene = &g_scenes[s];\n"
    "        for (int n = 0; n < scene->node_count && active_count < MAX_ACTIVE_NODES; n++) {\n"
    "            const scene_node_entry_t *entry = &scene->nodes[n];\n"
    "            void *instance = node_instantiate(entry->iface, entry->values);\n"
    "            if (instance != NULL) {\n"
    "                active_instances[active_count] = instance;\n"
    "                active_ifaces[active_count] = entry->iface;\n"
    "                active_count++;\n"
    "            }\n"
    "        }\n"
    "    }\n"
    "\n"
    "    while (1) {\n"
    "        gsKit_clear(gsGlobal, GS_SETREG_RGBAQ(0x00, 0x00, 0x00, 0x00, 0x00));\n"
    "\n"
    "        for (int i = 0; i < active_count; i++) {\n"
    "            if (active_ifaces[i]->update != NULL) active_ifaces[i]->update(active_instances[i], 1.0f / 60.0f);\n"
    "        }\n"
    "        for (int i = 0; i < active_count; i++) {\n"
    "            if (active_ifaces[i]->draw != NULL) active_ifaces[i]->draw(active_instances[i]);\n"
    "        }\n"
    "\n"
    "        gsKit_sync_flip(gsGlobal);\n"
    "        gsKit_queue_exec(gsGlobal);\n"
    "    }\n"
    "\n"
    "    return 0;\n"
    "}\n";

/* ------------------------------------------------------------
 * يمسح g_nodes_src_dir بحثاً عن أسماء الملفات المطابقة لأنواع
 * العقد المستخدمة (سطر "@NODE ... name=<X>" بكل ملف .c) - وينسخ
 * الملف (+ .h المطابق لو موجود) لمجلد بناء التصدير. يرجع 1 لو
 * كل الأنواع المستخدمة انطابقت بملف فعلي، 0 لو نوع واحد ع الأقل
 * ما له ملف (خطأ حقيقي - عقدة بمشروع بلا سورس تصدير لها) */
static int g_needs_image_loader = 0; /* أي عقدة مطابقة تتضمّن "image_loader.h" فعلياً - بلا ربط بأي اسم عقدة محدَّد */
static int g_needs_engine_context_stub = 0; /* أي عقدة مطابقة تتضمّن "engine_context.h" فعلياً */

/* المكتبات/مسارات include الإضافية (غير الأساسية) اللي ملفات العقد
 * المنسوخة نفسها تصرّح عنها بتعليق @PS2_EXPORT_LIBS/@PS2_EXPORT_INCLUDES
 * بأي مكان بملفها - بلا أي معرفة بالمكتبة أو اسم العقدة بكود المصدّر
 * نفسه. هذا يعني: عقدة مستقبلية تحتاج مكتبة ps2sdk جديدة (صوت، شبكة...)
 * تصرّح عن حاجتها بملفها هي بس - المصدّر ما يحتاج أي تعديل إطلاقاً */
#define MAX_EXTRA_FLAGS 32
static char g_extra_libs[MAX_EXTRA_FLAGS][160];
static int g_extra_libs_count = 0;
static char g_extra_incs[MAX_EXTRA_FLAGS][160];
static int g_extra_incs_count = 0;

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
    char *content = read_whole_file(path, &size);
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

static int copy_matched_node_sources(const char *dest_src_dir) {
    DIR *dir = opendir(g_nodes_src_dir);
    if (dir == NULL) {
        log_pushf("[exporter] failed to open extracted node sources: %s", g_nodes_src_dir);
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
        char *content = read_whole_file(full_path, &size);
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


/* ------------------------------------------------------------
 * يكتب Makefile مشروع التصدير - يستخدم قواعد بناء ps2sdk الرسمية
 * كما هي (Makefile.pref + Makefile.eeglobal)، بلا إعادة اختراع أي
 * قاعدة بناء بأنفسنا */
/* ------------------------------------------------------------
 * يحوّل كل صور المشروع المجمَّعة بـg_image_paths (مسارات مطلقة على
 * جهاز التطوير) لبكسلات RGBA8 خام (نفس تنسيق GS_PSM_CT32 المتوقَّع
 * من gsKit_texture_finish مباشرة، بلا أي swizzling - تحويل الـDMA
 * وقت الرفع يتولى تنسيق VRAM الداخلي بنفسه) عبر stb_image (نفس
 * المكتبة المستخدَمة أصلاً لأطلس الأيقونات بالمحرر)، ويكتبها
 * كمصفوفات C خام (نفس أسلوب bin2c بمشاريع PS2 اليدوية الحقيقية -
 * راجع تعليق التصميم بأعلى الملف) + جدول بحث بالاسم
 * (embedded_image_find) بملفي embedded_images.h/.c بمجلد البناء.
 *
 * يرجع 1 لو نجح تحويل كل الصور (أو ما فيه صور أصلاً)، 0 لو صورة
 * واحدة ع الأقل فشل تحويلها (مسار غير موجود، صيغة غير مدعومة...) */
static int convert_project_images(const char *dest_src_dir) {
    char header_path[1700];
    snprintf(header_path, sizeof(header_path), "%s/embedded_images.h", dest_src_dir);
    FILE *hf = fopen(header_path, "w");
    if (hf == NULL) return 0;

    fputs("/* مولَّد تلقائياً وقت التصدير من صور المشروع - لا تعدّله يدوياً */\n"
          "#ifndef EMBEDDED_IMAGES_H\n#define EMBEDDED_IMAGES_H\n\n"
          "typedef struct {\n"
          "    const char *path;\n"
          "    int width;\n"
          "    int height;\n"
          "    const unsigned char *data; /* RGBA8 خام، width*height*4 بايت */\n"
          "} embedded_image_t;\n\n"
          "const embedded_image_t *embedded_image_find(const char *path);\n\n"
          "#endif\n", hf);
    fclose(hf);

    char source_path[1700];
    snprintf(source_path, sizeof(source_path), "%s/embedded_images.c", dest_src_dir);
    FILE *cf = fopen(source_path, "w");
    if (cf == NULL) return 0;

    fputs("/* مولَّد تلقائياً وقت التصدير من صور المشروع - لا تعدّله يدوياً */\n"
          "#include <string.h>\n#include <stddef.h>\n"
          "#include \"embedded_images.h\"\n\n", cf);

    int ok_count = 0;
    for (int i = 0; i < g_image_paths_count; i++) {
        int w, h, channels;
        unsigned char *pixels = stbi_load(g_image_paths[i], &w, &h, &channels, 4);
        if (pixels == NULL) {
            log_pushf("[exporter] WARNING: could not read/decode image: %s", g_image_paths[i]);
            continue;
        }

        fprintf(cf, "static const unsigned char image_%d_data[] = {\n", i);
        long total = (long)w * h * 4;
        for (long b = 0; b < total; b++) {
            fprintf(cf, "%u,", pixels[b]);
            if ((b % 20) == 19) fputc('\n', cf);
        }
        fputs("\n};\n\n", cf);

        stbi_image_free(pixels);
        ok_count++;
        log_pushf("[exporter] converted image: %s (%dx%d)", g_image_paths[i], w, h);
    }

    /* نعيد فتح كل صورة نجح تحويلها لمعرفة أبعادها وقت كتابة جدول
     * البحث (أبسط من تخزين الأبعاد بمصفوفة موازية أثناء الحلقة
     * فوق - stbi_load سريعة بما يكفي لصور المشروع الاعتيادية) */
    fprintf(cf, "static const embedded_image_t g_embedded_images[] = {\n");
    for (int i = 0; i < g_image_paths_count; i++) {
        int w, h, channels;
        unsigned char *pixels = stbi_load(g_image_paths[i], &w, &h, &channels, 4);
        if (pixels == NULL) continue;
        fprintf(cf, "    { \"%s\", %d, %d, image_%d_data },\n", g_image_paths[i], w, h, i);
        stbi_image_free(pixels);
    }
    fprintf(cf, "};\n\n");

    fputs("const embedded_image_t *embedded_image_find(const char *path) {\n"
          "    size_t count = sizeof(g_embedded_images) / sizeof(g_embedded_images[0]);\n"
          "    for (size_t i = 0; i < count; i++) {\n"
          "        if (strcmp(g_embedded_images[i].path, path) == 0) return &g_embedded_images[i];\n"
          "    }\n"
          "    return NULL;\n"
          "}\n", cf);

    fclose(cf);

    return (ok_count == g_image_paths_count);
}

static void write_project_makefile(const char *build_dir, const char *src_dir_name) {
    char path[1300];
    snprintf(path, sizeof(path), "%s/Makefile", build_dir);
    FILE *f = fopen(path, "w");
    if (f == NULL) return;

    fprintf(f, "# مولَّد تلقائياً وقت التصدير من Rebax_Engine - لا تعدّله يدوياً\n\n");
    fprintf(f, "EE_BIN = %s.elf\n", g_exe_name);
    fprintf(f, "EE_OBJS = scene_loader_main.o scene_runtime.o scene_data.o");

    DIR *dir = opendir(build_dir);
    if (dir != NULL) {
        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
            size_t len = strlen(entry->d_name);
            if (len > 2 && strcmp(entry->d_name + len - 2, ".c") == 0
                && strcmp(entry->d_name, "scene_loader_main.c") != 0
                && strcmp(entry->d_name, "scene_runtime.c") != 0
                && strcmp(entry->d_name, "scene_data.c") != 0) {
                char obj_name[128];
                strncpy(obj_name, entry->d_name, sizeof(obj_name) - 1);
                obj_name[sizeof(obj_name) - 1] = '\0';
                char *dot = strrchr(obj_name, '.');
                if (dot) strcpy(dot, ".o");
                fprintf(f, " %s", obj_name);
            }
        }
        closedir(dir);
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

/* ------------------------------------------------------------
 * الواجهة العامة
 * ------------------------------------------------------------ */

int ps2_export_start(const char *exe_name, int is_release, const char *output_dir) {
    if (g_state != STATE_IDLE && g_state != STATE_SUCCESS && g_state != STATE_FAILED) {
        return 0; /* فيه تصدير شغال أصلاً */
    }
    if (!current_project_is_open()) {
        return 0;
    }

    g_log_head = g_log_tail = g_log_count = 0;

    strncpy(g_exe_name, exe_name, sizeof(g_exe_name) - 1);
    g_exe_name[sizeof(g_exe_name) - 1] = '\0';
    g_is_release = is_release;
    strncpy(g_output_dir, output_dir, sizeof(g_output_dir) - 1);
    g_output_dir[sizeof(g_output_dir) - 1] = '\0';

    /* بيئة ps2dev وسورس العقد جاهزتان مسبقاً - استُخرجتا مرة وحدة
     * وقت أول تشغيل للمحرك نفسه (راجع rebax_paths.h) - هذا الملف ما
     * عاد يستخرج أي شيء بنفسه إطلاقاً، فقط يقرأ من مساراتهما الثابتة */
    /* rebax_toolchain_dir() يعيد مسار ps2dev نفسه؛ إضافة /ps2dev هنا
     * كانت تنتج مساراً خاطئاً من نوع toolchains/ps2dev/ps2dev. */
    strncpy(g_ps2dev_root, rebax_toolchain_dir(), sizeof(g_ps2dev_root) - 1);
    g_ps2dev_root[sizeof(g_ps2dev_root) - 1] = '\0';
    strncpy(g_nodes_src_dir, rebax_node_sources_dir(), sizeof(g_nodes_src_dir) - 1);
    g_nodes_src_dir[sizeof(g_nodes_src_dir) - 1] = '\0';

    /* مساحة عمل التصدير - مجلد ثابت بشجرة ريباكس (Temp/export)، مو
     * بجانب الملف التنفيذي ولا داخل مجلد المشروع - يضمن صلاحيات
     * كتابة موثوقة بكل بيئة (راجع rebax_paths.h لسبب هذا القرار) */
    strncpy(g_build_dir, rebax_temp_export_dir(), sizeof(g_build_dir) - 1);
    g_build_dir[sizeof(g_build_dir) - 1] = '\0';

    /* تنظيف كامل لمساحة العمل قبل أي شيء - بدون هذا، ملفات من محاولة
     * تصدير سابقة (مثلاً image_loader_*.c من تصميم قديم) تبقى قابعة
     * وتُلتقط تلقائياً بمسح write_project_makefile (أي .c موجود
     * يُضاف لـEE_OBJS)، فيُربَط شيء ما عاد موجوداً بمصادر العقد
     * الحالية. كل تصدير الآن يبدأ من مجلد فاضٍ تماماً */
    {
        if (!rebax_fs_mkdir_p(g_build_dir)) { g_state=STATE_FAILED; return 0; }
        DIR *old=opendir(g_build_dir);
        if (old) { struct dirent *e; while((e=readdir(old))) { if(!strcmp(e->d_name,".")||!strcmp(e->d_name,"..")) continue; char q[1600]; snprintf(q,sizeof(q),"%s/%s",g_build_dir,e->d_name); rebax_fs_remove_recursive(q); } closedir(old); }
    }

    g_used_types_count = 0;
    g_needs_image_loader = 0;
    g_needs_engine_context_stub = 0;
    g_extra_libs_count = 0;
    g_extra_incs_count = 0;
    g_image_paths_count = 0;

    log_pushf("[exporter] starting export: %s (%s)", g_exe_name, g_is_release ? "Release" : "Debug");
    g_state = STATE_PREPARE_BUILD;
    return 1;
}

void ps2_export_cancel(void) {
    shell_step_cancel(&g_step);
    if (g_state != STATE_IDLE) {
        log_push("[exporter] export cancelled by user.");
    }
    g_state = STATE_IDLE;
}

ps2_export_status_t ps2_export_get_status(void) {
    switch (g_state) {
        case STATE_IDLE:    return PS2_EXPORT_STATUS_IDLE;
        case STATE_SUCCESS: return PS2_EXPORT_STATUS_SUCCESS;
        case STATE_FAILED:  return PS2_EXPORT_STATUS_FAILED;
        default:            return PS2_EXPORT_STATUS_RUNNING;
    }
}

void ps2_export_update(void) {
    char cmd[3200];
    int ok;

    switch (g_state) {

        case STATE_IDLE:
        case STATE_SUCCESS:
        case STATE_FAILED:
            return;

        case STATE_PREPARE_BUILD: {
            log_push("[exporter] scanning project scenes and generating typed node data...");
            const char *project_root = current_project_get_path();

            char src_dir[1536];
            snprintf(src_dir, sizeof(src_dir), "%s/src", g_build_dir);
            if (!rebax_fs_mkdir_p(src_dir)) { g_state=STATE_FAILED; return; }

            /* قوالب ثابتة أولاً - node_interface.h محتاجها scene_runtime.h،
             * لازم تكون جاهزة قبل أي تحليل (بلا ترتيب فعلي مهم هنا،
             * بس أوضح قراءة) */
            char path_buf[1600];
            snprintf(path_buf, sizeof(path_buf), "%s/scene_runtime.h", src_dir);
            FILE *rf = fopen(path_buf, "w");
            if (rf != NULL) { fputs(SCENE_RUNTIME_HEADER, rf); fclose(rf); }

            snprintf(path_buf, sizeof(path_buf), "%s/scene_runtime.c", src_dir);
            rf = fopen(path_buf, "w");
            if (rf != NULL) { fputs(SCENE_RUNTIME_SOURCE, rf); fclose(rf); }

            snprintf(path_buf, sizeof(path_buf), "%s/scene_loader_main.c", src_dir);
            rf = fopen(path_buf, "w");
            if (rf != NULL) { fputs(SCENE_LOADER_TEMPLATE, rf); fclose(rf); }

            /* scene_data.c المولَّد - جزء "الأصلي" (extern decls +
             * مصفوفات القيم/العقد) يُكتب مباشرة بينما نحلل كل مشهد،
             * جدول g_scenes[] يُبنى بملف مؤقت منفصل (يحتاج يشاور على
             * كل مصفوفات المشاهد اللي لسه ما اكتملت وقت أول ملف) */
            char main_path[1600], table_path[1600];
            snprintf(main_path, sizeof(main_path), "%s/scene_data.c", src_dir);
            snprintf(table_path, sizeof(table_path), "%s/.scene_table_tmp", src_dir);

            g_codegen_main = fopen(main_path, "w");
            g_codegen_table = fopen(table_path, "w");
            if (g_codegen_main == NULL || g_codegen_table == NULL) {
                log_push("[exporter] FAILED: could not create scene_data.c.");
                g_state = STATE_FAILED;
                return;
            }

            fputs("/* مولَّد تلقائياً وقت التصدير من مشاهد المشروع - لا تعدّله يدوياً */\n"
                  "#include \"scene_runtime.h\"\n\n", g_codegen_main);

            g_declared_interfaces_count = 0;
            g_used_types_count = 0;

            walk_rscene_files(project_root, generate_scene_from_file);

            fclose(g_codegen_main);
            fclose(g_codegen_table);
            g_codegen_main = NULL;
            g_codegen_table = NULL;

            if (g_used_types_count == 0) {
                log_push("[exporter] FAILED: no valid nodes found in any project scene.");
                remove(table_path);
                g_state = STATE_FAILED;
                return;
            }
            for (int i = 0; i < g_used_types_count; i++) {
                log_pushf("[exporter] scene usage found: %s", g_used_types[i]);
            }

            /* إلحاق جدول g_scenes[] بنهاية scene_data.c - الآن كل
             * مصفوفات العقد المُشار لها منه مكتوبة وموجودة فوق */
            FILE *append_target = fopen(main_path, "a");
            FILE *table_src = fopen(table_path, "r");
            if (append_target != NULL && table_src != NULL) {
                fprintf(append_target, "const scene_table_entry_t g_scenes[] = {\n");
                char line_buf[256];
                while (fgets(line_buf, sizeof(line_buf), table_src) != NULL) {
                    fputs(line_buf, append_target);
                }
                fprintf(append_target, "};\nconst int g_scene_count = sizeof(g_scenes) / sizeof(g_scenes[0]);\n");
            }
            if (append_target != NULL) fclose(append_target);
            if (table_src != NULL) fclose(table_src);
            remove(table_path);

            if (!copy_matched_node_sources(src_dir)) {
                log_push("[exporter] FAILED: one or more used node types have no matching source file.");
                g_state = STATE_FAILED;
                return;
            }

            if (g_image_paths_count > 0) {
                log_push("[exporter] converting project images to embedded raw pixel data...");
                if (!convert_project_images(src_dir)) {
                    log_push("[exporter] FAILED: one or more image paths could not be read/decoded.");
                    g_state = STATE_FAILED;
                    return;
                }
            }

            if (g_needs_engine_context_stub) {
                char impl_path[1600];
                snprintf(impl_path, sizeof(impl_path), "%s/engine_context_impl.c", src_dir);
                FILE *sf = fopen(impl_path, "w");
                if (sf != NULL) { fputs(ENGINE_CONTEXT_IMPL_SOURCE, sf); fclose(sf); }
            }

            write_project_makefile(src_dir, ".");

            log_push("[exporter] build directory ready - starting compilation...");
            g_state = STATE_BUILD;
            return;
        }

        case STATE_BUILD: {
            if (!g_step.active) {
                char src_dir[1536];
                snprintf(src_dir, sizeof(src_dir), "%s/src", g_build_dir);

                /* أمر شل واحد يجمع كل شيء: تصدير متغيرات بيئة PS2SDK
                 * (لازم بنفس استدعاء الشل الواحد - راجع تعليق ps2_exporter.h)،
                 * تضمين مسار أدوات mips64r5900el-ps2-elf-* بـPATH، ثم make.
                 * scene_data.c الآن كود C عادي (بيانات نمطية، لا حاجة
                 * لأي objcopy خام - راجع تعليق التصميم بأعلى الملف) */
                setenv("PS2DEV",g_ps2dev_root,1);
                char sdk[1600],gskit[1600],pathv[3600];
                snprintf(sdk,sizeof(sdk),"%s/ps2sdk",g_ps2dev_root);
                snprintf(gskit,sizeof(gskit),"%s/gsKit",g_ps2dev_root);
                setenv("PS2SDK",sdk,1); setenv("GSKIT",gskit,1);
                snprintf(pathv,sizeof(pathv),"%s/bin:%s/ee/bin:%s/iop/bin:%s/bin:%s",g_ps2dev_root,g_ps2dev_root,g_ps2dev_root,sdk,getenv("PATH")?getenv("PATH"):"");
                setenv("PATH",pathv,1);
                snprintf(cmd,sizeof(cmd),"cd '%s' && '%s' 2>&1",src_dir,rebax_make_path());

                if (!shell_step_start(&g_step, cmd)) {
                    log_push("[exporter] FAILED: could not start build process.");
                    g_state = STATE_FAILED;
                    return;
                }
            }

            if (!shell_step_poll(&g_step, &ok)) {
                if (!ok) {
                    log_push("[exporter] FAILED: build failed - see output above for the exact error.");
                    g_state = STATE_FAILED;
                    return;
                }
                log_push("[exporter] build succeeded.");
                g_state = STATE_COPY_OUTPUT;
            }
            return;
        }

        case STATE_STRIP:
            /* نزع الرموز بالنسخة Release مضمَّن فعلياً بقاعدة "all:"
             * بالـMakefile المولَّد (EE_STRIP) - هذي الحالة غير
             * مستخدَمة حالياً، محجوزة لو احتجنا خطوة نزع منفصلة لاحقاً */
            g_state = STATE_COPY_OUTPUT;
            return;

        case STATE_COPY_OUTPUT: {
            char src_dir[1536];
            snprintf(src_dir, sizeof(src_dir), "%s/src", g_build_dir);

            if (!rebax_fs_mkdir_p(g_output_dir)) { g_state=STATE_FAILED; return; }

            char elf_src[1600], elf_dst[1600];
            snprintf(elf_src,sizeof(elf_src),"%s/%s.elf",src_dir,g_exe_name);
            snprintf(elf_dst,sizeof(elf_dst),"%s/%s.elf",g_output_dir,g_exe_name);
            if (!rebax_fs_copy_file(elf_src,elf_dst)) { g_state=STATE_FAILED; return; }

            log_pushf("[exporter] done - %s/%s.elf", g_output_dir, g_exe_name);
            g_state = STATE_SUCCESS;
            return;
        }
    }
}
