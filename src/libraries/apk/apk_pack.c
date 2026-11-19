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

#include "libraries/apk/apk_pack.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mbedtls/sha256.h"

#include "filesystem/path_utils.h"
#include "filesystem/rebax_fs.h"
#include "libraries/apk/apk_manifest.h"
#include "libraries/apk/apk_sign.h"
#include "libraries/bytes/byte_buffer.h"
#include "libraries/zip/zip_writer.h"

#define CHUNK 1048576u
#define SO_ALIGN 16384u
#define DEFAULT_ALIGN 4u

typedef struct {
    char *rel;
    char *path;
} file_item_t;

typedef struct {
    file_item_t *items;
    int count;
    int cap;
    size_t prefix_len;
} file_list_t;

static void set_err(char *err, size_t cap, const char *fmt, const char *a) {
    if (err && cap) snprintf(err, cap, fmt, a ? a : "");
}

static void collect_cb(const char *path, void *user) {
    file_list_t *l = (file_list_t *)user;
    if (l->count == l->cap) {
        int c = l->cap ? l->cap * 2 : 64;
        file_item_t *p = (file_item_t *)realloc(l->items, (size_t)c * sizeof(file_item_t));
        if (!p) return;
        l->items = p;
        l->cap = c;
    }
    const char *rel = path + l->prefix_len;
    while (*rel == '/' || *rel == '\\') rel++;
    file_item_t *it = &l->items[l->count];
    it->path = strdup(path);
    it->rel = strdup(rel);
    if (!it->path || !it->rel) return;
    for (char *c = it->rel; *c; c++) if (*c == '\\') *c = '/';
    l->count++;
}

static int rank_of(const char *rel) {
    if (strcmp(rel, "classes.dex") == 0) return 0;
    if (strncmp(rel, "lib/", 4) == 0) return 1;
    if (strncmp(rel, "assets/", 7) == 0) return 2;
    return 3;
}

static int cmp_items(const void *x, const void *y) {
    const file_item_t *a = (const file_item_t *)x, *b = (const file_item_t *)y;
    int ra = rank_of(a->rel), rb = rank_of(b->rel);
    if (ra != rb) return ra - rb;
    return strcmp(a->rel, b->rel);
}

static void chunk_digest(const unsigned char *d, size_t n, byte_buf_t *all) {
    unsigned char out[32];
    unsigned char head[5] = {0xa5, (unsigned char)(n & 0xff), (unsigned char)((n >> 8) & 0xff), (unsigned char)((n >> 16) & 0xff), (unsigned char)((n >> 24) & 0xff)};
    mbedtls_sha256_context c;
    mbedtls_sha256_init(&c);
    mbedtls_sha256_starts(&c, 0);
    mbedtls_sha256_update(&c, head, 5);
    mbedtls_sha256_update(&c, d, n);
    mbedtls_sha256_finish(&c, out);
    mbedtls_sha256_free(&c);
    byte_buf_put(all, out, 32);
}

static uint32_t chunks_of(size_t n) {
    return (uint32_t)((n + CHUNK - 1) / CHUNK);
}

static void digest_memory(const byte_buf_t *b, byte_buf_t *all) {
    for (size_t off = 0; off < b->size; off += CHUNK) {
        size_t want = b->size - off > CHUNK ? CHUNK : b->size - off;
        chunk_digest(b->data + off, want, all);
    }
}

static int digest_stream(FILE *f, uint64_t length, byte_buf_t *all) {
    unsigned char *chunk = (unsigned char *)malloc(CHUNK);
    if (!chunk) return 0;
    fflush(f);
    fseek(f, 0, SEEK_SET);
    int ok = 1;
    while (ok && length > 0) {
        size_t want = length > CHUNK ? CHUNK : (size_t)length;
        if (fread(chunk, 1, want, f) != want) { ok = 0; break; }
        chunk_digest(chunk, want, all);
        length -= want;
    }
    free(chunk);
    return ok;
}

int apk_pack(const apk_pack_config_t *cfg, char *err, size_t err_cap) {
    int result = 0;
    file_list_t list;
    memset(&list, 0, sizeof(list));
    byte_buf_t manifest, cd, eocd, all_digests, block;
    byte_buf_init(&manifest);
    byte_buf_init(&cd);
    byte_buf_init(&eocd);
    byte_buf_init(&all_digests);
    byte_buf_init(&block);
    apk_key_t *key = NULL;
    zip_writer_t *zip = NULL;

    list.prefix_len = strlen(cfg->stage_dir);
    rebax_fs_walk_files(cfg->stage_dir, collect_cb, &list);
    if (list.count == 0) { set_err(err, err_cap, "nothing to package in %s", cfg->stage_dir); goto done; }
    qsort(list.items, (size_t)list.count, sizeof(file_item_t), cmp_items);

    if (!apk_manifest_build(cfg, &manifest)) { set_err(err, err_cap, "%s", "cannot build AndroidManifest.xml"); goto done; }
    key = apk_key_open(cfg->keys_dir, err, err_cap);
    if (!key) goto done;

    zip = zip_writer_create(cfg->out_path);
    if (!zip) { set_err(err, err_cap, "cannot create %s", cfg->out_path); goto done; }

    printf("Adding %d files\n", list.count + 1);
    fflush(stdout);
    if (!zip_writer_add_memory(zip, "AndroidManifest.xml", manifest.data, manifest.size, DEFAULT_ALIGN, 0)) { set_err(err, err_cap, "%s", zip_writer_error(zip)); goto done; }
    for (int i = 0; i < list.count; i++) {
        uint32_t align = path_utils_has_suffix(list.items[i].rel, ".so") ? SO_ALIGN : DEFAULT_ALIGN;
        if (!zip_writer_add_file(zip, list.items[i].rel, list.items[i].path, align, 0)) { set_err(err, err_cap, "%s", zip_writer_error(zip)); goto done; }
    }
    uint32_t cd_start = zip_writer_position(zip);
    if (!zip_writer_directory(zip, &cd, &eocd)) { set_err(err, err_cap, "%s", zip_writer_error(zip)); goto done; }

    printf("Signing\n");
    fflush(stdout);
    uint32_t nchunks = chunks_of(cd_start) + chunks_of(cd.size) + chunks_of(eocd.size);
    if (!digest_stream(zip_writer_stream(zip), cd_start, &all_digests)) { set_err(err, err_cap, "%s", "cannot read back the APK"); goto done; }
    digest_memory(&cd, &all_digests);
    digest_memory(&eocd, &all_digests);

    unsigned char head[5] = {0x5a, (unsigned char)(nchunks & 0xff), (unsigned char)((nchunks >> 8) & 0xff), (unsigned char)((nchunks >> 16) & 0xff), (unsigned char)((nchunks >> 24) & 0xff)};
    unsigned char top_digest[32];
    mbedtls_sha256_context top;
    mbedtls_sha256_init(&top);
    mbedtls_sha256_starts(&top, 0);
    mbedtls_sha256_update(&top, head, 5);
    mbedtls_sha256_update(&top, all_digests.data, all_digests.size);
    mbedtls_sha256_finish(&top, top_digest);
    mbedtls_sha256_free(&top);

    if (!apk_sign_v2_block(key, top_digest, &block, err, err_cap)) goto done;

    byte_buf_set_u32(&eocd, 16, cd_start + (uint32_t)block.size);
    fseek(zip_writer_stream(zip), 0, SEEK_END);
    if (!zip_writer_append(zip, block.data, block.size) || !zip_writer_append(zip, cd.data, cd.size) || !zip_writer_append(zip, eocd.data, eocd.size)) {
        set_err(err, err_cap, "%s", zip_writer_error(zip));
        goto done;
    }
    result = 1;

done:
    if (zip) {
        if (result) {
            if (!zip_writer_close(zip)) result = 0;
            zip_writer_free(zip);
            zip = NULL;
            if (!result) set_err(err, err_cap, "cannot finish %s", cfg->out_path);
        } else {
            zip_writer_abort(zip);
            zip = NULL;
        }
    }
    for (int i = 0; i < list.count; i++) {
        free(list.items[i].rel);
        free(list.items[i].path);
    }
    free(list.items);
    byte_buf_free(&manifest);
    byte_buf_free(&cd);
    byte_buf_free(&eocd);
    byte_buf_free(&all_digests);
    byte_buf_free(&block);
    apk_key_close(key);
    return result;
}
