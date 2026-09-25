/*
 * ============================================================
 * node_registry.c
 * ============================================================
 * بلا أي بيانات مكتوبة يدوياً هنا إطلاقاً - كل شيء يجيها جاهزاً من
 * node_registry_generated.h (مولَّد وقت البناء من سطر @NODE بكل
 * ملف src/nodes (ملفات .c)). إضافة عقدة جديدة = ملف .c جديد بمجلد
 * src/nodes/ فيه سطر @NODE واحد - تظهر هنا تلقائياً بأول بناء،
 * بلا أي تعديل بهذا الملف.
 * ============================================================
 */

#include <stddef.h>

#include "node_registry.h"
#include "icon_names.h"
#include "node_registry_generated.h"

#define REGISTRY_COUNT NODE_REGISTRY_GENERATED_COUNT

const node_registry_entry_t *node_registry_get(node_type_t type) {
    for (int i = 0; i < REGISTRY_COUNT; i++) {
        if (g_node_registry_table[i].type == type) {
            return &g_node_registry_table[i];
        }
    }
    return NULL;
}

int node_registry_count(void) {
    return REGISTRY_COUNT;
}

const node_registry_entry_t *node_registry_get_by_index(int index) {
    if (index < 0 || index >= REGISTRY_COUNT) {
        return NULL;
    }
    return &g_node_registry_table[index];
}
