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

#ifndef TOOL_COMMON_H
#define TOOL_COMMON_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    unsigned char *data;
    size_t len;
    size_t cap;
} sb_t;

void die(const char *fmt, ...);
void *xmalloc(size_t n);
void *xrealloc(void *p, size_t n);
char *xstrdup(const char *s);
void sb_reserve(sb_t *b, size_t extra);
void sb_append(sb_t *b, const void *src, size_t n);
void sb_puts(sb_t *b, const char *s);
void sb_putc(sb_t *b, unsigned char c);
void sb_printf(sb_t *b, const char *fmt, ...);
void sb_free(sb_t *b);
void put_le32(sb_t *b, uint32_t v);
void put_be32(sb_t *b, uint32_t v);
unsigned char *read_file(const char *path, size_t *out_len);
int is_sep(char c);
void ensure_parent_dir(const char *path);
void write_file(const char *path, const void *data, size_t len);
int write_text_file(const char *path, const char *text);

#endif
