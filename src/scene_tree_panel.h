#ifndef SCENE_TREE_PANEL_H
#define SCENE_TREE_PANEL_H

#include "node_types.h"
#include "node_registry.h" /* node_property_value_t */

void scene_tree_panel_update(int x, int y, int w, int h);
void scene_tree_panel_draw(int x, int y, int w, int h);
void scene_tree_panel_shutdown(void);

/* يضيف عقدة جديدة لأسفل قائمة الشجرة - تُستدعى من add_node_dialog
 * بعد ضغط "Add". تخصّص لها فوراً مصفوفة قيم حقيقية بطول
 * property_count بجدول node_registry لنفس النوع، مُهيَّأة من نفس
 * القيم الافتراضية - عامة تماماً، تشتغل لأي عدد خصائص */
void scene_tree_panel_add_node(node_type_t type, const char *name);

/* فهرس العقدة المحددة حالياً بالشجرة، -1 لو ولا وحدة محددة - هذا
 * هو "المؤشر الفريد" الحقيقي (بدل الاسم النصي القديم) اللي
 * properties_panel تستخدمه للقراءة/الكتابة على نفس العقدة بالضبط */
int scene_tree_panel_get_selected_index(void);

/* عدد كل عقد المشهد الحالي - لأي كود يحتاج يلف على كلها (الفيوبورت
 * وقت الرسم مثلاً) */
int scene_tree_panel_get_node_count(void);

/* نوع العقدة بفهرس معين - NODE_TYPE_ELEMENT (بلا معنى) لو الفهرس
 * غير صالح */
node_type_t scene_tree_panel_get_type(int index);

/* اسم العقدة بفهرس معين - NULL لو الفهرس غير صالح */
const char *scene_tree_panel_get_name(int index);

/* مؤشر حقيقي (قابل للتعديل) لمصفوفة قيم عقدة بفهرس معين - طولها
 * بالضبط property_count بجدول node_registry لنفس نوعها. NULL لو
 * الفهرس غير صالح أو العقدة بلا خصائص */
node_property_value_t *scene_tree_panel_get_values(int index);

/* اختيار عقدة من أدوات التحرير، وتعديل خاصية رقمية مباشرة من الفيوبورت. */
void scene_tree_panel_select_index(int index);
void scene_tree_panel_set_float_property(int index, int property_index, float value);

/* يمسح كل عقد المشهد الحالي بالكامل (يحرر ذاكرتها صح) - يُستخدم
 * عند فتح مشهد جديد فاضي أو تحميل مشهد ثانٍ من ملف، بديل مؤقت
 * لهذا المشهد الحالي أول */
void scene_tree_panel_clear(void);

/* يكتب تمثيلاً نصياً كاملاً لكل عقد المشهد الحالي بصيغة .rscene -
 * فقط الخصائص اللي قيمتها الحالية تختلف عن افتراضي نوعها (بمقارنة
 * مع node_registry) تُكتب - نفس فلسفة "نسجّل بس اللي تغيّر" المتفق
 * عليها. يرجّع عدد البايتات المكتوبة فعلياً (بلا حرف النهاية الفارغ)،
 * أو -1 لو buffer_size ما يكفي (المحتوى بهالحالة غير مكتمل/غير موثوق) */
int scene_tree_panel_serialize(char *buffer, int buffer_size);

/* يقرأ تمثيلاً نصياً بصيغة .rscene (نفس مخرجات scene_tree_panel_serialize
 * بالضبط) ويستبدل به كل عقد المشهد الحالي بالكامل (يمسح القديم أولاً
 * - نفس scene_tree_panel_clear داخلياً). القيم غير المذكورة بالملف
 * (خاصية أُضيفت لنوع العقدة بعد ما اتحفظ الملف) تبقى على افتراضيها
 * تلقائياً - مطابقة بالاسم مو بالترتيب، فعقدة قديمة تُقرأ صح حتى لو
 * تغيّر ترتيب/عدد خصائص نوعها لاحقاً. يرجّع 1 لو نجح، 0 لو فشل
 * (صيغة تالفة، نوع عقدة غير معروف...) */
int scene_tree_panel_deserialize(const char *buffer);

#endif
