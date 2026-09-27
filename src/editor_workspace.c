/*
 * ============================================================
 * editor_workspace.c
 * ============================================================
 * تخطيط الشاشة (بعد شريط الأدوات العلوي، غير محسوب هنا) - ثلاثة
 * أعمدة مستقلة تماماً، كل واحد يمتد بارتفاع كامل من تحت الشريط
 * العلوي لحد أسفل الشاشة، وينقسم لفوق/تحت بحده الخاص بلا أي تأثير
 * على الأعمدة الأخرى:
 *
 *   ┌─────────────┬───────────────────────┬─────────────┐
 *   │  LEFT_TOP   │                       │  RIGHT_TOP  │
 *   │             │       الكاميرا        │             │
 *   ├─────────────┤                       ├─────────────┤
 *   │ LEFT_BOTTOM ├───────────────────────┤RIGHT_BOTTOM │
 *   │             │     BOTTOM_CENTER     │             │
 *   └─────────────┴───────────────────────┴─────────────┘
 *
 * كل عمود له حد فاصل أفقي داخلي مستقل (يسار، وسط، يمين) - الحد
 * الفاصل بين الكاميرا والمنطقة السفلية محصور بعرض عمود الوسط فقط
 * (بالضبط زي الحرف H: عمودين جانبيين مستقلين، وخط أفقي بينهما في
 * عمود الوسط بس). بالإضافة لحدين عموديين بين الأعمدة الثلاثة.
 *
 * الكاميرا محمية بحد أدنى ثابت (حجم حقيقي، بلا انكماش أو تشويه) -
 * باقي الأربع مناطق بلا حد أدنى (تنكمش لصفر، تُمسك من الحافة).
 * ============================================================
 */

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "editor_workspace.h"
#include "window.h"
#include "ui_project_center.h" /* لأجل ألوان الثيم */
#include "scene_tree_panel.h"
#include "current_scene.h"
#include "scene_tabs.h"
#include "close_scene_dialog.h"
#include "asset_browser.h"
#include "add_node_dialog.h"
#include "file_system_panel.h"
#include "properties_panel.h"
#include "shape_provider.h"
#include "icon_atlas.h"
#include "export_dialog.h"
#include "font.h"
#include "viewport_2d.h"
#include "viewport_3d.h"

#define TOOLBAR_HEIGHT       26  /* الشريط العلوي بعرض الشاشة الكامل - مقلَّص ليطابق ارتفاع
                                   * أشرطة تبويبات البانلات (TAB_BAR_HEIGHT)، فتتمدد كل
                                   * البانلات لأعلى بالفرق (كان 48) */
#define TAB_BAR_HEIGHT       20  /* الشريط الرفيع أعلى كل بانل (لأسماء التبويبات لاحقاً) */
#define SPLITTER_THICKNESS    6  /* سماكة منطقة إمساك الحد الفاصل بالماوس */

/* فوق منطقة الكاميرا نفسها (عمود الوسط بس، مو الشاشة كاملة) شريط
 * واحد فقط - نفس ارتفاع TAB_BAR_HEIGHT بالضبط (يطابق صف أشرطة
 * تبويبات LEFT_TOP وRIGHT_TOP تماماً، بلا أي شريط مضاعف يقتطع من
 * ارتفاع الكاميرا). قسمه الأيسر: تبويبات تبديل نوع العرض
 * (3D/2D/Script) - بعدها خط فاصل، ثم أدوات الكاميرا (تحديد...) */
#define CAMERA_TOP_BAR_HEIGHT     TAB_BAR_HEIGHT

/* شريط أسماء المشاهد المفتوحة - فوق شريط تبديل العرض (3D/2D/Script)
 * مباشرة، بنفس ارتفاعه بالضبط (نسخة منه بالمكان القديم، بعد ما
 * تحرّك شريط تبديل العرض تحت بمقدار ارتفاعه هو نفسه). تبويب لكل
 * مشهد مفتوح بـscene_tabs - الاسم + نجمة لو غير محفوظ + زر "x"
 * إغلاق، وأيقونة "+" بأقصى يمين الشريط بالكامل لإنشاء تبويب جديد */
#define SCENE_TABS_BAR_HEIGHT     CAMERA_TOP_BAR_HEIGHT

#define CAMERA_MIN_W        200  /* الكاميرا وحدها لها حد أدنى - حجم حقيقي، بلا انكماش أو تشويه */
#define CAMERA_MIN_H        150

/* أزرق باهت للتحويم على أي عنصر تفاعلي (حد فاصل أو قسم بشريط
 * الأدوات) قبل الضغط عليه فعلياً - window_fill_rect ما يدعم شفافية
 * حقيقية، فهذا لون محسوب مباشرة (مزيج مرئي بين UI_COLOR_PANEL_BG
 * وUI_COLOR_BUTTON_BLUE) بدل أي محاولة شفافية فعلية */
#define UI_COLOR_HOVER_BLUE_R  0x2C
#define UI_COLOR_HOVER_BLUE_G  0x40
#define UI_COLOR_HOVER_BLUE_B  0x50

typedef enum {
    SPLITTER_NONE = -1,
    SPLITTER_LEFT_RIGHT_COL,   /* عمودي: بين اليسار والوسط */
    SPLITTER_CENTER_RIGHT_COL, /* عمودي: بين الوسط واليمين */
    SPLITTER_LEFT_INNER,       /* أفقي: داخل العمود الأيسر - مستقل تماماً */
    SPLITTER_RIGHT_INNER,      /* أفقي: داخل العمود الأيمن - مستقل تماماً */
    SPLITTER_CAMERA_BOTTOM     /* أفقي: بين الكاميرا وBOTTOM_CENTER - محصور بعرض عمود الوسط بس */
} splitter_id_t;

/* حالة التخطيط - كل القيم بالبكسل */
static int g_left_width           = 260;
static int g_right_width          = 300;
static int g_left_split_y         = 300; /* من أعلى العمود الأيسر (ارتفاعه الكامل مستقل) */
static int g_right_split_y        = 300; /* من أعلى العمود الأيمن (ارتفاعه الكامل مستقل) */
static int g_bottom_center_height = 220; /* ارتفاع BOTTOM_CENTER فقط - داخل عمود الوسط */

static splitter_id_t g_dragging = SPLITTER_NONE;
static splitter_id_t g_hovered_splitter = SPLITTER_NONE; /* يُعاد حسابه كل إطار - للتحويم الأزرق الباهت قبل السحب */

/* نوع الكاميرا المعروضة حالياً بمنطقة الكاميرا الوسطى - افتراضياً
 * 3D (لحين وجود منطق يحدد هذا تلقائياً حسب نوع المشهد المفتوح) */
static editor_camera_mode_t g_camera_mode = EDITOR_CAMERA_MODE_3D;

/* تبويبات تبديل العرض بالقسم الأيسر من الشريط أعلى الكاميرا - معرَّف
 * هنا مبكراً (قبل التصميم البصري لاحقاً بالملف) لأن الدالة الخارجية
 * editor_workspace_set_camera_mode تحتاجه فوراً تحتها */
typedef enum {
    CAMERA_VIEW_TAB_3D = 0,
    CAMERA_VIEW_TAB_2D,
    CAMERA_VIEW_TAB_SCRIPT,
    CAMERA_VIEW_TAB_COUNT
} camera_view_tab_id_t;

static camera_view_tab_id_t g_camera_view_tab_selected = CAMERA_VIEW_TAB_3D;

void editor_workspace_set_camera_mode(editor_camera_mode_t mode) {
    g_camera_mode = mode;
    /* يبقي تحديد تبويب العرض (3D/2D) متطابقاً بصرياً مع أي تغيير
     * برمجي خارجي لوضع الكاميرا - Script ليس ضمن هذا enum فلا يُمس */
    g_camera_view_tab_selected = (mode == EDITOR_CAMERA_MODE_3D) ? CAMERA_VIEW_TAB_3D : CAMERA_VIEW_TAB_2D;
}

/* ------------------------------------------------------------
 * القسم الأول (من اليسار) بشريط أدوات الكاميرا - أداة "تحديد"،
 * الأداة الافتراضية بأي محرك (زي Blender/Unity/Godot). هي الأداة
 * الوحيدة المبنية فعلياً حالياً - باقي الأقسام (تحريك/تدوير/تكبير)
 * تُضاف لاحقاً بنفس الآلية بالضبط (الحاوية مبنية لتستقبل أي عدد).
 * الخط الفاصل والتحويم/التحديد الأزرق حقيقيان وشغّالان من الآن.
 * ------------------------------------------------------------ */
#define CAMERA_TOOL_PADDING_X  14

typedef struct {
    window_texture_t *label_tex;
    int label_w, label_h;
    int ready;
} camera_tool_chrome_t;

static camera_tool_chrome_t g_tool_select = {0};


/* عرض القسم الأول (تحديد) بالبكسل - يعتمد على عرض نصه فعلياً +
 * حشو ثابت كل جهة، مو رقم ثابت اعتباطي */

/* ------------------------------------------------------------
 * تبويبات تبديل نوع العرض (3D / 2D / Script) - القسم الأيسر بالضبط
 * من الشريط الواحد أعلى الكاميرا. تحويم أزرق شفاف باهت، وتحديد
 * أزرق شفاف أغمق شوي - نفس اللون والشفافية المستخدمة تماماً بشجرة
 * العقد (UI_COLOR_BUTTON_BLUE بشفافية 0.25 للتحويم و0.55 للتحديد)،
 * مو الأزرق الصلب المستخدم بالحدود الفاصلة أو أداة "تحديد" المجاورة.
 * 3D هي الافتراضية عند فتح المحرك. الضغط على 3D أو 2D يبدّل محتوى
 * الكاميرا فعلياً (نفس g_camera_mode الموجود أصلاً). الضغط على
 * Script يحدد بصرياً بس - بلا أي تغيير بمحتوى الكاميرا، لعدم وجود
 * نظام ملفات سكربت فعلي بعد.
 * ------------------------------------------------------------ */
typedef struct {
    window_texture_t *label_tex;
    window_texture_t *hover_tex;
    window_texture_t *selected_tex;
    int label_w, label_h;
    int seg_w;
    int ready;
} camera_view_tab_chrome_t;

static camera_view_tab_chrome_t g_view_tabs[CAMERA_VIEW_TAB_COUNT] = {0};
static const char *g_view_tab_labels[CAMERA_VIEW_TAB_COUNT] = { "3D", "2D", "Script" };

static int g_view_tab_hovered = -1; /* -1 = لا تبويب محوَّم عليه حالياً */

/* نفس دالة إطفاء الشفافية المستخدمة بشجرة العقد (scene_tree_panel.c)
 * بالضبط - كل ملف عنده نسخته المستقلة بنفس أسلوب المشروع */
static void dim_alpha(shape_image_t *img, double factor) {
    if (img->pixels == NULL) return;
    int count = img->width * img->height;
    for (int i = 0; i < count; i++) {
        unsigned char *a = &img->pixels[i * 4 + 3];
        *a = (unsigned char)((double)(*a) * factor);
    }
}

static void ensure_view_tab_ready(camera_view_tab_chrome_t *chrome, const char *label) {
    if (chrome->ready) return;
    chrome->ready = 1;

    font_text_image_t txt = font_render_text(label, FONT_WEIGHT_REGULAR, 13);
    if (txt.pixels != NULL) {
        chrome->label_tex = window_create_texture(txt.pixels, txt.width, txt.height);
        chrome->label_w = txt.width;
        chrome->label_h = txt.height;
        font_free_text_image(&txt);
    }

    chrome->seg_w = chrome->label_w + CAMERA_TOOL_PADDING_X * 2;

    shape_image_t hov = shape_provider_render_rect(chrome->seg_w, CAMERA_TOP_BAR_HEIGHT, 0,
        UI_COLOR_BUTTON_BLUE.r, UI_COLOR_BUTTON_BLUE.g, UI_COLOR_BUTTON_BLUE.b);
    dim_alpha(&hov, 0.25);
    chrome->hover_tex = window_create_texture(hov.pixels, hov.width, hov.height);
    shape_provider_free_image(&hov);

    shape_image_t sel = shape_provider_render_rect(chrome->seg_w, CAMERA_TOP_BAR_HEIGHT, 0,
        UI_COLOR_BUTTON_BLUE.r, UI_COLOR_BUTTON_BLUE.g, UI_COLOR_BUTTON_BLUE.b);
    dim_alpha(&sel, 0.55);
    chrome->selected_tex = window_create_texture(sel.pixels, sel.width, sel.height);
    shape_provider_free_image(&sel);
}

static void ensure_all_view_tabs_ready(void) {
    for (int i = 0; i < CAMERA_VIEW_TAB_COUNT; i++) {
        ensure_view_tab_ready(&g_view_tabs[i], g_view_tab_labels[i]);
    }
}

/* المجموع الكلي لعرض التبويبات الثلاثة بالبكسل - يضمن أيضاً تهيئة
 * كل نصوصها وأبعادها أول استدعاء (بدون حاجة لاستدعاء منفصل) */


static void draw_camera_view_tabs(int x, int y) {
    ensure_all_view_tabs_ready();

    int tab_x = x;
    for (int i = 0; i < CAMERA_VIEW_TAB_COUNT; i++) {
        camera_view_tab_chrome_t *chrome = &g_view_tabs[i];
        if (chrome->label_tex == NULL) {
            tab_x += chrome->seg_w;
            continue;
        }

        if (i == (int)g_camera_view_tab_selected && chrome->selected_tex != NULL) {
            window_draw_texture(chrome->selected_tex, tab_x, y, chrome->seg_w, CAMERA_TOP_BAR_HEIGHT);
        } else if (i == g_view_tab_hovered && chrome->hover_tex != NULL) {
            window_draw_texture(chrome->hover_tex, tab_x, y, chrome->seg_w, CAMERA_TOP_BAR_HEIGHT);
        }

        window_draw_texture(chrome->label_tex,
                             tab_x + CAMERA_TOOL_PADDING_X,
                             y + (CAMERA_TOP_BAR_HEIGHT - chrome->label_h) / 2,
                             chrome->label_w, chrome->label_h);

        tab_x += chrome->seg_w;
    }
}

/* ------------------------------------------------------------
 * اسم تبويب البانل بالشريط العلوي - لوحة رصاصية بزوايا مدورة من
 * فوق بس (تمتد شوي داخل جسم البانل الرصاصي، فتختفي استدارة الأسفل
 * تلقائياً لأنها بنفس لون البانل - بدون خط فاصل، إحساس اندماج
 * ناعم). كل بانل له كاش مستقل (مو مشترك) - حالياً اثنين: Scene وFiles
 * ------------------------------------------------------------ */
#define TAB_NAME_PADDING  10
#define TAB_NAME_BLEED     8  /* كم بكسل تمتد اللوحة داخل جسم البانل تحت شريط التبويبات */
#define TAB_NAME_MARGIN   10  /* المسافة من الحافة اليمنى للبانل */

typedef struct {
    window_texture_t *name_tex;
    window_texture_t *highlight_tex;
    int name_w, name_h;
    int ready;
} tab_chrome_t;

static tab_chrome_t g_tab_scene = {0};
static tab_chrome_t g_tab_files = {0};
static tab_chrome_t g_tab_properties = {0};

static void ensure_tab_chrome_ready(tab_chrome_t *chrome, const char *label) {
    if (chrome->ready) return;
    chrome->ready = 1;

    font_text_image_t txt = font_render_text(label, FONT_WEIGHT_REGULAR, 14);
    if (txt.pixels != NULL) {
        chrome->name_tex = window_create_texture(txt.pixels, txt.width, txt.height);
        chrome->name_w = txt.width;
        chrome->name_h = txt.height;
        font_free_text_image(&txt);
    }

    int hl_w = chrome->name_w + TAB_NAME_PADDING * 2;
    int hl_h = TAB_BAR_HEIGHT + TAB_NAME_BLEED;
    shape_image_t hl = shape_provider_render_rect(hl_w, hl_h, 6,
        UI_COLOR_PANEL_BG.r, UI_COLOR_PANEL_BG.g, UI_COLOR_PANEL_BG.b);
    if (hl.pixels != NULL) {
        chrome->highlight_tex = window_create_texture(hl.pixels, hl.width, hl.height);
        shape_provider_free_image(&hl);
    }
}

/* يرسم اسم تبويب فوق شريط تبويبات أي بانل - يستقبل الكاش المناسب له */
static void draw_tab_name(tab_chrome_t *chrome, const char *label,
                           int panel_x, int panel_y, int panel_w) {
    ensure_tab_chrome_ready(chrome, label);
    if (chrome->highlight_tex == NULL) return;

    int hl_w = chrome->name_w + TAB_NAME_PADDING * 2;
    int hl_h = TAB_BAR_HEIGHT + TAB_NAME_BLEED;
    int hl_x = panel_x + panel_w - hl_w - TAB_NAME_MARGIN;
    int hl_y = panel_y;

    window_draw_texture(chrome->highlight_tex, hl_x, hl_y, hl_w, hl_h);
    if (chrome->name_tex != NULL) {
        window_draw_texture(chrome->name_tex,
                             hl_x + TAB_NAME_PADDING,
                             hl_y + (TAB_BAR_HEIGHT - chrome->name_h) / 2,
                             chrome->name_w, chrome->name_h);
    }
}

/* ------------------------------------------------------------
 * شريط أسماء المشاهد المفتوحة - فوق شريط تبديل العرض (3D/2D/Script)
 * مباشرة. تبويب لكل مشهد مفتوح بـscene_tabs.h (بلا حد أدنى واحد -
 * دائماً ≥1). كل تبويب: اسمه (current_scene_get_display_name عبر
 * scene_tabs_get_display_name) + " *" لو فيه تعديلات غير محفوظة،
 * وزر "x" صغير بجانبه لإغلاقه. أيقونة "+" ثابتة بأقصى يمين الشريط
 * بالكامل (مو ملتصقة بآخر تبويب) لإنشاء تبويب جديد.
 *
 * لا سحب/ترتيب ولا تمرير أفقي بعد لو تجاوزت التبويبات عرض الشريط -
 * SCENE_TABS_MAX (16) يحد الحالة أصلاً، وتحسين التمرير خطوة قادمة
 * منفصلة لو احتيج فعلياً بمشروع حقيقي بهالعدد من المشاهد المفتوحة
 * ------------------------------------------------------------ */
#define SCENE_TAB_PAD_L        10
#define SCENE_TAB_GAP           8
#define SCENE_TAB_CLOSE_SIZE   16
#define SCENE_TAB_PAD_R         8
#define SCENE_TAB_PLUS_SIZE    14

/* نفس مبدأ UI_COLOR_HOVER_BLUE_* فوق - مزيج مرئي محسوب مسبقاً بين
 * خلفية الشريط (UI_COLOR_BLACK_MUTED) وUI_COLOR_BUTTON_BLUE، بعامل
 * أقوى (0.55) للتبويب النشط وأضعف (0.25) لتحويم بلا ضغط - بلا أي
 * شفافية فعلية (window_fill_rect لا يدعمها) */
#define SCENE_TAB_ACTIVE_BG_R  0x29
#define SCENE_TAB_ACTIVE_BG_G  0x3B
#define SCENE_TAB_ACTIVE_BG_B  0x48
#define SCENE_TAB_HOVER_BG_R   0x1D
#define SCENE_TAB_HOVER_BG_G   0x25
#define SCENE_TAB_HOVER_BG_B   0x2B

typedef struct {
    window_texture_t *name_tex;
    int name_w, name_h;
    char cached_text[160]; /* آخر نص بُني منه name_tex - لإعادة البناء فقط لو تغيّر فعلياً */
} scene_tab_chrome_t;

static scene_tab_chrome_t g_scene_tab_chrome[SCENE_TABS_MAX];

static window_texture_t *g_scene_tab_close_glyph_tex = NULL;
static int g_scene_tab_close_glyph_w = 0, g_scene_tab_close_glyph_h = 0;

/* فهرس التبويب اللي جسمه محوَّم عليه حالياً (بلا زر X) - -1 يعني
 * ولا واحد. فهرس منفصل لزر X تحديداً (تحويم أدق داخل التبويب نفسه).
 * وحالة تحويم منفصلة لزر "+" الثابت */
static int g_scene_tab_hovered = -1;
static int g_scene_tab_close_hovered = -1;
static int g_scene_tab_plus_hovered = 0;

/* عرض تبويب كامل بالبكسل من عرض اسمه فقط - نفس الحساب يُستخدم
 * بالرسم وباختبار الضغط بالضبط (مصدر وحيد للحقيقة، بلا احتمال
 * تعارض بين الاثنين) */
static int scene_tab_segment_width(int name_w) {
    return SCENE_TAB_PAD_L + name_w + SCENE_TAB_GAP + SCENE_TAB_CLOSE_SIZE + SCENE_TAB_PAD_R;
}

static void ensure_close_glyph_ready(void) {
    if (g_scene_tab_close_glyph_tex != NULL) return;

    font_text_image_t txt = font_render_text("x", FONT_WEIGHT_REGULAR, 13);
    if (txt.pixels != NULL) {
        g_scene_tab_close_glyph_tex = window_create_texture(txt.pixels, txt.width, txt.height);
        g_scene_tab_close_glyph_w = txt.width;
        g_scene_tab_close_glyph_h = txt.height;
        font_free_text_image(&txt);
    }
}

/* يعيد بناء نسيج اسم تبويب معين بس لو تغيّر فعلياً (الاسم نفسه أو
 * حالة dirty) - بلا أي إعادة رسم نص كل إطار بلا داعٍ */
static void ensure_scene_tab_chrome_ready(int index) {
    scene_tab_chrome_t *chrome = &g_scene_tab_chrome[index];

    char text[160];
    const char *name = scene_tabs_get_display_name(index);
    int dirty = scene_tabs_get_dirty(index);
    snprintf(text, sizeof(text), "%s%s", name, dirty ? " *" : "");

    if (strcmp(text, chrome->cached_text) == 0 && chrome->name_tex != NULL) {
        return; /* بلا تغيير - نفس النسيج المبني سابقاً */
    }

    if (chrome->name_tex != NULL) {
        window_destroy_texture(chrome->name_tex);
        chrome->name_tex = NULL;
    }

    font_text_image_t txt = font_render_text(text, FONT_WEIGHT_REGULAR, 13);
    if (txt.pixels != NULL) {
        chrome->name_tex = window_create_texture(txt.pixels, txt.width, txt.height);
        chrome->name_w = txt.width;
        chrome->name_h = txt.height;
        font_free_text_image(&txt);
    }

    strncpy(chrome->cached_text, text, sizeof(chrome->cached_text) - 1);
    chrome->cached_text[sizeof(chrome->cached_text) - 1] = '\0';
}

static void draw_scene_tabs_bar(int x, int y, int w) {
    if (w <= 0) return;

    window_fill_rect(x, y, w, SCENE_TABS_BAR_HEIGHT,
                      UI_COLOR_BLACK_MUTED.r, UI_COLOR_BLACK_MUTED.g, UI_COLOR_BLACK_MUTED.b);

    ensure_close_glyph_ready();

    int active_idx = scene_tabs_active_index();
    int tab_count = scene_tabs_count();
    int tab_x = x;

    for (int i = 0; i < tab_count; i++) {
        ensure_scene_tab_chrome_ready(i);
        scene_tab_chrome_t *chrome = &g_scene_tab_chrome[i];
        int seg_w = scene_tab_segment_width(chrome->name_w);

        if (i == active_idx) {
            window_fill_rect(tab_x, y, seg_w, SCENE_TABS_BAR_HEIGHT,
                              SCENE_TAB_ACTIVE_BG_R, SCENE_TAB_ACTIVE_BG_G, SCENE_TAB_ACTIVE_BG_B);
        } else if (i == g_scene_tab_hovered) {
            window_fill_rect(tab_x, y, seg_w, SCENE_TABS_BAR_HEIGHT,
                              SCENE_TAB_HOVER_BG_R, SCENE_TAB_HOVER_BG_G, SCENE_TAB_HOVER_BG_B);
        }

        if (chrome->name_tex != NULL) {
            window_draw_texture(chrome->name_tex,
                                 tab_x + SCENE_TAB_PAD_L,
                                 y + (SCENE_TABS_BAR_HEIGHT - chrome->name_h) / 2,
                                 chrome->name_w, chrome->name_h);
        }

        int close_x = tab_x + SCENE_TAB_PAD_L + chrome->name_w + SCENE_TAB_GAP;
        int close_y = y + (SCENE_TABS_BAR_HEIGHT - SCENE_TAB_CLOSE_SIZE) / 2;

        if (i == g_scene_tab_close_hovered) {
            window_fill_rect(close_x, close_y, SCENE_TAB_CLOSE_SIZE, SCENE_TAB_CLOSE_SIZE,
                              SCENE_TAB_HOVER_BG_R, SCENE_TAB_HOVER_BG_G, SCENE_TAB_HOVER_BG_B);
        }
        if (g_scene_tab_close_glyph_tex != NULL) {
            window_draw_texture(g_scene_tab_close_glyph_tex,
                                 close_x + (SCENE_TAB_CLOSE_SIZE - g_scene_tab_close_glyph_w) / 2,
                                 close_y + (SCENE_TAB_CLOSE_SIZE - g_scene_tab_close_glyph_h) / 2,
                                 g_scene_tab_close_glyph_w, g_scene_tab_close_glyph_h);
        }

        tab_x += seg_w;
    }

    /* أيقونة "+" ثابتة بأقصى يمين الشريط بالكامل - بلا علاقة بعدد
     * التبويبات الحالي أو عرضها */
    int plus_x = x + w - SCENE_TAB_PAD_R - SCENE_TAB_PLUS_SIZE;
    int plus_y = y + (SCENE_TABS_BAR_HEIGHT - SCENE_TAB_PLUS_SIZE) / 2;
    if (g_scene_tab_plus_hovered) {
        window_fill_rect(plus_x - 3, plus_y - 3, SCENE_TAB_PLUS_SIZE + 6, SCENE_TAB_PLUS_SIZE + 6,
                          SCENE_TAB_HOVER_BG_R, SCENE_TAB_HOVER_BG_G, SCENE_TAB_HOVER_BG_B);
    }
    icon_atlas_draw(ICON_add, plus_x, plus_y, SCENE_TAB_PLUS_SIZE);
}

/* يرسم الشريط الواحد أعلى الكاميرا كاملاً - خلفية موحّدة، ثم من
 * اليسار: تبويبات تبديل العرض (3D/2D/Script)، خط فاصل رفيع، أداة
 * "تحديد" (تفاعلية فعلياً: تحويم أزرق باهت / تحديد أزرق صلب)، خط
 * فاصل رفيع آخر بعدها جاهز لاستقبال أي أقسام قادمة بنفس الآلية */
static void draw_camera_top_bar(int x, int y, int w) {
    if (w <= 0) return;

    window_fill_rect(x, y, w, CAMERA_TOP_BAR_HEIGHT,
                      UI_COLOR_BLACK_MUTED.r, UI_COLOR_BLACK_MUTED.g, UI_COLOR_BLACK_MUTED.b);

    draw_camera_view_tabs(x, y);


}

static void draw_panel(int x, int y, int w, int h) {
    if (w <= 0 || h <= 0) {
        return; /* بانل منكمش بالكامل - لا شيء نرسمه */
    }
    window_fill_rect(x, y, w, h,
                      UI_COLOR_PANEL_BG.r, UI_COLOR_PANEL_BG.g, UI_COLOR_PANEL_BG.b);

    int tab_h = (h < TAB_BAR_HEIGHT) ? h : TAB_BAR_HEIGHT;
    window_fill_rect(x, y, w, tab_h,
                      UI_COLOR_BLACK_MUTED.r, UI_COLOR_BLACK_MUTED.g, UI_COLOR_BLACK_MUTED.b);
}

/* حالة حد فاصل بصرياً - رصاصي عادي، أزرق باهت عند التحويم فوقه
 * (قبل الضغط)، أزرق صلب أثناء سحبه فعلياً */
typedef enum {
    SPLITTER_VISUAL_IDLE,
    SPLITTER_VISUAL_HOVER,
    SPLITTER_VISUAL_ACTIVE
} splitter_visual_t;

static void draw_splitter(int x, int y, int w, int h, splitter_visual_t state) {
    if (w <= 0 || h <= 0) {
        return;
    }
    switch (state) {
        case SPLITTER_VISUAL_ACTIVE:
            window_fill_rect(x, y, w, h,
                              UI_COLOR_BUTTON_BLUE.r, UI_COLOR_BUTTON_BLUE.g, UI_COLOR_BUTTON_BLUE.b);
            break;
        case SPLITTER_VISUAL_HOVER:
            window_fill_rect(x, y, w, h,
                              UI_COLOR_HOVER_BLUE_R, UI_COLOR_HOVER_BLUE_G, UI_COLOR_HOVER_BLUE_B);
            break;
        default:
            window_fill_rect(x, y, w, h,
                              UI_COLOR_BLACK_MUTED.r, UI_COLOR_BLACK_MUTED.g, UI_COLOR_BLACK_MUTED.b);
            break;
    }
}

static int point_in_rect(int px, int py, int x, int y, int w, int h) {
    return px >= x && px < x + w && py >= y && py < y + h;
}

/* سياق صغير يُمرَّر لـasset_browser_open_save وقت "Save" على مشهد
 * لسه بلا مسار (ضمن تدفق إغلاق تبويب) - يُخصَّص عند الفتح، ويُحرَّر
 * داخل on_scene_close_save_picked مهما كانت النتيجة (حفظ حقيقي أو
 * إلغاء)، بنفس أسلوب asset_pick_context_t بـproperties_panel.c */
typedef struct {
    int tab_index;
} scene_close_save_ctx_t;

/* رد استدعاء "Save As" لمشهد بلا مسار وقت إغلاق تبويبه - يُستدعى
 * مرة واحدة عند إغلاق متصفح الحفظ (حفظ حقيقي أو إلغاء) */
static void on_scene_close_save_picked(const char *picked_path, void *user_data) {
    scene_close_save_ctx_t *ctx = (scene_close_save_ctx_t *)user_data;

    if (picked_path != NULL) {
        /* current_scene ما زالت تعكس نفس التبويب - scene_tabs_switch_to
         * استُدعيت قبل فتح close_scene_dialog أصلاً، وما فيه أي تبديل
         * تبويب ممكن يحصل أثناء فتح asset_browser (يستحوذ على كل
         * التفاعل بمفرده) */
        current_scene_save_as(picked_path);
        scene_tabs_close(ctx->tab_index);
    }
    /* لو أُلغيت (NULL): بلا أي تأثير - المشهد يبقى مفتوحاً وغير محفوظ،
     * نفس منطق إلغاء الحفظ العادي */

    free(ctx);
}

/* يطبّق قرار المستخدم بنافذة تأكيد إغلاق المشهد على tab_index المرتبط
 * بها (التُقط قبل استهلاك النتيجة، لأن close_scene_dialog_get_tab_index
 * يفقد معناها بعد close_scene_dialog_consume_result) */
static void apply_close_scene_result(close_scene_result_t result, int tab_index) {
    switch (result) {
        case CLOSE_SCENE_RESULT_DONT_SAVE:
            scene_tabs_close(tab_index);
            break;
        case CLOSE_SCENE_RESULT_SAVE:
            if (current_scene_get_path() != NULL) {
                /* مسار موجود مسبقاً - حفظ مباشر بلا أي نافذة إضافية */
                current_scene_save();
                scene_tabs_close(tab_index);
            } else {
                /* لسه ما اتسمّى مسار - نفتح "حفظ باسم" (الجذر Assets،
                 * بامتداد .rscene جاهز بالاسم الافتراضي) */
                scene_close_save_ctx_t *ctx = malloc(sizeof(scene_close_save_ctx_t));
                if (ctx != NULL) {
                    ctx->tab_index = tab_index;
                    char default_name[160];
                    snprintf(default_name, sizeof(default_name), "%s.rscene",
                             current_scene_get_display_name());
                    asset_browser_open_save(ASSET_BROWSER_ROOT_ASSETS, default_name,
                                             on_scene_close_save_picked, ctx);
                }
            }
            break;
        case CLOSE_SCENE_RESULT_CANCEL:
        case CLOSE_SCENE_RESULT_NONE:
        default:
            break; /* بلا أي تأثير */
    }
}

/* يطلب إغلاق تبويب مشهد معين - لو فيه تعديلات غير محفوظة، ينقل
 * التبويب النشط له أولاً (current_scene يعكس محتواه فعلياً لو
 * المستخدم اختار "Save") ثم يفتح نافذة التأكيد. لو نظيف (بلا
 * تعديلات)، يُغلق مباشرة بلا أي سؤال */
static void request_close_scene_tab(int tab_index) {
    if (scene_tabs_get_dirty(tab_index)) {
        scene_tabs_switch_to(tab_index);
        close_scene_dialog_open(tab_index);
    } else {
        scene_tabs_close(tab_index);
    }
}

/* ------------------------------------------------------------
 * الشريط الطويل بأعلى الشاشة بالكامل (TOOLBAR_HEIGHT) - كلمات
 * قائمة بلا أي زر/خلفية حولها إطلاقاً، من أقصى اليمين لليسار:
 * Export (الوحيدة الفعّالة الآن - تفتح export_dialog)، ثم Project
 * Settings / Projects / Editor Settings (منظر فقط حالياً بانتظار
 * منطقها بخطوة قادمة منفصلة - بالاتفاق). تحوّم/ضغط بلون أفتح بس،
 * بلا أي حدود أو خلفية - نفس ما طُلب بالضبط */
#define TOOLBAR_MENU_FONT_SIZE 13
#define TOOLBAR_MENU_GAP       24 /* المسافة بين كل كلمتين */
#define TOOLBAR_MENU_MARGIN    16 /* من الحافة اليمنى للشاشة */

static const unsigned char TOOLBAR_MENU_COLOR_NORMAL_R = 0x9A, TOOLBAR_MENU_COLOR_NORMAL_G = 0x9A,
                            TOOLBAR_MENU_COLOR_NORMAL_B = 0x9A; /* رمادي باهت - نفس مستوى النصوص الثانوية بالمشروع */
static const unsigned char TOOLBAR_MENU_COLOR_ACTIVE_R = 0xF2, TOOLBAR_MENU_COLOR_ACTIVE_G = 0xF2,
                            TOOLBAR_MENU_COLOR_ACTIVE_B = 0xF2; /* أبيض تقريباً - تحويم أو ضغط */

#define TOOLBAR_MENU_ITEM_COUNT 4
static const char *TOOLBAR_MENU_LABELS[TOOLBAR_MENU_ITEM_COUNT] = {
    "Export", "Project Settings", "Projects", "Editor Settings"
};

static window_texture_t *g_toolbar_menu_tex[TOOLBAR_MENU_ITEM_COUNT];
static int g_toolbar_menu_w[TOOLBAR_MENU_ITEM_COUNT];
static int g_toolbar_menu_h[TOOLBAR_MENU_ITEM_COUNT];
static int g_toolbar_menu_ready = 0;
static int g_toolbar_menu_hovered = -1;

static void ensure_toolbar_menu_ready(void) {
    if (g_toolbar_menu_ready) return;
    g_toolbar_menu_ready = 1;

    for (int i = 0; i < TOOLBAR_MENU_ITEM_COUNT; i++) {
        font_text_image_t img = font_render_text(TOOLBAR_MENU_LABELS[i], FONT_WEIGHT_REGULAR,
                                                   TOOLBAR_MENU_FONT_SIZE);
        g_toolbar_menu_tex[i] = NULL;
        if (img.pixels != NULL) {
            g_toolbar_menu_tex[i] = window_create_texture(img.pixels, img.width, img.height);
            g_toolbar_menu_w[i] = img.width;
            g_toolbar_menu_h[i] = img.height;
            font_free_text_image(&img);
        }
    }
}

/* يحسب موضع x لكل كلمة بالشريط (من اليمين لليسار، بالترتيب المعرَّف
 * أعلاه) - نفس الحساب بالضبط يُستخدم بالرسم وباختبار الضغط/التحويم،
 * عشان الاثنين متطابقين دائماً بلا احتمال تعارض */
static void toolbar_menu_layout(int window_w, int out_x[TOOLBAR_MENU_ITEM_COUNT]) {
    int x = window_w - TOOLBAR_MENU_MARGIN;
    for (int i = 0; i < TOOLBAR_MENU_ITEM_COUNT; i++) {
        x -= g_toolbar_menu_w[i];
        out_x[i] = x;
        x -= TOOLBAR_MENU_GAP;
    }
}

void editor_workspace_update(int window_w, int window_h) {
    /* أول دخول لواجهة التطوير فقط - ينشئ أول تبويب مشهد فاضٍ
     * ("empty") ويفعّله (scene_tabs_init تنادي current_scene_new
     * داخلياً بنفسها) */
    static int scene_system_initialized = 0;
    if (!scene_system_initialized) {
        scene_system_initialized = 1;
        scene_tabs_init();
    }

    /* نافذة تأكيد إغلاق المشهد (Save/Don't Save/Cancel) - لو مفتوحة،
     * تستحوذ على تفاعل الماوس بالكامل هذا الإطار، وتطبّق قرار
     * المستخدم فور صدوره (قد يفتح متصفح "حفظ باسم" فوقها بدوره) */
    if (close_scene_dialog_is_open()) {
        close_scene_dialog_update(window_w, window_h);
        int dlg_tab_index = close_scene_dialog_get_tab_index();
        close_scene_result_t result = close_scene_dialog_consume_result();
        apply_close_scene_result(result, dlg_tab_index);
        return;
    }

    /* نافذة إعدادات التصدير - نفس مبدأ الحجب أعلاه، طالما مفتوحة */
    if (export_dialog_is_open()) {
        export_dialog_update(window_w, window_h);
        return;
    }

    int mx = window_mouse_x();
    int my = window_mouse_y();

    /* كلمات الشريط الطويل بأعلى الشاشة - Export فقط فعّالة الآن */
    ensure_toolbar_menu_ready();
    {
        int label_x[TOOLBAR_MENU_ITEM_COUNT];
        toolbar_menu_layout(window_w, label_x);
        g_toolbar_menu_hovered = -1;
        for (int i = 0; i < TOOLBAR_MENU_ITEM_COUNT; i++) {
            int label_y = (TOOLBAR_HEIGHT - g_toolbar_menu_h[i]) / 2;
            if (point_in_rect(mx, my, label_x[i], label_y, g_toolbar_menu_w[i], g_toolbar_menu_h[i])) {
                g_toolbar_menu_hovered = i;
                if (i == 0 && window_mouse_left_just_pressed()) {
                    export_dialog_open();
                }
            }
        }
    }

    int col_top = TOOLBAR_HEIGHT;
    int col_h    = window_h - TOOLBAR_HEIGHT; /* ارتفاع كامل، مشترك بين الأعمدة الثلاثة بلا استثناء */

    int right_x = window_w - g_right_width;

    int col_split_left_x  = g_left_width;
    int col_split_right_x = right_x;

    int left_inner_y  = col_top + g_left_split_y;
    int right_inner_y = col_top + g_right_split_y;

    /* عمود الوسط - حده الأفقي الداخلي محصور بعرضه هو بس */
    int center_x = g_left_width;
    int center_w = window_w - g_left_width - g_right_width;
    int camera_bottom_y = window_h - g_bottom_center_height;

    /* شجرة العقد (LEFT_TOP) - تُحدَّث دائماً */
    scene_tree_panel_update(0, col_top + TAB_BAR_HEIGHT, g_left_width, g_left_split_y - TAB_BAR_HEIGHT);

    /* نظام الملفات (LEFT_BOTTOM) - تُحدَّث دائماً */
    {
        int fb_y = col_top + g_left_split_y;
        int fb_h = col_h - g_left_split_y;
        file_system_panel_update(0, fb_y + TAB_BAR_HEIGHT, g_left_width, fb_h - TAB_BAR_HEIGHT);
    }

    /* بانل الخصائص (RIGHT_TOP) - يحدّث خانات تحرير القيم الحقيقية
     * (كتابة/تركيز/مؤشر) ويكتبها فوراً على ذاكرة العقدة المحددة -
     * نفس مستطيل draw بالضبط */
    properties_panel_update(right_x, col_top + TAB_BAR_HEIGHT, g_right_width, g_right_split_y - TAB_BAR_HEIGHT);

    /* نافذة إضافة عقدة - لو مفتوحة، تستحوذ على تفاعل الماوس بالكامل
     * (نمنع سحب أي حد فاصل تحتها بنفس اللحظة) */
    if (add_node_dialog_is_open()) {
        add_node_dialog_update(window_w, window_h);
        return;
    }

    /* التحويم (بلا ضغط) على أي حد فاصل - يُحسب من جديد كل إطار،
     * بلا ذاكرة سابقة. أثناء السحب الفعلي يبقى الحد المسحوب "محدداً
     * بصرياً" حتى لو خرج المؤشر لحظياً عن نطاقه الدقيق (سلوك سحب
     * طبيعي، بلا وميض) */
    if (g_dragging != SPLITTER_NONE) {
        g_hovered_splitter = g_dragging;
    } else {
        g_hovered_splitter = SPLITTER_NONE;
        if (point_in_rect(mx, my, col_split_left_x - SPLITTER_THICKNESS / 2, col_top,
                           SPLITTER_THICKNESS, col_h)) {
            g_hovered_splitter = SPLITTER_LEFT_RIGHT_COL;
        } else if (point_in_rect(mx, my, col_split_right_x - SPLITTER_THICKNESS / 2, col_top,
                                  SPLITTER_THICKNESS, col_h)) {
            g_hovered_splitter = SPLITTER_CENTER_RIGHT_COL;
        } else if (point_in_rect(mx, my, 0, left_inner_y - SPLITTER_THICKNESS / 2,
                                  g_left_width, SPLITTER_THICKNESS)) {
            g_hovered_splitter = SPLITTER_LEFT_INNER;
        } else if (point_in_rect(mx, my, right_x, right_inner_y - SPLITTER_THICKNESS / 2,
                                  g_right_width, SPLITTER_THICKNESS)) {
            g_hovered_splitter = SPLITTER_RIGHT_INNER;
        } else if (point_in_rect(mx, my, center_x, camera_bottom_y - SPLITTER_THICKNESS / 2,
                                  center_w, SPLITTER_THICKNESS)) {
            g_hovered_splitter = SPLITTER_CAMERA_BOTTOM;
        }
    }

    /* شريط أسماء المشاهد - جسم كل تبويب يبدّل التبويب النشط، زر
     * "x" تبعه يطلب إغلاقه (قد يفتح نافذة تأكيد لو فيه تعديلات غير
     * محفوظة)، وأيقونة "+" الثابتة يمين الشريط تنشئ تبويباً جديداً.
     * نفس حساب scene_tab_segment_width المستخدم بالرسم بالضبط -
     * مصدر وحيد للحقيقة بين الاثنين */
    {
        int scene_tabs_y = col_top;
        int active_idx = scene_tabs_active_index();
        int tab_count = scene_tabs_count();

        g_scene_tab_hovered = -1;
        g_scene_tab_close_hovered = -1;

        int tab_x = center_x;
        for (int i = 0; i < tab_count; i++) {
            ensure_scene_tab_chrome_ready(i);
            int name_w = g_scene_tab_chrome[i].name_w;
            int seg_w = scene_tab_segment_width(name_w);

            int close_x = tab_x + SCENE_TAB_PAD_L + name_w + SCENE_TAB_GAP;
            int close_y = scene_tabs_y + (SCENE_TABS_BAR_HEIGHT - SCENE_TAB_CLOSE_SIZE) / 2;

            if (point_in_rect(mx, my, close_x, close_y, SCENE_TAB_CLOSE_SIZE, SCENE_TAB_CLOSE_SIZE)) {
                g_scene_tab_close_hovered = i;
                if (window_mouse_left_just_pressed()) {
                    request_close_scene_tab(i);
                    /* إغلاق/فتح نافذة تأكيد قد يغيّر عدد/ترتيب
                     * التبويبات فوراً - نوقف حلقة هذا الإطار هنا،
                     * تُعاد حسابها صح بالإطار القادم */
                    break;
                }
            } else if (point_in_rect(mx, my, tab_x, scene_tabs_y, seg_w, SCENE_TABS_BAR_HEIGHT)) {
                g_scene_tab_hovered = i;
                if (i != active_idx && window_mouse_left_just_pressed()) {
                    scene_tabs_switch_to(i);
                }
            }

            tab_x += seg_w;
        }

        {
            int plus_x = center_x + center_w - SCENE_TAB_PAD_R - SCENE_TAB_PLUS_SIZE;
            int plus_y = scene_tabs_y + (SCENE_TABS_BAR_HEIGHT - SCENE_TAB_PLUS_SIZE) / 2;
            g_scene_tab_plus_hovered = point_in_rect(mx, my, plus_x - 3, plus_y - 3,
                                                      SCENE_TAB_PLUS_SIZE + 6, SCENE_TAB_PLUS_SIZE + 6);
            if (g_scene_tab_plus_hovered && window_mouse_left_just_pressed()) {
                scene_tabs_new();
            }
        }
    }

    /* الشريط الواحد أعلى الكاميرا - تبويبات تبديل العرض (3D/2D/Script)
     * بالقسم الأيسر، ثم أداة "تحديد" بعدها مباشرة. موضعه الرأسي
     * ثابت: يبدأ فوراً تحت شريط أسماء المشاهد، فوق منطقة الكاميرا
     * الفعلية */
    {
        int top_bar_y = col_top + SCENE_TABS_BAR_HEIGHT;

        g_view_tab_hovered = -1;
        {
            int tab_x = center_x;
            for (int i = 0; i < CAMERA_VIEW_TAB_COUNT; i++) {
                int tab_seg_w = g_view_tabs[i].seg_w;
                if (point_in_rect(mx, my, tab_x, top_bar_y, tab_seg_w, CAMERA_TOP_BAR_HEIGHT)) {
                    g_view_tab_hovered = i;
                    if (window_mouse_left_just_pressed()) {
                        g_camera_view_tab_selected = (camera_view_tab_id_t)i;
                        if (i == CAMERA_VIEW_TAB_3D) {
                            g_camera_mode = EDITOR_CAMERA_MODE_3D;
                        } else if (i == CAMERA_VIEW_TAB_2D) {
                            g_camera_mode = EDITOR_CAMERA_MODE_2D;
                        }
                        /* Script: لا يوجد نظام ملفات سكربت فعلي بعد -
                         * تحديد بصري فقط، محتوى الكاميرا يبقى كما هو */
                    }
                }
                tab_x += tab_seg_w;
            }
        }
    }

    /* الكاميرا الفعلية (3D أو 2D حسب المحدد) - نفس مستطيل منطقة
     * الكاميرا المستخدم بالرسم بالضبط (تحت الشريط العلوي مباشرة،
     * بعرض عمود الوسط وارتفاعه الحالي) */
    {
        int camera_h_now = col_h - g_bottom_center_height;
        int viewport_y_now = col_top + SCENE_TABS_BAR_HEIGHT + CAMERA_TOP_BAR_HEIGHT;
        int viewport_h_now = camera_h_now - SCENE_TABS_BAR_HEIGHT - CAMERA_TOP_BAR_HEIGHT;

        if (g_camera_mode == EDITOR_CAMERA_MODE_3D) {
            viewport_3d_update(center_x, viewport_y_now, center_w, viewport_h_now);
        } else {
            viewport_2d_update(center_x, viewport_y_now, center_w, viewport_h_now);
        }
    }

    if (g_dragging == SPLITTER_NONE && window_mouse_left_just_pressed()) {
        if (point_in_rect(mx, my, col_split_left_x - SPLITTER_THICKNESS / 2, col_top,
                           SPLITTER_THICKNESS, col_h)) {
            g_dragging = SPLITTER_LEFT_RIGHT_COL;
        } else if (point_in_rect(mx, my, col_split_right_x - SPLITTER_THICKNESS / 2, col_top,
                                  SPLITTER_THICKNESS, col_h)) {
            g_dragging = SPLITTER_CENTER_RIGHT_COL;
        } else if (point_in_rect(mx, my, 0, left_inner_y - SPLITTER_THICKNESS / 2,
                                  g_left_width, SPLITTER_THICKNESS)) {
            g_dragging = SPLITTER_LEFT_INNER;
        } else if (point_in_rect(mx, my, right_x, right_inner_y - SPLITTER_THICKNESS / 2,
                                  g_right_width, SPLITTER_THICKNESS)) {
            g_dragging = SPLITTER_RIGHT_INNER;
        } else if (point_in_rect(mx, my, center_x, camera_bottom_y - SPLITTER_THICKNESS / 2,
                                  center_w, SPLITTER_THICKNESS)) {
            g_dragging = SPLITTER_CAMERA_BOTTOM;
        }
    }

    if (!window_mouse_left_down()) {
        g_dragging = SPLITTER_NONE;
    }

    switch (g_dragging) {
        case SPLITTER_LEFT_RIGHT_COL: {
            int max_w = window_w - g_right_width - CAMERA_MIN_W;
            g_left_width = mx;
            if (g_left_width < 0) g_left_width = 0;
            if (g_left_width > max_w) g_left_width = (max_w > 0) ? max_w : 0;
            break;
        }
        case SPLITTER_CENTER_RIGHT_COL: {
            int max_w = window_w - g_left_width - CAMERA_MIN_W;
            g_right_width = window_w - mx;
            if (g_right_width < 0) g_right_width = 0;
            if (g_right_width > max_w) g_right_width = (max_w > 0) ? max_w : 0;
            break;
        }
        case SPLITTER_LEFT_INNER: {
            /* ارتفاع العمود الأيسر الكامل ومستقل تماماً */
            g_left_split_y = my - col_top;
            if (g_left_split_y < 0) g_left_split_y = 0;
            if (g_left_split_y > col_h) g_left_split_y = col_h;
            break;
        }
        case SPLITTER_RIGHT_INNER: {
            g_right_split_y = my - col_top;
            if (g_right_split_y < 0) g_right_split_y = 0;
            if (g_right_split_y > col_h) g_right_split_y = col_h;
            break;
        }
        case SPLITTER_CAMERA_BOTTOM: {
            /* الكاميرا محمية بحد أدنى، وفوقها شريطا أسماء المشاهد
             * والأدوات محجوزان دائماً - فالمساحة القابلة للتوزيع
             * الفعلية أقل من ارتفاع العمود الكامل بمقدارهما */
            int max_bottom_h = col_h - CAMERA_MIN_H - SCENE_TABS_BAR_HEIGHT - CAMERA_TOP_BAR_HEIGHT;
            g_bottom_center_height = window_h - my;
            if (g_bottom_center_height < 0) g_bottom_center_height = 0;
            if (g_bottom_center_height > max_bottom_h) {
                g_bottom_center_height = (max_bottom_h > 0) ? max_bottom_h : 0;
            }
            break;
        }
        default:
            break;
    }
}

void editor_workspace_draw(int window_w, int window_h) {
    int col_top = TOOLBAR_HEIGHT;
    int col_h    = window_h - TOOLBAR_HEIGHT;
    int right_x   = window_w - g_right_width;

    /* الشريط العلوي - بعرض الشاشة كاملة، بنفس ارتفاع أشرطة تبويبات
     * البانلات (كان أثخن من قبل وفاضياً بلا رسم - الآن مقلَّص
     * ومرسوم فعلياً، فالبانلات تمددت لأعلى بالفرق تلقائياً لأن
     * col_top نفسه صغُر) */
    window_fill_rect(0, 0, window_w, TOOLBAR_HEIGHT,
                      UI_COLOR_BLACK_MUTED.r, UI_COLOR_BLACK_MUTED.g, UI_COLOR_BLACK_MUTED.b);

    /* كلمات الشريط الطويل - بلا أي خلفية/حدود، فقط تغيّر لون النص
     * عند التحويم (أو الضغط - نفس اللون، الفعل نفسه فوري بلا حالة
     * "مضغوط" منفصلة مطلوبة هنا) */
    ensure_toolbar_menu_ready();
    {
        int label_x[TOOLBAR_MENU_ITEM_COUNT];
        toolbar_menu_layout(window_w, label_x);
        for (int i = 0; i < TOOLBAR_MENU_ITEM_COUNT; i++) {
            if (g_toolbar_menu_tex[i] == NULL) continue;
            int label_y = (TOOLBAR_HEIGHT - g_toolbar_menu_h[i]) / 2;
            int is_active = (i == g_toolbar_menu_hovered);
            unsigned char cr = is_active ? TOOLBAR_MENU_COLOR_ACTIVE_R : TOOLBAR_MENU_COLOR_NORMAL_R;
            unsigned char cg = is_active ? TOOLBAR_MENU_COLOR_ACTIVE_G : TOOLBAR_MENU_COLOR_NORMAL_G;
            unsigned char cb = is_active ? TOOLBAR_MENU_COLOR_ACTIVE_B : TOOLBAR_MENU_COLOR_NORMAL_B;
            window_draw_texture_tinted(g_toolbar_menu_tex[i], label_x[i], label_y,
                                        g_toolbar_menu_w[i], g_toolbar_menu_h[i], cr, cg, cb);
        }
    }

    /* العمود الأيسر - ارتفاع كامل مستقل، فوق وتحت
     * LEFT_TOP: شجرة العقد | LEFT_BOTTOM: نظام الملفات */
    draw_panel(0, col_top, g_left_width, g_left_split_y);
    draw_tab_name(&g_tab_scene, "Scene", 0, col_top, g_left_width);
    scene_tree_panel_draw(0, col_top + TAB_BAR_HEIGHT, g_left_width, g_left_split_y - TAB_BAR_HEIGHT);

    {
        int fb_y = col_top + g_left_split_y;
        int fb_h = col_h - g_left_split_y;
        draw_panel(0, fb_y, g_left_width, fb_h);
        draw_tab_name(&g_tab_files, "Files", 0, fb_y, g_left_width);
        file_system_panel_draw(0, fb_y + TAB_BAR_HEIGHT, g_left_width, fb_h - TAB_BAR_HEIGHT);
    }

    /* العمود الأيمن - نفس الشيء، مستقل تماماً عن اليسار وعن الوسط
     * RIGHT_TOP: بانل الخصائص | RIGHT_BOTTOM: فاضي حالياً */
    draw_panel(right_x, col_top, g_right_width, g_right_split_y);
    draw_tab_name(&g_tab_properties, "Properties", right_x, col_top, g_right_width);
    properties_panel_draw(right_x, col_top + TAB_BAR_HEIGHT, g_right_width, g_right_split_y - TAB_BAR_HEIGHT);

    draw_panel(right_x, col_top + g_right_split_y, g_right_width, col_h - g_right_split_y);

    /* عمود الوسط: شريط واحد فوق الكاميرا (تبويبات العرض + أدوات
     * الكاميرا)، ثم الكاميرا الفعلية مباشرة (بارتفاع = العمود
     * الكامل ناقص البانل السفلي ناقص هذا الشريط الواحد بس - بلا أي
     * شريط ثانٍ يقتطع ارتفاعاً إضافياً) */
    int center_x   = g_left_width;
    int center_w   = window_w - g_left_width - g_right_width;
    int camera_h    = col_h - g_bottom_center_height; /* من تحت الشريط العلوي لحد بداية BOTTOM_CENTER */

    int scene_tabs_y = col_top;
    int top_bar_y    = scene_tabs_y + SCENE_TABS_BAR_HEIGHT;
    int viewport_y   = top_bar_y + CAMERA_TOP_BAR_HEIGHT;
    int viewport_h   = camera_h - SCENE_TABS_BAR_HEIGHT - CAMERA_TOP_BAR_HEIGHT;

    draw_scene_tabs_bar(center_x, scene_tabs_y, center_w);
    draw_camera_top_bar(center_x, top_bar_y, center_w);

    if (g_camera_mode == EDITOR_CAMERA_MODE_3D) {
        viewport_3d_draw(center_x, viewport_y, center_w, viewport_h);
    } else {
        viewport_2d_draw(center_x, viewport_y, center_w, viewport_h);
    }

    /* BOTTOM_CENTER - بعرض عمود الوسط فقط (مو الشاشة كاملة) */
    draw_panel(center_x, col_top + camera_h, center_w, g_bottom_center_height);

    /* الحدود الفاصلة الخمسة - كل واحد بحالته الفعلية (عادي/تحويم/سحب) */
    draw_splitter(g_left_width - SPLITTER_THICKNESS / 2, col_top, SPLITTER_THICKNESS, col_h,
                  (g_dragging == SPLITTER_LEFT_RIGHT_COL) ? SPLITTER_VISUAL_ACTIVE :
                  (g_hovered_splitter == SPLITTER_LEFT_RIGHT_COL) ? SPLITTER_VISUAL_HOVER : SPLITTER_VISUAL_IDLE);
    draw_splitter(right_x - SPLITTER_THICKNESS / 2, col_top, SPLITTER_THICKNESS, col_h,
                  (g_dragging == SPLITTER_CENTER_RIGHT_COL) ? SPLITTER_VISUAL_ACTIVE :
                  (g_hovered_splitter == SPLITTER_CENTER_RIGHT_COL) ? SPLITTER_VISUAL_HOVER : SPLITTER_VISUAL_IDLE);
    draw_splitter(0, col_top + g_left_split_y - SPLITTER_THICKNESS / 2, g_left_width, SPLITTER_THICKNESS,
                  (g_dragging == SPLITTER_LEFT_INNER) ? SPLITTER_VISUAL_ACTIVE :
                  (g_hovered_splitter == SPLITTER_LEFT_INNER) ? SPLITTER_VISUAL_HOVER : SPLITTER_VISUAL_IDLE);
    draw_splitter(right_x, col_top + g_right_split_y - SPLITTER_THICKNESS / 2, g_right_width, SPLITTER_THICKNESS,
                  (g_dragging == SPLITTER_RIGHT_INNER) ? SPLITTER_VISUAL_ACTIVE :
                  (g_hovered_splitter == SPLITTER_RIGHT_INNER) ? SPLITTER_VISUAL_HOVER : SPLITTER_VISUAL_IDLE);
    /* هذا الحد محصور بعرض عمود الوسط بس - نفس فكرة حرف H */
    draw_splitter(center_x, col_top + camera_h - SPLITTER_THICKNESS / 2, center_w, SPLITTER_THICKNESS,
                  (g_dragging == SPLITTER_CAMERA_BOTTOM) ? SPLITTER_VISUAL_ACTIVE :
                  (g_hovered_splitter == SPLITTER_CAMERA_BOTTOM) ? SPLITTER_VISUAL_HOVER : SPLITTER_VISUAL_IDLE);

    /* نافذة إضافة عقدة، ثم نافذة تأكيد إغلاق المشهد فوقها لو الاثنتان
     * مفتوحتان بنفس اللحظة (عملياً لا يحدث - كل واحدة تستحوذ على
     * التفاعل بمفردها بـeditor_workspace_update) - تُرسمان أخيراً،
     * فوق كل شيء ثاني بالواجهة */
    add_node_dialog_draw(window_w, window_h);
    close_scene_dialog_draw(window_w, window_h);
    export_dialog_draw(window_w, window_h);
}

void editor_workspace_shutdown(void) {
    for (int i = 0; i < SCENE_TABS_MAX; i++) {
        if (g_scene_tab_chrome[i].name_tex) window_destroy_texture(g_scene_tab_chrome[i].name_tex);
    }
    if (g_scene_tab_close_glyph_tex) window_destroy_texture(g_scene_tab_close_glyph_tex);
    scene_tabs_shutdown();
    close_scene_dialog_shutdown();
    export_dialog_shutdown();

    for (int i = 0; i < TOOLBAR_MENU_ITEM_COUNT; i++) {
        if (g_toolbar_menu_tex[i]) window_destroy_texture(g_toolbar_menu_tex[i]);
    }

    if (g_tab_scene.name_tex) window_destroy_texture(g_tab_scene.name_tex);
    if (g_tab_scene.highlight_tex) window_destroy_texture(g_tab_scene.highlight_tex);
    if (g_tab_files.name_tex) window_destroy_texture(g_tab_files.name_tex);
    if (g_tab_files.highlight_tex) window_destroy_texture(g_tab_files.highlight_tex);
    if (g_tab_properties.name_tex) window_destroy_texture(g_tab_properties.name_tex);
    if (g_tab_properties.highlight_tex) window_destroy_texture(g_tab_properties.highlight_tex);
    if (g_tool_select.label_tex) window_destroy_texture(g_tool_select.label_tex);
    for (int i = 0; i < CAMERA_VIEW_TAB_COUNT; i++) {
        if (g_view_tabs[i].label_tex)    window_destroy_texture(g_view_tabs[i].label_tex);
        if (g_view_tabs[i].hover_tex)    window_destroy_texture(g_view_tabs[i].hover_tex);
        if (g_view_tabs[i].selected_tex) window_destroy_texture(g_view_tabs[i].selected_tex);
    }
    scene_tree_panel_shutdown();
    file_system_panel_shutdown();
    properties_panel_shutdown();
    add_node_dialog_shutdown();
}
