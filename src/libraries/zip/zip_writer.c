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
 * Licensed under the MIT License.
 * See the LICENSE file for details.
 */

#include "libraries/zip/zip_writer.h"

#include <stdlib.h>
#include <string.h>

#define COPY_BUF 262144u
#define DOS_DATE 0x5821u
#define MAX_SIZE 0x7fffffffu

typedef struct {
    char *name;
    uint32_t crc;
    uint32_t size;
    uint32_t offset;
    uint32_t mode;
} entry_t;

struct zip_writer {
    FILE *f;
    char *path;
    entry_t *entries;
    int count;
    int cap;
    uint64_t pos;
    unsigned char *copy;
    char error[600];
};

static uint32_t g_table[256];
static int g_table_ready = 0;

uint32_t zip_crc32(uint32_t crc, const unsigned char *data, size_t n) {
    if (!g_table_ready) {
        for (uint32_t i = 0; i < 256; i++) {
            uint32_t c = i;
            for (int k = 0; k < 8; k++) c = (c & 1) ? (0xedb88320u ^ (c >> 1)) : (c >> 1);
            g_table[i] = c;
        }
        g_table_ready = 1;
    }
    crc = ~crc;
    for (size_t i = 0; i < n; i++) crc = g_table[(crc ^ data[i]) & 0xff] ^ (crc >> 8);
    return ~crc;
}

static void fail(zip_writer_t *z, const char *what, const char *name) {
    snprintf(z->error, sizeof(z->error), "%s %s", what, name);
}

zip_writer_t *zip_writer_create(const char *path) {
    zip_writer_t *z = (zip_writer_t *)calloc(1, sizeof(*z));
    if (!z) return NULL;
    z->f = fopen(path, "wb+");
    z->path = strdup(path);
    z->copy = (unsigned char *)malloc(COPY_BUF);
    if (!z->f || !z->path || !z->copy) {
        zip_writer_abort(z);
        return NULL;
    }
    return z;
}

static void release(zip_writer_t *z) {
    for (int i = 0; i < z->count; i++) free(z->entries[i].name);
    free(z->entries);
    free(z->copy);
    free(z->path);
    free(z);
}

void zip_writer_abort(zip_writer_t *z) {
    if (!z) return;
    if (z->f) fclose(z->f);
    if (z->path) remove(z->path);
    release(z);
}

FILE *zip_writer_stream(zip_writer_t *z) { return z->f; }

uint32_t zip_writer_position(const zip_writer_t *z) { return (uint32_t)z->pos; }

const char *zip_writer_error(const zip_writer_t *z) { return z->error; }

int zip_writer_append(zip_writer_t *z, const void *data, size_t n) {
    if (n == 0) return 1;
    if (fwrite(data, 1, n, z->f) != n) {
        fail(z, "cannot write", z->path);
        return 0;
    }
    z->pos += n;
    return 1;
}

static int begin_entry(zip_writer_t *z, const char *name, uint32_t crc, uint32_t size, uint32_t align, uint32_t mode) {
    if (z->count == z->cap) {
        int c = z->cap ? z->cap * 2 : 64;
        entry_t *p = (entry_t *)realloc(z->entries, (size_t)c * sizeof(entry_t));
        if (!p) { fail(z, "out of memory for", name); return 0; }
        z->entries = p;
        z->cap = c;
    }
    if (align == 0) align = 1;
    size_t name_len = strlen(name);
    uint64_t data_pos = z->pos + 30 + name_len;
    uint32_t pad = (uint32_t)((align - (data_pos % align)) % align);
    if (pad > 0 && pad < 4) pad += align;
    byte_buf_t h;
    byte_buf_init(&h);
    byte_buf_u32(&h, 0x04034b50u);
    byte_buf_u16(&h, 20);
    byte_buf_u16(&h, 0x0800);
    byte_buf_u16(&h, 0);
    byte_buf_u16(&h, 0);
    byte_buf_u16(&h, DOS_DATE);
    byte_buf_u32(&h, crc);
    byte_buf_u32(&h, size);
    byte_buf_u32(&h, size);
    byte_buf_u16(&h, (uint32_t)name_len);
    byte_buf_u16(&h, pad);
    byte_buf_put(&h, name, name_len);
    if (pad) {
        byte_buf_u16(&h, 0xd935);
        byte_buf_u16(&h, pad - 4);
        byte_buf_zero(&h, pad - 4);
    }
    entry_t *e = &z->entries[z->count];
    e->name = strdup(name);
    e->crc = crc;
    e->size = size;
    e->offset = (uint32_t)z->pos;
    e->mode = mode;
    int ok = e->name && !h.failed && zip_writer_append(z, h.data, h.size);
    byte_buf_free(&h);
    if (ok) z->count++;
    return ok;
}

int zip_writer_add_memory(zip_writer_t *z, const char *name, const unsigned char *data, size_t n, uint32_t align, uint32_t unix_mode) {
    if (n > MAX_SIZE) { fail(z, "entry is too large:", name); return 0; }
    if (!begin_entry(z, name, zip_crc32(0, data, n), (uint32_t)n, align, unix_mode)) return 0;
    return zip_writer_append(z, data, n);
}

int zip_writer_add_file(zip_writer_t *z, const char *name, const char *path, uint32_t align, uint32_t unix_mode) {
    FILE *f = fopen(path, "rb");
    if (!f) { fail(z, "cannot read", path); return 0; }
    uint32_t crc = 0;
    uint64_t total = 0;
    size_t n;
    while ((n = fread(z->copy, 1, COPY_BUF, f)) > 0) {
        crc = zip_crc32(crc, z->copy, n);
        total += n;
    }
    if (total > MAX_SIZE) { fclose(f); fail(z, "file is too large for a zip:", path); return 0; }
    fseek(f, 0, SEEK_SET);
    int ok = begin_entry(z, name, crc, (uint32_t)total, align, unix_mode);
    while (ok && (n = fread(z->copy, 1, COPY_BUF, f)) > 0) ok = zip_writer_append(z, z->copy, n);
    fclose(f);
    return ok;
}

int zip_writer_directory(zip_writer_t *z, byte_buf_t *directory, byte_buf_t *end_record) {
    for (int i = 0; i < z->count; i++) {
        entry_t *e = &z->entries[i];
        size_t nl = strlen(e->name);
        byte_buf_u32(directory, 0x02014b50u);
        byte_buf_u16(directory, e->mode ? 0x0314u : 20u);
        byte_buf_u16(directory, 20);
        byte_buf_u16(directory, 0x0800);
        byte_buf_u16(directory, 0);
        byte_buf_u16(directory, 0);
        byte_buf_u16(directory, DOS_DATE);
        byte_buf_u32(directory, e->crc);
        byte_buf_u32(directory, e->size);
        byte_buf_u32(directory, e->size);
        byte_buf_u16(directory, (uint32_t)nl);
        byte_buf_u16(directory, 0);
        byte_buf_u16(directory, 0);
        byte_buf_u16(directory, 0);
        byte_buf_u16(directory, 0);
        byte_buf_u32(directory, e->mode << 16);
        byte_buf_u32(directory, e->offset);
        byte_buf_put(directory, e->name, nl);
    }
    byte_buf_u32(end_record, 0x06054b50u);
    byte_buf_u16(end_record, 0);
    byte_buf_u16(end_record, 0);
    byte_buf_u16(end_record, (uint32_t)z->count);
    byte_buf_u16(end_record, (uint32_t)z->count);
    byte_buf_u32(end_record, (uint32_t)directory->size);
    byte_buf_u32(end_record, (uint32_t)z->pos);
    byte_buf_u16(end_record, 0);
    if (directory->failed || end_record->failed) { fail(z, "out of memory for", z->path); return 0; }
    return 1;
}

int zip_writer_close(zip_writer_t *z) {
    int ok = z->f ? fclose(z->f) == 0 : 1;
    z->f = NULL;
    return ok;
}

int zip_writer_finish(zip_writer_t *z) {
    byte_buf_t cd, eocd;
    byte_buf_init(&cd);
    byte_buf_init(&eocd);
    int ok = zip_writer_directory(z, &cd, &eocd) && zip_writer_append(z, cd.data, cd.size) && zip_writer_append(z, eocd.data, eocd.size);
    byte_buf_free(&cd);
    byte_buf_free(&eocd);
    if (!zip_writer_close(z)) ok = 0;
    if (!ok) {
        if (!z->error[0]) fail(z, "cannot write", z->path);
        remove(z->path);
    }
    return ok;
}

void zip_writer_free(zip_writer_t *z) {
    if (!z) return;
    if (z->f) fclose(z->f);
    release(z);
}
