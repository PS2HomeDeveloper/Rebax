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

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <time.h>

#include "filesystem/rebax_fs.h"
#include "filesystem/rebax_fs_tree.h"
#include "tool_commands.h"
#include "tool_common.h"

int cmd_mkdir(int argc, char **argv) {
    if (argc < 3) die("usage: mkdir <dir>...");
    for (int i = 2; i < argc; i++) {
        if (!rebax_fs_mkdir_p(argv[i])) die("cannot create directory '%s'", argv[i]);
    }
    return 0;
}

int cmd_rm(int argc, char **argv) {
    if (argc < 3) die("usage: rm <path>...");
    for (int i = 2; i < argc; i++) {
        if (!rebax_fs_remove_recursive(argv[i])) die("cannot remove '%s'", argv[i]);
        printf("Delete [%s]\n", argv[i]);
    }
    return 0;
}

int cmd_cp(int argc, char **argv) {
    if (argc != 4) die("usage: cp <src> <dst>");
    ensure_parent_dir(argv[3]);
    if (!rebax_fs_copy_file(argv[2], argv[3])) die("cannot copy '%s' to '%s'", argv[2], argv[3]);
    return 0;
}

typedef struct {
    const char *src;
    const char *dst;
    const char *skip[16];
    int skip_count;
    int failed;
} copy_dir_ctx_t;

static int path_has_skipped_part(const char *rel, const char *const *skip, int skip_count) {
    const char *p = rel;
    while (*p) {
        char part[512];
        size_t n = 0;
        while (*p && !is_sep(*p) && n < sizeof(part) - 1) part[n++] = *p++;
        part[n] = '\0';
        while (is_sep(*p)) p++;
        for (int i = 0; i < skip_count; i++) if (strcmp(part, skip[i]) == 0) return 1;
    }
    return 0;
}

static void copy_dir_cb(const char *path, void *user) {
    copy_dir_ctx_t *c = (copy_dir_ctx_t *)user;
    size_t sl = strlen(c->src);
    const char *rel = path + sl;
    while (is_sep(*rel)) rel++;
    if (path_has_skipped_part(rel, c->skip, c->skip_count)) return;
    char to[1700];
    snprintf(to, sizeof(to), "%s/%s", c->dst, rel);
    for (char *q = to + strlen(c->dst); *q; q++) if (*q == '\\') *q = '/';
    ensure_parent_dir(to);
    if (!rebax_fs_copy_file(path, to)) {
        fprintf(stderr, "[rebax-tool] error: cannot copy '%s' to '%s'\n", path, to);
        c->failed = 1;
    }
}

int cmd_write_text(int argc, char **argv) {
    if (argc != 4) die("usage: write-text <out_file> <text>");
    ensure_parent_dir(argv[2]);
    FILE *f = fopen(argv[2], "wb");
    if (!f) die("cannot write '%s'", argv[2]);
    const char *marker = strstr(argv[3], "@time@");
    if (marker) {
        fwrite(argv[3], 1, (size_t)(marker - argv[3]), f);
        fprintf(f, "%lld", (long long)time(NULL));
        fputs(marker + 6, f);
    } else {
        fputs(argv[3], f);
    }
    if (fclose(f) != 0) die("cannot write '%s'", argv[2]);
    return 0;
}

int cmd_copy_tree(int argc, char **argv) {
    if (argc != 4) die("usage: copy-tree <src_dir> <dst_dir>");
    if (!rebax_fs_is_dir(argv[2])) die("'%s' is not a directory", argv[2]);
    if (!rebax_fs_copy_tree(argv[2], argv[3])) die("cannot copy '%s' to '%s'", argv[2], argv[3]);
    return 0;
}

int cmd_copy_dir(int argc, char **argv) {
    if (argc < 4) die("usage: copy-dir <src_dir> <dst_dir> [--skip <name>]...");
    copy_dir_ctx_t c;
    memset(&c, 0, sizeof(c));
    c.src = argv[2];
    c.dst = argv[3];
    for (int i = 4; i + 1 < argc; i += 2) {
        if (strcmp(argv[i], "--skip") != 0 || c.skip_count >= 16) die("usage: copy-dir <src_dir> <dst_dir> [--skip <name>]...");
        c.skip[c.skip_count++] = argv[i + 1];
    }
    if (!rebax_fs_is_dir(c.src)) die("'%s' is not a directory", c.src);
    if (!rebax_fs_mkdir_p(c.dst)) die("cannot create directory '%s'", c.dst);
    rebax_fs_walk_files(c.src, copy_dir_cb, &c);
    if (c.failed) return 1;
    return 0;
}

typedef struct {
    const char *root;
    char **items;
    size_t count;
    size_t cap;
} manifest_ctx_t;

static void manifest_cb(const char *path, void *user) {
    manifest_ctx_t *m = (manifest_ctx_t *)user;
    const char *rel = path + strlen(m->root);
    while (is_sep(*rel)) rel++;
    if (m->count == m->cap) {
        m->cap = m->cap ? m->cap * 2 : 64;
        m->items = (char **)xrealloc(m->items, m->cap * sizeof(char *));
    }
    char *copy = xstrdup(rel);
    for (char *q = copy; *q; q++) if (*q == '\\') *q = '/';
    m->items[m->count++] = copy;
}

static int manifest_cmp(const void *a, const void *b) {
    return strcmp(*(char *const *)a, *(char *const *)b);
}

int cmd_asset_manifest(int argc, char **argv) {
    if (argc != 4) die("usage: asset-manifest <assets_dir> <out_file>");
    manifest_ctx_t m;
    memset(&m, 0, sizeof(m));
    m.root = argv[2];
    if (!rebax_fs_is_dir(m.root)) die("'%s' is not a directory", m.root);
    rebax_fs_walk_files(m.root, manifest_cb, &m);
    qsort(m.items, m.count, sizeof(char *), manifest_cmp);
    ensure_parent_dir(argv[3]);
    FILE *f = fopen(argv[3], "wb");
    if (!f) die("cannot write '%s'", argv[3]);
    for (size_t i = 0; i < m.count; i++) {
        const char *rel = m.items[i];
        const char *out_rel = argv[3] + strlen(m.root);
        while (is_sep(*out_rel)) out_rel++;
        if (strcmp(rel, out_rel) != 0) fprintf(f, "%s\n", rel);
        free(m.items[i]);
    }
    free(m.items);
    if (fclose(f) != 0) die("cannot write '%s'", argv[3]);
    return 0;
}

