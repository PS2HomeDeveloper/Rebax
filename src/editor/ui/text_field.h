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

#ifndef TEXT_FIELD_H
#define TEXT_FIELD_H

#define TEXT_FIELD_MAX_LEN 256

typedef struct {
    char text[TEXT_FIELD_MAX_LEN];
    int  cursor_pos;
    int  scroll_offset;
    int  focused;

    void *_cache_bg_tex;
    void *_cache_text_tex;
    char  _cache_text[TEXT_FIELD_MAX_LEN];
    int   _cache_w;
    int   _cache_h;
    int   _cache_text_w;
    int   _cache_text_h;
    int   _cache_valid;
} text_field_t;

void text_field_init(text_field_t *field);

void text_field_free(text_field_t *field);

void text_field_update(text_field_t *field, int x, int y, int w, int h,
                        int field_width);

void text_field_draw(text_field_t *field, int x, int y, int w, int h,
                      int font_pixel_size);

#endif
