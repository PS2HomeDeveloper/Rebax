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

#ifndef TEXT_SHAPING_H
#define TEXT_SHAPING_H

typedef struct {
    int cp;
    int src;
} text_cp_t;

int text_decode_utf8(const char *text, int len, text_cp_t *out);
int text_has_rtl(const text_cp_t *cps, int count);
int text_shape_and_reorder(text_cp_t *cps, int count);

#endif
