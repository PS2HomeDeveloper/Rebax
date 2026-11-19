/*
 *  ____  _____ ____    _    __  __
 * |  _ \| ____| __ )  / \   \ \/ /
 * | |_) | _| | _ \ / _ \   \  /
 * |  _ <| |___| |_) / ___ \  /  \
 * |_| \_\_____|____/_/   \_\/_/\_\
 *
 * Rebax Engine
 * Copyright (c) 2026 PS2HomeDeveloper
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */
#include "formats/tim/tim_internal.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>

static int color_distance(const unsigned char *a, const unsigned char *b) {
    int r = (int)a[0] - b[0], g = (int)a[1] - b[1], b0 = (int)a[2] - b[2];
    return r * r + g * g + b0 * b0;
}

int tim_quantize(const unsigned char *rgba, int w, int h, int colors, int dither, unsigned char **indices,
                 unsigned char **palette) {
    size_t np = (size_t)w * h;
    unsigned char *idx, *pal;
    double *sum, *count;
    int i, k, iter;
    if (!rgba || !indices || !palette || w <= 0 || h <= 0 || colors < 2 || colors > 256)
        return 0;
    idx = (unsigned char *)malloc(np);
    pal = (unsigned char *)malloc((size_t)colors * 4);
    sum = (double *)calloc((size_t)colors * 4, sizeof(double));
    count = (double *)calloc((size_t)colors, sizeof(double));
    if (!idx || !pal || !sum || !count) {
        free(idx);
        free(pal);
        free(sum);
        free(count);
        return 0;
    }
    for (k = 0; k < colors; k++) {
        size_t q = (size_t)k * np / (size_t)colors;
        memcpy(pal + k * 4, rgba + q * 4, 4);
        if (pal[k * 4 + 3] == 0)
            pal[k * 4 + 3] = 255;
    }
    for (iter = 0; iter < 4; iter++) {
        memset(sum, 0, (size_t)colors * 4 * sizeof(double));
        memset(count, 0, (size_t)colors * sizeof(double));
        for (i = 0; i < (int)np; i++) {
            int best = 0, best_distance = INT_MAX;
            for (k = 0; k < colors; k++) {
                int d = color_distance(rgba + i * 4, pal + k * 4);
                if (d < best_distance) {
                    best_distance = d;
                    best = k;
                }
            }
            idx[i] = (unsigned char)best;
            sum[best * 4] += rgba[i * 4];
            sum[best * 4 + 1] += rgba[i * 4 + 1];
            sum[best * 4 + 2] += rgba[i * 4 + 2];
            sum[best * 4 + 3] += rgba[i * 4 + 3];
            count[best] += 1.0;
        }
        for (k = 0; k < colors; k++)
            if (count[k] > 0.0) {
                pal[k * 4] = (unsigned char)(sum[k * 4] / count[k] + 0.5);
                pal[k * 4 + 1] = (unsigned char)(sum[k * 4 + 1] / count[k] + 0.5);
                pal[k * 4 + 2] = (unsigned char)(sum[k * 4 + 2] / count[k] + 0.5);
                pal[k * 4 + 3] = (unsigned char)(sum[k * 4 + 3] / count[k] + 0.5);
            }
    }
    if (dither) {
        double *cur = (double *)calloc((size_t)(w + 2) * 3, sizeof(double));
        double *next = (double *)calloc((size_t)(w + 2) * 3, sizeof(double));
        if (cur && next) {
            int y, x, c;
            for (y = 0; y < h; y++) {
                memset(next, 0, (size_t)(w + 2) * 3 * sizeof(double));
                for (x = 0; x < w; x++) {
                    double adjusted[3];
                    int best = 0, best_distance = INT_MAX;
                    for (c = 0; c < 3; c++)
                        adjusted[c] = rgba[((size_t)y * w + x) * 4 + c] + cur[(x + 1) * 3 + c];
                    for (k = 0; k < colors; k++) {
                        int dr = (int)adjusted[0] - pal[k * 4];
                        int dg = (int)adjusted[1] - pal[k * 4 + 1];
                        int db = (int)adjusted[2] - pal[k * 4 + 2];
                        int d = dr * dr + dg * dg + db * db;
                        if (d < best_distance) {
                            best_distance = d;
                            best = k;
                        }
                    }
                    idx[(size_t)y * w + x] = (unsigned char)best;
                    for (c = 0; c < 3; c++) {
                        double e = adjusted[c] - pal[best * 4 + c];
                        cur[(x + 2) * 3 + c] += e * 7.0 / 16.0;
                        next[x * 3 + c] += e * 3.0 / 16.0;
                        next[(x + 1) * 3 + c] += e * 5.0 / 16.0;
                        next[(x + 2) * 3 + c] += e / 16.0;
                    }
                }
                {
                    double *swap = cur;
                    cur = next;
                    next = swap;
                }
            }
        }
        free(cur);
        free(next);
    }
    free(sum);
    free(count);
    *indices = idx;
    *palette = pal;
    return 1;
}
