/*
 *  ____  _____ ____    _    __  __
 * |  _ \| ____| __ )  / \   \ \/ /
 * | |_) |  _| |  _ \ / _ \   \  / 
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

#include <stdlib.h>
#include <string.h>

#include "editor/ui/text_shaping.h"

int text_decode_utf8(const char *text, int len, text_cp_t *out) {
    int n = 0;
    int i = 0;
    while (i < len) {
        unsigned char c = (unsigned char)text[i];
        int cp = c;
        int extra = 0;
        if (c >= 0xF0 && c <= 0xF4) { cp = c & 0x07; extra = 3; }
        else if (c >= 0xE0) { cp = c & 0x0F; extra = 2; }
        else if (c >= 0xC2 && c < 0xE0) { cp = c & 0x1F; extra = 1; }
        else if (c >= 0x80) { out[n].cp = c; out[n].src = i; n++; i++; continue; }
        int ok = 1;
        for (int k = 1; k <= extra && ok; k++) {
            if (i + k >= len || ((unsigned char)text[i + k] & 0xC0) != 0x80) ok = 0;
            else cp = (cp << 6) | ((unsigned char)text[i + k] & 0x3F);
        }
        if (!ok) { out[n].cp = c; out[n].src = i; n++; i++; continue; }
        out[n].cp = cp;
        out[n].src = i;
        n++;
        i += extra + 1;
    }
    return n;
}

static int is_rtl_char(int c) {
    return (c >= 0x0590 && c <= 0x08FF) || (c >= 0xFB1D && c <= 0xFDFF) || (c >= 0xFE70 && c <= 0xFEFF);
}

int text_has_rtl(const text_cp_t *cps, int count) {
    for (int i = 0; i < count; i++) if (is_rtl_char(cps[i].cp)) return 1;
    return 0;
}

typedef struct { int iso, fin, ini, med; } arabic_forms_t;

static int arabic_forms(int c, arabic_forms_t *f) {
    static const struct { int base; int iso; int dual; } table[] = {
        {0x0621, 0xFE80, 0}, {0x0622, 0xFE81, 0}, {0x0623, 0xFE83, 0}, {0x0624, 0xFE85, 0},
        {0x0625, 0xFE87, 0}, {0x0626, 0xFE89, 1}, {0x0627, 0xFE8D, 0}, {0x0628, 0xFE8F, 1},
        {0x0629, 0xFE93, 0}, {0x062A, 0xFE95, 1}, {0x062B, 0xFE99, 1}, {0x062C, 0xFE9D, 1},
        {0x062D, 0xFEA1, 1}, {0x062E, 0xFEA5, 1}, {0x062F, 0xFEA9, 0}, {0x0630, 0xFEAB, 0},
        {0x0631, 0xFEAD, 0}, {0x0632, 0xFEAF, 0}, {0x0633, 0xFEB1, 1}, {0x0634, 0xFEB5, 1},
        {0x0635, 0xFEB9, 1}, {0x0636, 0xFEBD, 1}, {0x0637, 0xFEC1, 1}, {0x0638, 0xFEC5, 1},
        {0x0639, 0xFEC9, 1}, {0x063A, 0xFECD, 1}, {0x0641, 0xFED1, 1}, {0x0642, 0xFED5, 1},
        {0x0643, 0xFED9, 1}, {0x0644, 0xFEDD, 1}, {0x0645, 0xFEE1, 1}, {0x0646, 0xFEE5, 1},
        {0x0647, 0xFEE9, 1}, {0x0648, 0xFEED, 0}, {0x0649, 0xFEEF, 0}, {0x064A, 0xFEF1, 1}
    };
    if (c == 0x0621) { f->iso = f->fin = f->ini = f->med = 0xFE80; return 3; }
    if (c == 0x0640) { f->iso = f->fin = f->ini = f->med = 0x0640; return 2; }
    for (size_t i = 0; i < sizeof(table) / sizeof(table[0]); i++) {
        if (table[i].base != c) continue;
        f->iso = table[i].iso;
        if (table[i].dual) { f->fin = f->iso + 1; f->ini = f->iso + 2; f->med = f->iso + 3; return 2; }
        f->fin = f->iso + 1; f->ini = 0; f->med = 0;
        return 1;
    }
    return 0;
}

static int is_mark(int c) {
    return (c >= 0x064B && c <= 0x065F) || c == 0x0670 || (c >= 0x06D6 && c <= 0x06DC) || (c >= 0x06DF && c <= 0x06E4);
}

static int lam_alef(int alef) {
    switch (alef) {
        case 0x0622: return 0xFEF5;
        case 0x0623: return 0xFEF7;
        case 0x0625: return 0xFEF9;
        case 0x0627: return 0xFEFB;
        default: return 0;
    }
}

static void shape_arabic(text_cp_t *cps, int *count) {
    int n = *count;
    text_cp_t *src = (text_cp_t *)malloc((size_t)n * sizeof(text_cp_t));
    if (src == NULL) return;
    int m = 0;
    for (int i = 0; i < n; i++) if (!is_mark(cps[i].cp)) src[m++] = cps[i];
    int out = 0;
    for (int i = 0; i < m; i++) {
        arabic_forms_t f;
        int kind = arabic_forms(src[i].cp, &f);
        if (!kind) { cps[out++] = src[i]; continue; }
        if (kind == 3) { cps[out].cp = f.iso; cps[out].src = src[i].src; out++; continue; }
        arabic_forms_t pf, nf;
        int prev_joins = 0;
        if (i > 0) {
            int pk = arabic_forms(src[i - 1].cp, &pf);
            prev_joins = (pk == 2);
        }
        if (src[i].cp == 0x0644 && i + 1 < m && lam_alef(src[i + 1].cp)) {
            int lig = lam_alef(src[i + 1].cp);
            cps[out].cp = prev_joins ? lig + 1 : lig;
            cps[out].src = src[i].src;
            out++;
            i++;
            continue;
        }
        int next_joins = 0;
        if (kind == 2 && i + 1 < m) {
            int nk = arabic_forms(src[i + 1].cp, &nf);
            next_joins = (nk == 1 || nk == 2);
        }
        int form;
        if (kind == 2) form = prev_joins ? (next_joins ? f.med : f.fin) : (next_joins ? f.ini : f.iso);
        else form = prev_joins ? f.fin : f.iso;
        cps[out].cp = form;
        cps[out].src = src[i].src;
        out++;
    }
    free(src);
    *count = out;
}

enum { T_R, T_L, T_N, T_W };

static int classify(int c) {
    if (is_rtl_char(c)) return T_R;
    if ((c >= '0' && c <= '9') || (c >= 0x0660 && c <= 0x0669)) return T_N;
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')) return T_L;
    if (c >= 0xC0 && !(c >= 0x2000 && c <= 0x206F) && !(c >= 0x3000 && c <= 0x303F)) return T_L;
    return T_W;
}

static int mirror(int c) {
    switch (c) {
        case '(': return ')'; case ')': return '(';
        case '[': return ']'; case ']': return '[';
        case '{': return '}'; case '}': return '{';
        case '<': return '>'; case '>': return '<';
        default: return c;
    }
}

static void reverse_range(text_cp_t *a, int from, int to) {
    while (from < to) { text_cp_t t = a[from]; a[from] = a[to]; a[to] = t; from++; to--; }
}

static void reorder(text_cp_t *cps, int n) {
    int *type = (int *)malloc((size_t)n * sizeof(int) * 2);
    if (type == NULL) return;
    int *level = type + n;
    int base_rtl = 0;
    for (int i = 0; i < n; i++) {
        type[i] = classify(cps[i].cp);
    }
    for (int i = 0; i < n; i++) {
        if (type[i] == T_R) { base_rtl = 1; break; }
        if (type[i] == T_L) break;
    }
    int base_dir = base_rtl ? T_R : T_L;
    for (int i = 0; i < n; i++) {
        if (type[i] != T_W) continue;
        int j = i;
        while (j < n && type[j] == T_W) j++;
        int prev = i > 0 ? (type[i - 1] == T_N ? T_R : type[i - 1]) : base_dir;
        int next = j < n ? (type[j] == T_N ? T_R : type[j]) : base_dir;
        int dir = (prev == next) ? prev : base_dir;
        for (int k = i; k < j; k++) level[k] = dir == T_R ? 1 : (base_rtl ? 2 : 0);
        i = j - 1;
    }
    int last_strong = base_dir;
    for (int i = 0; i < n; i++) {
        if (type[i] == T_R) { level[i] = 1; last_strong = T_R; }
        else if (type[i] == T_L) { level[i] = base_rtl ? 2 : 0; last_strong = T_L; }
        else if (type[i] == T_N) level[i] = (base_rtl || last_strong == T_R) ? 2 : 0;
    }
    int max_level = 0, min_odd = 99;
    for (int i = 0; i < n; i++) {
        if (level[i] > max_level) max_level = level[i];
        if ((level[i] & 1) && level[i] < min_odd) min_odd = level[i];
        if (level[i] & 1) cps[i].cp = mirror(cps[i].cp);
    }
    for (int lv = max_level; lv >= min_odd && lv > 0; lv--) {
        for (int i = 0; i < n; i++) {
            if (level[i] < lv) continue;
            int j = i;
            while (j < n && level[j] >= lv) j++;
            reverse_range(cps, i, j - 1);
            {
                int a = i, b = j - 1;
                while (a < b) { int t = level[a]; level[a] = level[b]; level[b] = t; a++; b--; }
            }
            i = j - 1;
        }
    }
    free(type);
}

int text_shape_and_reorder(text_cp_t *cps, int count) {
    shape_arabic(cps, &count);
    reorder(cps, count);
    return count;
}
