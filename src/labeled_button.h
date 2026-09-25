/*
 * ============================================================
 * labeled_button.h
 * ============================================================
 * يجمع شكلاً (من shape_provider) ونصاً (من font) في زر واحد،
 * بحجم يُحسب تلقائياً = حجم النص + هامش ثابت من كل جهة. يوفر
 * لأي مكتبة واجهة (ui_project_center، project_dialog، إلخ) طريقة
 * جاهزة لإنشاء زر مقاسه مناسب دائماً لمحتواه.
 * ============================================================
 */

#ifndef LABELED_BUTTON_H
#define LABELED_BUTTON_H

#include "shape_provider.h"
#include "font.h"

typedef struct {
    shape_image_t      shape_img;    /* بكسلات خلفية الزر بالحجم النهائي */
    font_text_image_t  text_img;     /* بكسلات النص نفسه */
    int width;                       /* = shape_img.width، للراحة */
    int height;                      /* = shape_img.height، للراحة */
    int text_offset_x;               /* موضع النص داخل الزر من الزاوية العلوية اليسرى */
    int text_offset_y;
} labeled_button_t;

/* ينشئ زراً بحجم يُحسب تلقائياً = حجم النص + هامش (padding) ثابت
 * من كل جهة، بلون ودرجة استدارة زوايا محددة. يستدعي font_init
 * داخلياً عند الحاجة. عند الفشل، الحقول تكون فارغة (width = 0) */
labeled_button_t labeled_button_create(const char *text, font_weight_t weight,
                                        int font_pixel_size,
                                        int padding_x, int padding_y,
                                        int corner_radius,
                                        unsigned char r, unsigned char g, unsigned char b);

/* يحرر كل الذاكرة المرتبطة بزر أُنشئ عبر labeled_button_create */
void labeled_button_free(labeled_button_t *btn);

#endif /* LABELED_BUTTON_H */
