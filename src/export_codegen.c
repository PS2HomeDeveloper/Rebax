/* Native PS2 runtime/code-generation templates. */
#include <stdio.h>
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
