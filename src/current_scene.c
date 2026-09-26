/*
 * ============================================================
 * current_scene.c
 * ============================================================
 * راجع current_scene.h للتوثيق الكامل.
 * ============================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "current_scene.h"
#include "scene_tree_panel.h"

/* حد أقصى لحجم نص المشهد المُصدَّر بايت - كافٍ جداً لمشاهد واقعية
 * حالياً (عشرات-مئات العقد). لو مشروع مستقبلاً احتاج مشاهد أضخم من
 * هذا، الحل تحويل السطر تحت لحلقة تكبير تدريجي بدل رقم ثابت - غير
 * لازم الآن */
#define SCENE_BUFFER_MAX (256 * 1024)

static char g_path[1024] = "";      /* فاضي = المشهد لسه ما اتحفظ */
static int g_dirty = 0;
static char g_display_name_buf[128] = "empty";

void current_scene_new(void) {
    scene_tree_panel_clear();
    g_path[0] = '\0';
    g_dirty = 0;
    strncpy(g_display_name_buf, "empty", sizeof(g_display_name_buf) - 1);
    g_display_name_buf[sizeof(g_display_name_buf) - 1] = '\0';
}

const char *current_scene_get_path(void) {
    return (g_path[0] != '\0') ? g_path : NULL;
}

int current_scene_is_dirty(void) {
    return g_dirty;
}

void current_scene_mark_dirty(void) {
    g_dirty = 1;
}

/* يستخرج اسم الملف بلا مساره ولا امتداده (مثال:
 * "/a/b/Player.rscene" → "Player") - لعرض اسم المشهد المحفوظ
 * بالتبويب بلا تطويل زائد */
static void extract_basename_no_ext(const char *path, char *out, size_t out_size) {
    const char *slash = strrchr(path, '/');
    const char *name_start = (slash != NULL) ? (slash + 1) : path;

    strncpy(out, name_start, out_size - 1);
    out[out_size - 1] = '\0';

    char *dot = strrchr(out, '.');
    if (dot != NULL) *dot = '\0';
}

const char *current_scene_get_display_name(void) {
    if (g_path[0] != '\0') {
        extract_basename_no_ext(g_path, g_display_name_buf, sizeof(g_display_name_buf));
        return g_display_name_buf;
    }

    if (scene_tree_panel_get_node_count() > 0) {
        const char *first_node_name = scene_tree_panel_get_name(0);
        if (first_node_name != NULL && first_node_name[0] != '\0') {
            strncpy(g_display_name_buf, first_node_name, sizeof(g_display_name_buf) - 1);
            g_display_name_buf[sizeof(g_display_name_buf) - 1] = '\0';
            return g_display_name_buf;
        }
    }

    strncpy(g_display_name_buf, "empty", sizeof(g_display_name_buf) - 1);
    g_display_name_buf[sizeof(g_display_name_buf) - 1] = '\0';
    return g_display_name_buf;
}

int current_scene_save_as(const char *path) {
    static char buffer[SCENE_BUFFER_MAX];

    int written = scene_tree_panel_serialize(buffer, sizeof(buffer));
    if (written < 0) return 0; /* المشهد أكبر من SCENE_BUFFER_MAX، أو خطأ تنسيق */

    FILE *f = fopen(path, "wb");
    if (f == NULL) return 0;

    size_t actually_written = fwrite(buffer, 1, (size_t)written, f);
    fclose(f);
    if (actually_written != (size_t)written) return 0;

    strncpy(g_path, path, sizeof(g_path) - 1);
    g_path[sizeof(g_path) - 1] = '\0';
    g_dirty = 0;
    return 1;
}

int current_scene_save(void) {
    if (g_path[0] == '\0') return 0; /* ما اتسمّى مسار بعد - استخدم save_as */
    return current_scene_save_as(g_path);
}

int current_scene_load(const char *path) {
    FILE *f = fopen(path, "rb");
    if (f == NULL) return 0;

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (size < 0 || size >= SCENE_BUFFER_MAX) { fclose(f); return 0; }

    char *buffer = malloc((size_t)size + 1);
    if (buffer == NULL) { fclose(f); return 0; }

    size_t read_count = fread(buffer, 1, (size_t)size, f);
    fclose(f);
    buffer[read_count] = '\0';

    int ok = scene_tree_panel_deserialize(buffer);
    free(buffer);
    if (!ok) return 0;

    strncpy(g_path, path, sizeof(g_path) - 1);
    g_path[sizeof(g_path) - 1] = '\0';
    g_dirty = 0;
    return 1;
}

void current_scene_restore_state(const char *path, int dirty) {
    if (path != NULL) {
        strncpy(g_path, path, sizeof(g_path) - 1);
        g_path[sizeof(g_path) - 1] = '\0';
    } else {
        g_path[0] = '\0';
    }
    g_dirty = dirty;
}
