/*
 * ============================================================
 * ui_common.h
 * ============================================================
 * أدوات واجهة صغيرة مشتركة بين كل نوافذ المحرك ولوحاته - كانت
 * مكرّرة بنفس الشكل في export_dialog وproject_dialog وasset_browser
 * وadd_node_dialog وغيرها. أي نافذة جديدة تستدعيها من هنا بدل ما
 * تكتب نسختها الخاصة.
 * ============================================================
 */

#ifndef UI_COMMON_H
#define UI_COMMON_H

#include "window.h"
#include "font.h"
#include "labeled_button.h"
#include "shape_provider.h"
#include "ui_theme.h"

/* استدارة زوايا الأزرار الموحدة، وهوامش النص داخل الزر */
#define UI_BUTTON_RADIUS      3
#define UI_BUTTON_PADDING_X  14
#define UI_BUTTON_PADDING_Y   6

/* يحوّل نصاً إلى صورة جاهزة للرسم بالشاشة. يرجع NULL عند الفشل.
 * out_w/out_h يستلمان أبعاد النص بالبكسل */
window_texture_t *ui_make_text_texture(const char *text, font_weight_t weight,
                                        int pixel_size, int *out_w, int *out_h);

/* يصنع زراً أزرق موحداً بحجم يناسب نصه: out_btn هو الزر نفسه،
 * وout_btn_tex صورة خلفيته، وout_txt_tex صورة نصه */
void ui_make_blue_button(const char *text, int font_pixel_size,
                          labeled_button_t *out_btn,
                          window_texture_t **out_btn_tex,
                          window_texture_t **out_txt_tex);

/* يضرب شفافية كل بكسلات الصورة بـfactor (0.25 = أخف بأربع مرات) */
void ui_dim_alpha(shape_image_t *img, double factor);

/* يحسب الزاوية العلوية اليسرى لعنصر بأبعاد (w, h) بمنتصف النافذة */
void ui_center_rect(int window_w, int window_h, int w, int h, int *out_x, int *out_y);

/* هل النقطة (px, py) داخل المستطيل؟ */
int ui_point_in_rect(int px, int py, int x, int y, int w, int h);

/* هل text يحتوي needle (بلا حساسية لحالة الأحرف)؟ لو needle فارغ يرجع 1 -
 * يستخدم لفلترة القوائم بحسب خانة البحث */
int ui_text_contains_ci(const char *text, const char *needle);

#endif /* UI_COMMON_H */
