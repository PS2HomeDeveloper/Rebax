#define _POSIX_C_SOURCE 200809L

#include "filesystem/rebax_archive.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>

#if !defined(_WIN32)
#include <sys/stat.h>
#include <unistd.h>
#endif

#include "filesystem/rebax_fs.h"
#include "xz_embedded.h"

#define IN_CHUNK (64u * 1024u)
#define OUT_CHUNK (256u * 1024u)
#define NAME_CAP 4096
#define PATH_BUF (NAME_CAP * 2 + 8)
#define META_CAP (64u * 1024u)
#define XZ_DICT_MAX (128u * 1024u * 1024u)

enum { ST_HEADER, ST_FILE, ST_META, ST_SKIP, ST_PAD, ST_END, ST_FAIL };

typedef struct {
    const char *dest;
    char *error;
    unsigned long error_cap;
    int state;
    unsigned char header[512];
    size_t header_fill;
    unsigned long long remaining;
    size_t pad;
    char typeflag;
    unsigned int mode;
    FILE *out;
    char out_path[PATH_BUF];
    int out_first;
    char meta[META_CAP];
    size_t meta_fill;
    char long_name[NAME_CAP];
    int has_long_name;
    char long_link[NAME_CAP];
    int has_long_link;
    char pax_path[NAME_CAP];
    int has_pax_path;
    char pax_link[NAME_CAP];
    int has_pax_link;
    char last_parent[PATH_BUF];
} tar_t;

static void set_error(tar_t *t, const char *fmt, const char *a) {
    if (t->error && t->error_cap && t->error[0] == '\0') snprintf(t->error, t->error_cap, fmt, a ? a : "");
    t->state = ST_FAIL;
}

static unsigned long long parse_number(const unsigned char *f, int len) {
    unsigned long long v = 0;
    if (f[0] & 0x80) {
        for (int i = 1; i < len; i++) v = (v << 8) | f[i];
        return v;
    }
    for (int i = 0; i < len; i++) {
        if (f[i] < '0' || f[i] > '7') {
            if (f[i] == ' ' && v == 0) continue;
            break;
        }
        v = v * 8 + (unsigned)(f[i] - '0');
    }
    return v;
}

static int checksum_ok(const unsigned char *h) {
    unsigned long stored = (unsigned long)parse_number(h + 148, 8), sum = 0;
    for (int i = 0; i < 512; i++) sum += (i >= 148 && i < 156) ? (unsigned char)' ' : h[i];
    return stored == sum;
}

static int name_safe(const char *name) {
    if (!name[0] || name[0] == '/' || name[0] == '\\') return 0;
    if (((name[0] >= 'A' && name[0] <= 'Z') || (name[0] >= 'a' && name[0] <= 'z')) && name[1] == ':') return 0;
    const char *p = name;
    while (*p) {
        while (*p == '/') p++;
        const char *b = p;
        while (*p && *p != '/') p++;
        if (p - b == 2 && b[0] == '.' && b[1] == '.') return 0;
    }
    return 1;
}

static int link_safe(const char *entry, const char *target) {
    if (!target[0] || target[0] == '/' || target[0] == '\\') return 0;
    if (((target[0] >= 'A' && target[0] <= 'Z') || (target[0] >= 'a' && target[0] <= 'z')) && target[1] == ':') return 0;
    int depth = 0;
    for (const char *p = entry; *p;) {
        while (*p == '/') p++;
        const char *b = p;
        while (*p && *p != '/') p++;
        if (p > b && *p == '/') depth++;
    }
    const char *p = target;
    while (*p) {
        while (*p == '/') p++;
        const char *b = p;
        while (*p && *p != '/') p++;
        size_t n = (size_t)(p - b);
        if (n == 2 && b[0] == '.' && b[1] == '.') {
            if (--depth < 0) return 0;
        } else if (n > 0 && !(n == 1 && b[0] == '.')) {
            depth++;
        }
    }
    return 1;
}

static int make_parent(tar_t *t, const char *full) {
    char parent[PATH_BUF];
    snprintf(parent, sizeof(parent), "%s", full);
    char *slash = strrchr(parent, '/');
    if (!slash) return 1;
    *slash = '\0';
    if (strcmp(parent, t->last_parent) == 0) return 1;
    if (!rebax_fs_mkdir_p(parent)) return 0;
    snprintf(t->last_parent, sizeof(t->last_parent), "%s", parent);
    return 1;
}

static void parse_pax(tar_t *t) {
    size_t i = 0;
    t->meta[t->meta_fill < META_CAP ? t->meta_fill : META_CAP - 1] = '\0';
    while (i < t->meta_fill) {
        size_t len = 0;
        size_t j = i;
        while (j < t->meta_fill && t->meta[j] >= '0' && t->meta[j] <= '9') len = len * 10 + (size_t)(t->meta[j++] - '0');
        if (j >= t->meta_fill || t->meta[j] != ' ' || len == 0 || i + len > t->meta_fill) break;
        char *rec = t->meta + j + 1;
        size_t rec_len = i + len - (j + 1);
        if (rec_len > 0 && rec[rec_len - 1] == '\n') rec_len--;
        if (rec_len > 5 && strncmp(rec, "path=", 5) == 0 && rec_len - 5 < NAME_CAP) {
            memcpy(t->pax_path, rec + 5, rec_len - 5);
            t->pax_path[rec_len - 5] = '\0';
            t->has_pax_path = 1;
        } else if (rec_len > 9 && strncmp(rec, "linkpath=", 9) == 0 && rec_len - 9 < NAME_CAP) {
            memcpy(t->pax_link, rec + 9, rec_len - 9);
            t->pax_link[rec_len - 9] = '\0';
            t->has_pax_link = 1;
        }
        i += len;
    }
}

static void finish_file(tar_t *t) {
    if (t->out) {
        fclose(t->out);
        t->out = NULL;
    }
}

static void begin_entry(tar_t *t) {
    const unsigned char *h = t->header;
    int all_zero = 1;
    for (int i = 0; i < 512; i++) if (h[i]) { all_zero = 0; break; }
    if (all_zero) {
        t->state = ST_END;
        return;
    }
    if (!checksum_ok(h)) {
        set_error(t, "the archive header is damaged%s", "");
        return;
    }
    t->typeflag = (char)h[156];
    t->remaining = parse_number(h + 124, 12);
    t->mode = (unsigned int)parse_number(h + 100, 8) & 0777u;
    t->pad = (size_t)((512 - (t->remaining % 512)) % 512);

    if (t->typeflag == 'L' || t->typeflag == 'K' || t->typeflag == 'x') {
        if (t->remaining >= META_CAP) {
            set_error(t, "an archive header block is too large%s", "");
            return;
        }
        t->meta_fill = 0;
        t->state = t->remaining ? ST_META : ST_PAD;
        if (!t->remaining) t->state = ST_HEADER;
        return;
    }
    if (t->typeflag == 'g') {
        t->state = t->remaining ? ST_SKIP : ST_HEADER;
        if (t->remaining == 0 && t->pad) t->state = ST_PAD;
        return;
    }

    char name[NAME_CAP];
    if (t->has_long_name) {
        snprintf(name, sizeof(name), "%s", t->long_name);
    } else if (t->has_pax_path) {
        snprintf(name, sizeof(name), "%s", t->pax_path);
    } else {
        char n[101], p[156];
        memcpy(n, h, 100);
        n[100] = '\0';
        memcpy(p, h + 345, 155);
        p[155] = '\0';
        if (p[0] && memcmp(h + 257, "ustar", 5) == 0) snprintf(name, sizeof(name), "%s/%s", p, n);
        else snprintf(name, sizeof(name), "%s", n);
    }
    char link[NAME_CAP];
    if (t->has_long_link) {
        snprintf(link, sizeof(link), "%s", t->long_link);
    } else if (t->has_pax_link) {
        snprintf(link, sizeof(link), "%s", t->pax_link);
    } else {
        char l[101];
        memcpy(l, h + 157, 100);
        l[100] = '\0';
        snprintf(link, sizeof(link), "%s", l);
    }
    t->has_long_name = t->has_long_link = t->has_pax_path = t->has_pax_link = 0;

    size_t nl = strlen(name);
    while (nl > 1 && name[nl - 1] == '/') name[--nl] = '\0';
    if (!name_safe(name)) {
        set_error(t, "the archive contains an unsafe path: %s", name);
        return;
    }
    char full[PATH_BUF];
    snprintf(full, sizeof(full), "%s/%s", t->dest, name);

    t->state = t->remaining ? ST_SKIP : ST_HEADER;
    if (t->state == ST_HEADER && t->pad) t->state = ST_PAD;

    if (t->typeflag == '5') {
        if (!rebax_fs_mkdir_p(full)) set_error(t, "cannot create the folder %s", full);
        t->state = t->remaining ? ST_SKIP : ST_HEADER;
        return;
    }
    if (t->typeflag == '0' || t->typeflag == '\0' || t->typeflag == '7') {
        if (!make_parent(t, full)) {
            set_error(t, "cannot create the folder for %s", full);
            return;
        }
        t->out = fopen(full, "wb");
        if (!t->out) {
            set_error(t, "cannot write the file %s", full);
            return;
        }
        snprintf(t->out_path, sizeof(t->out_path), "%s", full);
        t->out_first = 1;
        if (t->remaining) {
            t->state = ST_FILE;
        } else {
            finish_file(t);
#if !defined(_WIN32)
            chmod(full, (mode_t)(t->mode ? t->mode : 0644u));
#endif
            t->state = t->pad ? ST_PAD : ST_HEADER;
        }
        return;
    }
    if (t->typeflag == '2' || t->typeflag == '1') {
        if (!make_parent(t, full)) {
            set_error(t, "cannot create the folder for %s", full);
            return;
        }
        if (t->typeflag == '2') {
            if (!link_safe(name, link)) {
                set_error(t, "the archive contains an unsafe link: %s", name);
                return;
            }
#if !defined(_WIN32)
            unlink(full);
            if (symlink(link, full) != 0) {
                set_error(t, "cannot create the link %s", full);
                return;
            }
#endif
        } else {
            if (!name_safe(link)) {
                set_error(t, "the archive contains an unsafe link: %s", name);
                return;
            }
            char target[PATH_BUF];
            snprintf(target, sizeof(target), "%s/%s", t->dest, link);
            if (!rebax_fs_copy_file(target, full)) {
                set_error(t, "cannot copy the linked file %s", full);
                return;
            }
#if !defined(_WIN32)
            {
                struct stat st;
                if (stat(target, &st) == 0) chmod(full, st.st_mode & 0777);
            }
#endif
        }
        t->state = t->remaining ? ST_SKIP : ST_HEADER;
        if (t->state == ST_HEADER && t->pad) t->state = ST_PAD;
    }
}

static void tar_feed(tar_t *t, const unsigned char *data, size_t len) {
    size_t pos = 0;
    while (pos < len && t->state != ST_FAIL && t->state != ST_END) {
        size_t avail = len - pos;
        if (t->state == ST_HEADER) {
            size_t need = 512 - t->header_fill;
            size_t take = avail < need ? avail : need;
            memcpy(t->header + t->header_fill, data + pos, take);
            t->header_fill += take;
            pos += take;
            if (t->header_fill == 512) {
                t->header_fill = 0;
                begin_entry(t);
            }
        } else if (t->state == ST_FILE) {
            size_t take = avail < t->remaining ? avail : (size_t)t->remaining;
            if (t->out_first) {
                t->out_first = 0;
#if !defined(_WIN32)
                if (take >= 4 && ((data[pos] == 0x7F && data[pos + 1] == 'E' && data[pos + 2] == 'L' && data[pos + 3] == 'F') ||
                                  (data[pos] == '#' && data[pos + 1] == '!')))
                    t->mode |= 0755u;
#endif
            }
            if (fwrite(data + pos, 1, take, t->out) != take) {
                finish_file(t);
                set_error(t, "cannot write the file %s (disk full?)", t->out_path);
                return;
            }
            pos += take;
            t->remaining -= take;
            if (t->remaining == 0) {
                finish_file(t);
#if !defined(_WIN32)
                chmod(t->out_path, (mode_t)(t->mode ? t->mode : 0644u));
#endif
                t->state = t->pad ? ST_PAD : ST_HEADER;
            }
        } else if (t->state == ST_META) {
            size_t take = avail < t->remaining ? avail : (size_t)t->remaining;
            memcpy(t->meta + t->meta_fill, data + pos, take);
            t->meta_fill += take;
            pos += take;
            t->remaining -= take;
            if (t->remaining == 0) {
                t->meta[t->meta_fill] = '\0';
                if (t->typeflag == 'L') {
                    snprintf(t->long_name, sizeof(t->long_name), "%.4095s", t->meta);
                    t->has_long_name = 1;
                } else if (t->typeflag == 'K') {
                    snprintf(t->long_link, sizeof(t->long_link), "%.4095s", t->meta);
                    t->has_long_link = 1;
                } else {
                    parse_pax(t);
                }
                t->state = t->pad ? ST_PAD : ST_HEADER;
            }
        } else if (t->state == ST_SKIP) {
            size_t take = avail < t->remaining ? avail : (size_t)t->remaining;
            pos += take;
            t->remaining -= take;
            if (t->remaining == 0) t->state = t->pad ? ST_PAD : ST_HEADER;
        } else if (t->state == ST_PAD) {
            size_t take = avail < t->pad ? avail : t->pad;
            pos += take;
            t->pad -= take;
            if (t->pad == 0) t->state = ST_HEADER;
        }
    }
}

int rebax_archive_extract_tar_xz(const char *xz_path, const char *dest_dir, rebax_progress_fn progress, void *user, char *error, unsigned long error_cap) {
    if (error && error_cap) error[0] = '\0';
    FILE *f = fopen(xz_path, "rb");
    if (!f) {
        if (error && error_cap) snprintf(error, error_cap, "cannot open %s", xz_path);
        return 0;
    }
    fseek(f, 0, SEEK_END);
    long total = ftell(f);
    fseek(f, 0, SEEK_SET);

    int ok = 0;
    tar_t *t = calloc(1, sizeof(*t));
    unsigned char *in = malloc(IN_CHUNK);
    unsigned char *out = malloc(OUT_CHUNK);
    struct xz_dec *dec = NULL;
    if (!t || !in || !out) {
        if (error && error_cap) snprintf(error, error_cap, "not enough memory");
        goto done;
    }
    t->dest = dest_dir;
    t->error = error;
    t->error_cap = error_cap;
    t->state = ST_HEADER;
    if (!rebax_fs_mkdir_p(dest_dir)) {
        set_error(t, "cannot create the folder %s", dest_dir);
        goto done;
    }

    xz_crc32_init();
    xz_crc64_init();
    dec = xz_dec_init(XZ_DYNALLOC, XZ_DICT_MAX);
    if (!dec) {
        set_error(t, "not enough memory%s", "");
        goto done;
    }

    struct xz_buf b;
    b.in = in;
    b.in_pos = 0;
    b.in_size = 0;
    b.out = out;
    b.out_pos = 0;
    b.out_size = OUT_CHUNK;
    long consumed = 0;
    int eof = 0;
    for (;;) {
        if (b.in_pos == b.in_size && !eof) {
            size_t n = fread(in, 1, IN_CHUNK, f);
            consumed += (long)n;
            b.in_pos = 0;
            b.in_size = n;
            if (n == 0) eof = 1;
            if (progress) progress((unsigned long long)consumed, (unsigned long long)total, user);
        }
        enum xz_ret r = xz_dec_run(dec, &b);
        if (b.out_pos > 0) {
            tar_feed(t, out, b.out_pos);
            b.out_pos = 0;
        }
        if (t->state == ST_FAIL) goto done;
        if (r == XZ_STREAM_END) {
            ok = (t->state == ST_END || t->state == ST_HEADER);
            if (!ok) set_error(t, "the archive ended unexpectedly%s", "");
            break;
        }
        if (r != XZ_OK) {
            if (r == XZ_BUF_ERROR && eof) set_error(t, "the archive is truncated%s", "");
            else if (r == XZ_MEMLIMIT_ERROR) set_error(t, "the archive needs more memory than allowed%s", "");
            else set_error(t, "the archive is damaged or not .tar.xz%s", "");
            goto done;
        }
    }

done:
    if (t) finish_file(t);
    if (dec) xz_dec_end(dec);
    free(in);
    free(out);
    free(t);
    fclose(f);
    if (!ok && error && error_cap && error[0] == '\0') snprintf(error, error_cap, "extraction failed");
    return ok;
}

int rebax_archive_install(const char *xz_path, const char *dest_dir, rebax_progress_fn progress, void *user, char *error, unsigned long error_cap) {
    char tmp[PATH_BUF], only[PATH_BUF], parent[PATH_BUF];
    snprintf(tmp, sizeof(tmp), "%s.rebax-tmp", dest_dir);
    snprintf(parent, sizeof(parent), "%s", dest_dir);
    char *slash = strrchr(parent, '/');
    if (slash) {
        *slash = '\0';
        rebax_fs_mkdir_p(parent);
    }
    rebax_fs_remove_recursive(tmp);
    rebax_fs_remove_recursive(dest_dir);
    if (!rebax_archive_extract_tar_xz(xz_path, tmp, progress, user, error, error_cap)) {
        rebax_fs_remove_recursive(tmp);
        return 0;
    }
    int entries = 0;
    only[0] = '\0';
    DIR *d = opendir(tmp);
    if (d) {
        struct dirent *e;
        while ((e = readdir(d)) != NULL) {
            if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0) continue;
            entries++;
            snprintf(only, sizeof(only), "%s/%.255s", tmp, e->d_name);
        }
        closedir(d);
    }
    int ok = 0;
    if (entries == 1 && rebax_fs_is_dir(only)) {
        ok = rebax_fs_move(only, dest_dir);
        rebax_fs_remove_recursive(tmp);
    } else if (entries > 0) {
        ok = rebax_fs_move(tmp, dest_dir);
    }
    if (!ok) {
        rebax_fs_remove_recursive(tmp);
        rebax_fs_remove_recursive(dest_dir);
        if (error && error_cap && error[0] == '\0') snprintf(error, error_cap, "the package is empty or damaged");
    }
    return ok;
}
