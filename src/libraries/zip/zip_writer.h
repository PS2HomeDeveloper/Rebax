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

#ifndef ZIP_WRITER_H
#define ZIP_WRITER_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "libraries/bytes/byte_buffer.h"

typedef struct zip_writer zip_writer_t;

uint32_t zip_crc32(uint32_t crc, const unsigned char *data, size_t n);

zip_writer_t *zip_writer_create(const char *path);
int zip_writer_add_memory(zip_writer_t *z, const char *name, const unsigned char *data, size_t n, uint32_t align, uint32_t unix_mode);
int zip_writer_add_file(zip_writer_t *z, const char *name, const char *path, uint32_t align, uint32_t unix_mode);
int zip_writer_directory(zip_writer_t *z, byte_buf_t *directory, byte_buf_t *end_record);
int zip_writer_append(zip_writer_t *z, const void *data, size_t n);
int zip_writer_close(zip_writer_t *z);
int zip_writer_finish(zip_writer_t *z);
void zip_writer_abort(zip_writer_t *z);
void zip_writer_free(zip_writer_t *z);
FILE *zip_writer_stream(zip_writer_t *z);
uint32_t zip_writer_position(const zip_writer_t *z);
const char *zip_writer_error(const zip_writer_t *z);

#endif
