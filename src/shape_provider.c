/*
 * ============================================================
 * shape_provider.c
 * ============================================================
 * الطريقة: لكل بكسل في الصورة الناتجة، نحسب "أقرب مسافة" بينه
 * وبين حافة الشكل (معادلة رياضية قياسية لمستطيل بزوايا مدورة).
 * لو البكسل داخل الشكل تماماً: لون صلب كامل. لو خارجه تماماً:
 * شفاف كامل. لو قريب جداً من الحافة (أقل من بكسل واحد): شفافية
 * جزئية متدرجة - وهذا بالضبط ما يعطي إحساس "النعومة" للعين.
 *
 * بما إن الحساب رياضي دقيق (مو نسخ بكسلات من صورة جاهزة)، النعومة
 * مثالية دائماً بأي حجم وأي درجة استدارة، بدون أي قيود.
 * ============================================================
 */

#include <stdlib.h>
#include <math.h>

#include "shape_provider.h"

/* المسافة الموقّعة (Signed Distance) بين نقطة ومستطيل بزوايا مدورة
 * مركزه نقطة الأصل. سالبة = داخل الشكل، موجبة = خارجه، صفر = بالضبط
 * على الحافة. px, py إحداثيات النقطة بالنسبة لمركز المستطيل.
 * hw, hh نصف العرض ونصف الارتفاع. r نصف قطر الاستدارة. */
static double signed_distance_rounded_rect(double px, double py,
                                            double hw, double hh, double r) {
    double qx = fabs(px) - (hw - r);
    double qy = fabs(py) - (hh - r);
    double outside_x = qx > 0.0 ? qx : 0.0;
    double outside_y = qy > 0.0 ? qy : 0.0;
    double outside_dist = sqrt(outside_x * outside_x + outside_y * outside_y);
    double inside_dist = qx > qy ? qy : qx; /* min(qx, qy) */
    if (inside_dist > 0.0) inside_dist = 0.0;
    return outside_dist + inside_dist - r;
}

shape_image_t shape_provider_render_rect(int width, int height, int corner_radius,
                                          unsigned char r, unsigned char g, unsigned char b) {
    shape_image_t out = {0};

    if (width <= 0 || height <= 0) {
        return out;
    }

    /* نصف القطر ما يمكن يتعدى نصف أصغر بُعد، وإلا الشكل يتشوه */
    double radius = (double)corner_radius;
    double max_radius = (width < height ? width : height) / 2.0;
    if (radius > max_radius) radius = max_radius;
    if (radius < 0.0) radius = 0.0;

    out.width = width;
    out.height = height;
    out.pixels = (unsigned char *)malloc((size_t)width * (size_t)height * 4);
    if (out.pixels == NULL) {
        out.width = out.height = 0;
        return out;
    }

    double hw = width  / 2.0;
    double hh = height / 2.0;

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            /* مركز البكسل (x+0.5, y+0.5) بالنسبة لمركز المستطيل */
            double px = (x + 0.5) - hw;
            double py = (y + 0.5) - hh;

            double dist = signed_distance_rounded_rect(px, py, hw, hh, radius);

            /* تغطية البكسل (0 = شفاف كامل، 1 = صلب كامل) - تدرج
             * ناعم على مسافة بكسل واحد حول الحافة (dist = 0) */
            double coverage = 0.5 - dist;
            if (coverage < 0.0) coverage = 0.0;
            if (coverage > 1.0) coverage = 1.0;

            unsigned char *px_out = &out.pixels[(y * width + x) * 4];
            px_out[0] = r;
            px_out[1] = g;
            px_out[2] = b;
            px_out[3] = (unsigned char)(coverage * 255.0);
        }
    }

    return out;
}

void shape_provider_free_image(shape_image_t *img) {
    if (img != NULL && img->pixels != NULL) {
        free(img->pixels);
        img->pixels = NULL;
        img->width = img->height = 0;
    }
}
