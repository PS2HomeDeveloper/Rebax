/* Native PS2 runtime/code-generation templates. */
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include "export_internal.h"

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
    "/* Weak fallback: a project-owned C/C++ main() overrides this entry point. */\n"
    "__attribute__((weak)) int main(void) {\n"
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


void export_codegen_write_runtime_files(const char *src_dir) {
    char path[1600];
    FILE *f;
    snprintf(path, sizeof(path), "%s/scene_runtime.h", src_dir);
    f = fopen(path, "w"); if (f) { fputs(SCENE_RUNTIME_HEADER, f); fclose(f); }
    snprintf(path, sizeof(path), "%s/scene_runtime.c", src_dir);
    f = fopen(path, "w"); if (f) { fputs(SCENE_RUNTIME_SOURCE, f); fclose(f); }
    snprintf(path, sizeof(path), "%s/scene_loader_main.c", src_dir);
    f = fopen(path, "w"); if (f) { fputs(SCENE_LOADER_TEMPLATE, f); fclose(f); }
}

void export_codegen_write_engine_context_impl(const char *src_dir) {
    char path[1600];
    snprintf(path, sizeof(path), "%s/engine_context_impl.c", src_dir);
    FILE *f = fopen(path, "w");
    if (f != NULL) { fputs(ENGINE_CONTEXT_IMPL_SOURCE, f); fclose(f); }
}

static void sdk_interface_symbol(const char *type_name, char *out, size_t out_size) {
    size_t i = 0;
    for (; type_name[i] != '\0' && i + 11 < out_size; i++) {
        out[i] = (char)tolower((unsigned char)type_name[i]);
    }
    out[i] = '\0';
    strncat(out, "_interface", out_size - strlen(out) - 1);
}

int export_codegen_write_sdk_files(const char *src_dir) {
    char path[1600];
    snprintf(path, sizeof(path), "%s/rebax_sdk.c", src_dir);
    FILE *f = fopen(path, "w");
    if (f == NULL) return 0;

    fputs("/* Generated Native Rebax SDK implementation. */\n"
          "#include <stdlib.h>\n#include <string.h>\n"
          "#include \"rebax_sdk.h\"\n\n", f);
    for (int i = 0; i < g_sdk_types_count; i++) {
        char symbol[96];
        sdk_interface_symbol(g_sdk_types[i], symbol, sizeof(symbol));
        fprintf(f, "extern const node_interface_t %s;\n", symbol);
    }
    fputs("\ntypedef struct { const char *name; const node_interface_t *iface; } rebax_sdk_entry_t;\n"
          "struct rebax_node { const node_interface_t *iface; void *instance; int destroyed; };\n\n"
          "static const rebax_sdk_entry_t g_entries[] = {\n", f);
    for (int i = 0; i < g_sdk_types_count; i++) {
        char symbol[96];
        sdk_interface_symbol(g_sdk_types[i], symbol, sizeof(symbol));
        fprintf(f, "    { \"%s\", &%s },\n", g_sdk_types[i], symbol);
    }
    fputs("};\n#define REBAX_SDK_ENTRY_COUNT ((int)(sizeof(g_entries) / sizeof(g_entries[0])))\n\n"
          "static char *rebax_sdk_strdup(const char *value) {\n"
          "    if (value == NULL) value = \"\";\n"
          "    size_t n = strlen(value) + 1;\n"
          "    char *copy = (char *)malloc(n);\n"
          "    if (copy != NULL) memcpy(copy, value, n);\n"
          "    return copy;\n"
          "}\n\n"
          "static int property_index(const rebax_node_t *node, const char *name) {\n"
          "    if (node == NULL || node->iface == NULL || name == NULL) return -1;\n"
          "    for (int i = 0; i < node->iface->property_count; i++)\n"
          "        if (strcmp(node->iface->properties[i].name, name) == 0) return i;\n"
          "    return -1;\n"
          "}\n\n"
          "static rebax_result_t check_property(const rebax_node_t *node, const char *name,\n"
          "                                      node_property_type_t type, int *index) {\n"
          "    int i = property_index(node, name);\n"
          "    if (i < 0) return REBAX_ERROR_NOT_FOUND;\n"
          "    if (node->iface->properties[i].type != type) return REBAX_ERROR_WRONG_PROPERTY_TYPE;\n"
          "    if (index != NULL) *index = i;\n"
          "    return REBAX_OK;\n"
          "}\n\n"
          "const char *rebax_node_type_name(rebax_node_type_t type) {\n"
          "    switch (type) {\n"
          "        case REBAX_NODE_ELEMENT: return \"Element\";\n"
          "        case REBAX_NODE_ELEMENT_2D: return \"Element2D\";\n"
          "        case REBAX_NODE_ELEMENT_3D: return \"Element3D\";\n"
          "        case REBAX_NODE_SPRITE_2D: return \"Sprite2D\";\n"
          "        default: return NULL;\n"
          "    }\n}\n\n"
          "rebax_result_t rebax_node_create_by_name(const char *name, rebax_node_t **out_node) {\n"
          "    if (out_node == NULL || name == NULL) return REBAX_ERROR_INVALID_ARGUMENT;\n"
          "    *out_node = NULL;\n"
          "    const node_interface_t *iface = NULL;\n"
          "    for (int e = 0; e < REBAX_SDK_ENTRY_COUNT; e++)\n"
          "        if (strcmp(g_entries[e].name, name) == 0) { iface = g_entries[e].iface; break; }\n"
          "    if (iface == NULL) return REBAX_ERROR_UNSUPPORTED_NODE;\n"
          "    rebax_node_t *node = (rebax_node_t *)calloc(1, sizeof(*node));\n"
          "    if (node == NULL) return REBAX_ERROR_OUT_OF_MEMORY;\n"
          "    node->iface = iface; node->instance = calloc(1, iface->instance_size);\n"
          "    if (node->instance == NULL) { free(node); return REBAX_ERROR_OUT_OF_MEMORY; }\n"
          "    for (int p = 0; p < iface->property_count; p++) {\n"
          "        char *field = (char *)node->instance + iface->property_offsets[p];\n"
          "        const node_property_t *prop = &iface->properties[p];\n"
          "        if (prop->type == NODE_PROPERTY_TYPE_FLOAT) *(float *)field = prop->default_value.f;\n"
          "        else if (prop->type == NODE_PROPERTY_TYPE_INT) *(int *)field = prop->default_value.i;\n"
          "        else if (prop->type == NODE_PROPERTY_TYPE_STRING) {\n"
          "            *(const char **)field = rebax_sdk_strdup(prop->default_value.s);\n"
          "            if (*(const char **)field == NULL) { rebax_node_free(node); return REBAX_ERROR_OUT_OF_MEMORY; }\n"
          "        }\n"
          "    }\n"
          "    if (iface->init != NULL) iface->init(node->instance);\n"
          "    *out_node = node; return REBAX_OK;\n"
          "}\n\n"
          "rebax_result_t rebax_node_create(rebax_node_type_t type, rebax_node_t **out_node) {\n"
          "    const char *name = rebax_node_type_name(type);\n"
          "    return name != NULL ? rebax_node_create_by_name(name, out_node) : REBAX_ERROR_UNSUPPORTED_NODE;\n"
          "}\n\n"
          "int rebax_node_property_count(const rebax_node_t *node) { return node && node->iface ? node->iface->property_count : 0; }\n"
          "const char *rebax_node_property_name(const rebax_node_t *node, int index) { return node && node->iface && index >= 0 && index < node->iface->property_count ? node->iface->properties[index].name : NULL; }\n"
          "node_property_type_t rebax_node_property_type(const rebax_node_t *node, int index) { return node && node->iface && index >= 0 && index < node->iface->property_count ? node->iface->properties[index].type : NODE_PROPERTY_TYPE_STRING; }\n"
          "int rebax_node_find_property(const rebax_node_t *node, const char *name) { return property_index(node, name); }\n\n"
          "rebax_result_t rebax_node_set_float(rebax_node_t *node, const char *name, float value) { int i; rebax_result_t r = check_property(node, name, NODE_PROPERTY_TYPE_FLOAT, &i); if (r != REBAX_OK) return r; *(float *)((char *)node->instance + node->iface->property_offsets[i]) = value; return REBAX_OK; }\n"
          "rebax_result_t rebax_node_get_float(const rebax_node_t *node, const char *name, float *out_value) { int i; if (out_value == NULL) return REBAX_ERROR_INVALID_ARGUMENT; rebax_result_t r = check_property(node, name, NODE_PROPERTY_TYPE_FLOAT, &i); if (r != REBAX_OK) return r; *out_value = *(const float *)((const char *)node->instance + node->iface->property_offsets[i]); return REBAX_OK; }\n"
          "rebax_result_t rebax_node_set_int(rebax_node_t *node, const char *name, int value) { int i; rebax_result_t r = check_property(node, name, NODE_PROPERTY_TYPE_INT, &i); if (r != REBAX_OK) return r; *(int *)((char *)node->instance + node->iface->property_offsets[i]) = value; return REBAX_OK; }\n"
          "rebax_result_t rebax_node_get_int(const rebax_node_t *node, const char *name, int *out_value) { int i; if (out_value == NULL) return REBAX_ERROR_INVALID_ARGUMENT; rebax_result_t r = check_property(node, name, NODE_PROPERTY_TYPE_INT, &i); if (r != REBAX_OK) return r; *out_value = *(const int *)((const char *)node->instance + node->iface->property_offsets[i]); return REBAX_OK; }\n"
          "rebax_result_t rebax_node_set_string(rebax_node_t *node, const char *name, const char *value) { int i; rebax_result_t r = check_property(node, name, NODE_PROPERTY_TYPE_STRING, &i); if (r != REBAX_OK) return r; char *copy = rebax_sdk_strdup(value); if (copy == NULL) return REBAX_ERROR_OUT_OF_MEMORY; char **field = (char **)((char *)node->instance + node->iface->property_offsets[i]); free(*field); *field = copy; return REBAX_OK; }\n"
          "const char *rebax_node_get_string(const rebax_node_t *node, const char *name) { int i; if (check_property(node, name, NODE_PROPERTY_TYPE_STRING, &i) != REBAX_OK) return NULL; return *(const char **)((const char *)node->instance + node->iface->property_offsets[i]); }\n\n"
          "void rebax_node_update(rebax_node_t *node, float delta_time) { if (node && !node->destroyed && node->iface->update) node->iface->update(node->instance, delta_time); }\n"
          "void rebax_node_draw(rebax_node_t *node) { if (node && !node->destroyed && node->iface->draw) node->iface->draw(node->instance); }\n"
          "void rebax_node_destroy(rebax_node_t *node) { if (!node || node->destroyed) return; if (node->iface->destroy) node->iface->destroy(node->instance); for (int p = 0; p < node->iface->property_count; p++) if (node->iface->properties[p].type == NODE_PROPERTY_TYPE_STRING) { char **field = (char **)((char *)node->instance + node->iface->property_offsets[p]); free(*field); *field = NULL; } node->destroyed = 1; }\n"
          "void rebax_node_free(rebax_node_t *node) { if (!node) return; rebax_node_destroy(node); free(node->instance); free(node); }\n", f);
    fclose(f);
    return 1;
}
