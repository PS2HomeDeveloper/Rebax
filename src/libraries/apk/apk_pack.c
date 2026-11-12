#include "libraries/apk/apk_pack.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mbedtls/sha256.h"

#include "libraries/apk/apk_buffer.h"
#include "libraries/apk/apk_manifest.h"
#include "libraries/apk/apk_sign.h"
#include "filesystem/rebax_fs.h"

#define CHUNK 1048576u
#define COPY_BUF 262144u
#define SO_ALIGN 16384u

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

typedef struct {
    char *name;
    uint32_t crc;
    uint32_t size;
    uint32_t offset;
} entry_t;

typedef struct {
    FILE *f;
    entry_t *entries;
    int count;
    int cap;
    uint64_t pos;
    unsigned char *copy;
    uint32_t crc_table[256];
} zip_t;

static void set_err(char *err, size_t cap, const char *fmt, const char *a) {
    if (err && cap) snprintf(err, cap, fmt, a ? a : "");
}

static void crc_init(zip_t *z) {
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t c = i;
        for (int k = 0; k < 8; k++) c = (c & 1) ? (0xedb88320u ^ (c >> 1)) : (c >> 1);
        z->crc_table[i] = c;
    }
}

static uint32_t crc_update(const zip_t *z, uint32_t crc, const unsigned char *d, size_t n) {
    crc = ~crc;
    for (size_t i = 0; i < n; i++) crc = z->crc_table[(crc ^ d[i]) & 0xff] ^ (crc >> 8);
    return ~crc;
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

static int put_all(zip_t *z, const void *d, size_t n) {
    if (n == 0) return 1;
    if (fwrite(d, 1, n, z->f) != n) return 0;
    z->pos += n;
    return 1;
}

static int ends_with(const char *s, const char *suffix) {
    size_t a = strlen(s), b = strlen(suffix);
    return a >= b && strcmp(s + a - b, suffix) == 0;
}

static int begin_entry(zip_t *z, const char *name, uint32_t crc, uint32_t size) {
    if (z->count == z->cap) {
        int c = z->cap ? z->cap * 2 : 64;
        entry_t *p = (entry_t *)realloc(z->entries, (size_t)c * sizeof(entry_t));
        if (!p) return 0;
        z->entries = p;
        z->cap = c;
    }
    size_t name_len = strlen(name);
    uint32_t align = ends_with(name, ".so") ? SO_ALIGN : 4;
    uint64_t data_pos = z->pos + 30 + name_len;
    uint32_t pad = (uint32_t)((align - (data_pos % align)) % align);
    if (pad > 0 && pad < 4) pad += align;
    apk_buf_t h;
    apk_buf_init(&h);
    apk_buf_u32(&h, 0x04034b50u);
    apk_buf_u16(&h, 20);
    apk_buf_u16(&h, 0x0800);
    apk_buf_u16(&h, 0);
    apk_buf_u16(&h, 0);
    apk_buf_u16(&h, 0x0021);
    apk_buf_u32(&h, crc);
    apk_buf_u32(&h, size);
    apk_buf_u32(&h, size);
    apk_buf_u16(&h, (uint32_t)name_len);
    apk_buf_u16(&h, pad);
    apk_buf_put(&h, name, name_len);
    if (pad) {
        apk_buf_u16(&h, 0xd935);
        apk_buf_u16(&h, pad - 4);
        apk_buf_zero(&h, pad - 4);
    }
    entry_t *e = &z->entries[z->count];
    e->name = strdup(name);
    e->crc = crc;
    e->size = size;
    e->offset = (uint32_t)z->pos;
    int ok = e->name && !h.failed && put_all(z, h.data, h.size);
    apk_buf_free(&h);
    if (ok) z->count++;
    return ok;
}

static int add_memory(zip_t *z, const char *name, const unsigned char *d, size_t n) {
    if (!begin_entry(z, name, crc_update(z, 0, d, n), (uint32_t)n)) return 0;
    return put_all(z, d, n);
}

static int add_file(zip_t *z, const char *name, const char *path, char *err, size_t cap) {
    FILE *f = fopen(path, "rb");
    if (!f) { set_err(err, cap, "cannot read %s", path); return 0; }
    uint32_t crc = 0;
    uint64_t total = 0;
    size_t n;
    while ((n = fread(z->copy, 1, COPY_BUF, f)) > 0) {
        crc = crc_update(z, crc, z->copy, n);
        total += n;
    }
    if (total > 0x7fffffffu) { fclose(f); set_err(err, cap, "file is too large for an APK: %s", path); return 0; }
    fseek(f, 0, SEEK_SET);
    int ok = begin_entry(z, name, crc, (uint32_t)total);
    while (ok && (n = fread(z->copy, 1, COPY_BUF, f)) > 0) ok = put_all(z, z->copy, n);
    fclose(f);
    if (!ok) set_err(err, cap, "cannot write %s", name);
    return ok;
}

static void chunk_digest(const unsigned char *d, size_t n, unsigned char out[32], apk_buf_t *all) {
    mbedtls_sha256_context c;
    mbedtls_sha256_init(&c);
    mbedtls_sha256_starts(&c, 0);
    unsigned char head[5] = {0xa5, (unsigned char)(n & 0xff), (unsigned char)((n >> 8) & 0xff), (unsigned char)((n >> 16) & 0xff), (unsigned char)((n >> 24) & 0xff)};
    mbedtls_sha256_update(&c, head, 5);
    mbedtls_sha256_update(&c, d, n);
    mbedtls_sha256_finish(&c, out);
    mbedtls_sha256_free(&c);
    apk_buf_put(all, out, 32);
}

static uint32_t chunks_of(size_t n) {
    return (uint32_t)((n + CHUNK - 1) / CHUNK);
}

int apk_pack(const apk_pack_config_t *cfg, char *err, size_t err_cap) {
    int result = 0;
    file_list_t list;
    memset(&list, 0, sizeof(list));
    zip_t z;
    memset(&z, 0, sizeof(z));
    apk_buf_t manifest, cd, eocd, all_digests, block;
    apk_buf_init(&manifest);
    apk_buf_init(&cd);
    apk_buf_init(&eocd);
    apk_buf_init(&all_digests);
    apk_buf_init(&block);
    apk_key_t *key = NULL;
    unsigned char *chunk = NULL;

    list.prefix_len = strlen(cfg->stage_dir);
    rebax_fs_walk_files(cfg->stage_dir, collect_cb, &list);
    if (list.count == 0) { set_err(err, err_cap, "nothing to package in %s", cfg->stage_dir); goto done; }
    qsort(list.items, (size_t)list.count, sizeof(file_item_t), cmp_items);

    if (!apk_manifest_build(cfg, &manifest)) { set_err(err, err_cap, "%s", "cannot build AndroidManifest.xml"); goto done; }
    key = apk_key_open(cfg->keys_dir, err, err_cap);
    if (!key) goto done;

    z.f = fopen(cfg->out_path, "wb+");
    if (!z.f) { set_err(err, err_cap, "cannot create %s", cfg->out_path); goto done; }
    z.copy = (unsigned char *)malloc(COPY_BUF);
    chunk = (unsigned char *)malloc(CHUNK);
    if (!z.copy || !chunk) { set_err(err, err_cap, "%s", "out of memory"); goto done; }
    crc_init(&z);

    printf("Adding %d files\n", list.count + 1);
    fflush(stdout);
    if (!add_memory(&z, "AndroidManifest.xml", manifest.data, manifest.size)) { set_err(err, err_cap, "%s", "cannot write the APK"); goto done; }
    for (int i = 0; i < list.count; i++) {
        if (!add_file(&z, list.items[i].rel, list.items[i].path, err, err_cap)) goto done;
    }
    uint32_t cd_start = (uint32_t)z.pos;

    for (int i = 0; i < z.count; i++) {
        entry_t *e = &z.entries[i];
        size_t nl = strlen(e->name);
        apk_buf_u32(&cd, 0x02014b50u);
        apk_buf_u16(&cd, 20);
        apk_buf_u16(&cd, 20);
        apk_buf_u16(&cd, 0x0800);
        apk_buf_u16(&cd, 0);
        apk_buf_u16(&cd, 0);
        apk_buf_u16(&cd, 0x0021);
        apk_buf_u32(&cd, e->crc);
        apk_buf_u32(&cd, e->size);
        apk_buf_u32(&cd, e->size);
        apk_buf_u16(&cd, (uint32_t)nl);
        apk_buf_u16(&cd, 0);
        apk_buf_u16(&cd, 0);
        apk_buf_u16(&cd, 0);
        apk_buf_u16(&cd, 0);
        apk_buf_u32(&cd, 0);
        apk_buf_u32(&cd, e->offset);
        apk_buf_put(&cd, e->name, nl);
    }
    if (cd.failed) { set_err(err, err_cap, "%s", "out of memory"); goto done; }

    apk_buf_u32(&eocd, 0x06054b50u);
    apk_buf_u16(&eocd, 0);
    apk_buf_u16(&eocd, 0);
    apk_buf_u16(&eocd, (uint32_t)z.count);
    apk_buf_u16(&eocd, (uint32_t)z.count);
    apk_buf_u32(&eocd, (uint32_t)cd.size);
    apk_buf_u32(&eocd, cd_start);
    apk_buf_u16(&eocd, 0);

    printf("Signing\n");
    fflush(stdout);
    uint32_t nchunks = chunks_of(cd_start) + chunks_of(cd.size) + chunks_of(eocd.size);
    unsigned char d[32];
    fflush(z.f);
    fseek(z.f, 0, SEEK_SET);
    uint64_t left = cd_start;
    while (left > 0) {
        size_t want = left > CHUNK ? CHUNK : (size_t)left;
        if (fread(chunk, 1, want, z.f) != want) { set_err(err, err_cap, "%s", "cannot read back the APK"); goto done; }
        chunk_digest(chunk, want, d, &all_digests);
        left -= want;
    }
    for (size_t off = 0; off < cd.size; off += CHUNK) {
        size_t want = cd.size - off > CHUNK ? CHUNK : cd.size - off;
        chunk_digest(cd.data + off, want, d, &all_digests);
    }
    for (size_t off = 0; off < eocd.size; off += CHUNK) {
        size_t want = eocd.size - off > CHUNK ? CHUNK : eocd.size - off;
        chunk_digest(eocd.data + off, want, d, &all_digests);
    }
    mbedtls_sha256_context top;
    mbedtls_sha256_init(&top);
    mbedtls_sha256_starts(&top, 0);
    unsigned char head[5] = {0x5a, (unsigned char)(nchunks & 0xff), (unsigned char)((nchunks >> 8) & 0xff), (unsigned char)((nchunks >> 16) & 0xff), (unsigned char)((nchunks >> 24) & 0xff)};
    mbedtls_sha256_update(&top, head, 5);
    mbedtls_sha256_update(&top, all_digests.data, all_digests.size);
    unsigned char top_digest[32];
    mbedtls_sha256_finish(&top, top_digest);
    mbedtls_sha256_free(&top);

    if (!apk_sign_v2_block(key, top_digest, &block, err, err_cap)) goto done;

    fseek(z.f, 0, SEEK_END);
    z.pos = cd_start;
    fseek(z.f, (long)cd_start, SEEK_SET);
    uint32_t real_cd_offset = cd_start + (uint32_t)block.size;
    apk_buf_set_u32(&eocd, 16, real_cd_offset);
    if (!put_all(&z, block.data, block.size) || !put_all(&z, cd.data, cd.size) || !put_all(&z, eocd.data, eocd.size)) {
        set_err(err, err_cap, "%s", "cannot finish the APK");
        goto done;
    }
    result = 1;

done:
    if (z.f) {
        if (fclose(z.f) != 0) result = 0;
    }
    if (!result && z.f) remove(cfg->out_path);
    for (int i = 0; i < z.count; i++) free(z.entries[i].name);
    free(z.entries);
    free(z.copy);
    free(chunk);
    for (int i = 0; i < list.count; i++) {
        free(list.items[i].rel);
        free(list.items[i].path);
    }
    free(list.items);
    apk_buf_free(&manifest);
    apk_buf_free(&cd);
    apk_buf_free(&eocd);
    apk_buf_free(&all_digests);
    apk_buf_free(&block);
    apk_key_close(key);
    return result;
}
