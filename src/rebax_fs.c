/*
 * ============================================================
 * rebax_fs.c
 * ============================================================
 * راجع rebax_fs.h للتوثيق الكامل. كل دالة هنا لها فرعان: POSIX
 * (لينكس/أندرويد/ماك macOS مستقبلاً) وWin32 حقيقي (#ifdef _WIN32) -
 * بلا أي استدعاء system()/popen() لأمر shell خارجي إطلاقاً بهذا
 * الملف، فقط استدعاءات نظام تشغيل مباشرة.
 * ============================================================
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "rebax_fs.h"

#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#else
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>
#include <errno.h>
#endif

/* ------------------------------------------------------------
 * فواصل المسار - ويندوز يقبل / و\ معاً فعلياً، لكن نكتب بـ/ دائماً
 * (نفس بقية المشروع) ونتعرّف عليهما معاً وقت القراءة تحسباً */
static int is_path_sep(char c) {
#ifdef _WIN32
    return c == '/' || c == '\\';
#else
    return c == '/';
#endif
}

int rebax_fs_exists(const char *path) {
#ifdef _WIN32
    DWORD attrs = GetFileAttributesA(path);
    return attrs != INVALID_FILE_ATTRIBUTES;
#else
    struct stat st;
    return stat(path, &st) == 0;
#endif
}

int rebax_fs_is_dir(const char *path) {
#ifdef _WIN32
    DWORD attrs = GetFileAttributesA(path);
    if (attrs == INVALID_FILE_ATTRIBUTES) return 0;
    return (attrs & FILE_ATTRIBUTE_DIRECTORY) != 0;
#else
    struct stat st;
    if (stat(path, &st) != 0) return 0;
    return S_ISDIR(st.st_mode);
#endif
}

static int make_one_dir(const char *path) {
#ifdef _WIN32
    if (CreateDirectoryA(path, NULL)) return 1;
    return GetLastError() == ERROR_ALREADY_EXISTS;
#else
    if (mkdir(path, 0755) == 0) return 1;
    return errno == EEXIST;
#endif
}

int rebax_fs_mkdir_p(const char *path) {
    if (path == NULL || path[0] == '\0') return 0;
    if (rebax_fs_is_dir(path)) return 1;

    char buf[1600];
    strncpy(buf, path, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    size_t len = strlen(buf);

    /* نبني المسار مقطعاً بمقطع - كل ما وصلنا فاصل، ننشئ المجلد
     * لحد هذي النقطة (لو ما كان موجوداً) قبل نكمل للمقطع التالي */
    for (size_t i = 1; i < len; i++) {
        if (is_path_sep(buf[i])) {
            char saved = buf[i];
            buf[i] = '\0';
            if (buf[0] != '\0' && !rebax_fs_is_dir(buf)) {
                if (!make_one_dir(buf)) return 0;
            }
            buf[i] = saved;
        }
    }
    if (!rebax_fs_is_dir(buf)) {
        if (!make_one_dir(buf)) return 0;
    }
    return 1;
}

int rebax_fs_move(const char *src_path, const char *dst_path) {
    if (!src_path || !dst_path || !rebax_fs_exists(src_path)) return 0;
    if (rebax_fs_exists(dst_path) && !rebax_fs_remove_recursive(dst_path)) return 0;
#ifdef _WIN32
    return MoveFileA(src_path, dst_path) != 0;
#else
    return rename(src_path, dst_path) == 0;
#endif
}

int rebax_fs_copy_file(const char *src_path, const char *dst_path) {
    FILE *src = fopen(src_path, "rb");
    if (src == NULL) return 0;
    FILE *dst = fopen(dst_path, "wb");
    if (dst == NULL) { fclose(src); return 0; }

    char buf[65536];
    size_t n;
    int ok = 1;
    while ((n = fread(buf, 1, sizeof(buf), src)) > 0) {
        if (fwrite(buf, 1, n, dst) != n) { ok = 0; break; }
    }
    if (ferror(src)) ok = 0;

    fclose(src);
    fclose(dst);
    if (!ok) remove(dst_path);
    return ok;
}

#ifdef _WIN32

int rebax_fs_remove_recursive(const char *path) {
    if (!rebax_fs_exists(path)) return 1;

    if (!rebax_fs_is_dir(path)) {
        return DeleteFileA(path) != 0;
    }

    char search[1600];
    snprintf(search, sizeof(search), "%s\\*", path);

    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(search, &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            if (strcmp(fd.cFileName, ".") == 0 || strcmp(fd.cFileName, "..") == 0) continue;
            char child[1600];
            snprintf(child, sizeof(child), "%s\\%s", path, fd.cFileName);
            if (!rebax_fs_remove_recursive(child)) {
                FindClose(h);
                return 0;
            }
        } while (FindNextFileA(h, &fd) != 0);
        FindClose(h);
    }

    return RemoveDirectoryA(path) != 0;
}

void rebax_fs_walk_files(const char *dir_path, rebax_fs_walk_callback_t callback, void *user_data) {
    char search[1600];
    snprintf(search, sizeof(search), "%s\\*", dir_path);

    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(search, &fd);
    if (h == INVALID_HANDLE_VALUE) return;

    do {
        if (strcmp(fd.cFileName, ".") == 0 || strcmp(fd.cFileName, "..") == 0) continue;
        char child[1600];
        snprintf(child, sizeof(child), "%s\\%s", dir_path, fd.cFileName);
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            rebax_fs_walk_files(child, callback, user_data);
        } else {
            callback(child, user_data);
        }
    } while (FindNextFileA(h, &fd) != 0);
    FindClose(h);
}

#else /* POSIX */

int rebax_fs_remove_recursive(const char *path) {
    if (!rebax_fs_exists(path)) return 1;

    if (!rebax_fs_is_dir(path)) {
        return unlink(path) == 0;
    }

    DIR *dir = opendir(path);
    if (dir == NULL) return 0;

    struct dirent *entry;
    int ok = 1;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
        char child[1600];
        snprintf(child, sizeof(child), "%s/%s", path, entry->d_name);
        if (!rebax_fs_remove_recursive(child)) { ok = 0; break; }
    }
    closedir(dir);

    if (!ok) return 0;
    return rmdir(path) == 0;
}

void rebax_fs_walk_files(const char *dir_path, rebax_fs_walk_callback_t callback, void *user_data) {
    DIR *dir = opendir(dir_path);
    if (dir == NULL) return;

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;

        char child[1600];
        snprintf(child, sizeof(child), "%s/%s", dir_path, entry->d_name);

        if (rebax_fs_is_dir(child)) {
            rebax_fs_walk_files(child, callback, user_data);
        } else {
            callback(child, user_data);
        }
    }
    closedir(dir);
}

#endif

void rebax_fs_basename(const char *path, char *out, int out_size) {
    if (path == NULL || out == NULL || out_size <= 0) return;

    const char *last = path;
    for (const char *p = path; *p != '\0'; p++) {
        if (is_path_sep(*p)) last = p + 1;
    }
    strncpy(out, last, (size_t)out_size - 1);
    out[out_size - 1] = '\0';
}

const char *rebax_fs_extension(const char *path) {
    if (path == NULL) return NULL;
    const char *dot = strrchr(path, '.');
    const char *slash1 = strrchr(path, '/');
#ifdef _WIN32
    const char *slash2 = strrchr(path, '\\');
    if (slash2 != NULL && (slash1 == NULL || slash2 > slash1)) slash1 = slash2;
#endif
    if (dot == NULL) return NULL;
    if (slash1 != NULL && dot < slash1) return NULL; /* النقطة جزء من اسم مجلد، مو الامتداد */
    if (dot[1] == '\0') return NULL; /* "file." بلا شيء بعد النقطة */
    return dot + 1;
}

void rebax_fs_basename_no_ext(const char *path, char *out, int out_size) {
    rebax_fs_basename(path, out, out_size);
    char *dot = strrchr(out, '.');
    if (dot != NULL) *dot = '\0';
}

/* ------------------------------------------------------------
 * فك .tar.xz - مرحلتان: (1) فك ضغط xz كامل لذاكرة واحدة عبر
 * xz_embedded المُتبنَّاة (نفس خوارزمية xz الحقيقية حرفياً - مؤلّفها
 * نفس مؤلّف liblzma المرجعية)، (2) تحليل تنسيق tar الناتج (كتل رأس
 * 512 بايت ثابتة) وكتابة كل ملف. الاثنتان بذاكرة كاملة مرة وحدة
 * (مو تدفّق جزء-جزء) - مقبول لأن الاستخراج يحدث مرة وحدة فقط طول
 * عمر التثبيت (راجع rebax_paths.c: علامة .extracted_ok)، فحجم
 * الذاكرة الأعلى مؤقتاً وقت الاستخراج تكلفة بسيطة مقابل بساطة/موثوقية
 * الكود مقارنة بمحلّل تدفّق متجزّئ عبر حدود القطع
 * ------------------------------------------------------------ */
#include "xz_embedded.h"

/* سقف أمان لحجم الأرشيف المفكوك. ps2dev الكاملة قد تتجاوز 512MB بعد فكها؛
 * نحتاج سقفاً أعلى، مع بقاء الحماية من الأرشيفات غير المحدودة. */
#define XZ_DECOMPRESS_MAX_SIZE ((size_t)2 * 1024 * 1024 * 1024)

static unsigned char *decompress_xz_file(const char *xz_path, size_t *out_size) {
    FILE *f = fopen(xz_path, "rb");
    if (f == NULL) return NULL;

    fseek(f, 0, SEEK_END);
    long compressed_size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (compressed_size <= 0) { fclose(f); return NULL; }

    unsigned char *compressed = malloc((size_t)compressed_size);
    if (compressed == NULL) { fclose(f); return NULL; }
    if (fread(compressed, 1, (size_t)compressed_size, f) != (size_t)compressed_size) {
        free(compressed);
        fclose(f);
        return NULL;
    }
    fclose(f);

    xz_crc32_init();
    xz_crc64_init();

    /* 64 ميجابايت لقاموس LZMA2 - يطابق أعلى مستوى ضغط قياسي لأداة
     * xz نفسها (xz -9)، يكفي أي أرشيف مضغوط بأي إعداد قياسي دونه */
    struct xz_dec *dec = xz_dec_init(XZ_DYNALLOC, 64 * 1024 * 1024);
    if (dec == NULL) { free(compressed); return NULL; }

    size_t cap = (size_t)compressed_size * 3;
    if (cap < (1 << 20)) cap = (1 << 20);
    if (cap > XZ_DECOMPRESS_MAX_SIZE) cap = XZ_DECOMPRESS_MAX_SIZE;

    unsigned char *out = malloc(cap);
    if (out == NULL) { xz_dec_end(dec); free(compressed); return NULL; }
    size_t out_used = 0;

    struct xz_buf buf;
    buf.in = compressed;
    buf.in_pos = 0;
    buf.in_size = (size_t)compressed_size;

    enum xz_ret ret = XZ_OK;
    int ok = 1;
    while (1) {
        if (out_used == cap) {
            if (cap >= XZ_DECOMPRESS_MAX_SIZE) { ok = 0; break; }
            size_t new_cap = cap * 2;
            if (new_cap > XZ_DECOMPRESS_MAX_SIZE) new_cap = XZ_DECOMPRESS_MAX_SIZE;
            unsigned char *new_out = realloc(out, new_cap);
            if (new_out == NULL) { ok = 0; break; }
            out = new_out;
            cap = new_cap;
        }
        buf.out = out;
        buf.out_pos = out_used;
        buf.out_size = cap;

        ret = xz_dec_run(dec, &buf);
        out_used = buf.out_pos;

        if (ret == XZ_STREAM_END) break;
        if (ret != XZ_OK) { ok = 0; break; }
    }

    xz_dec_end(dec);
    free(compressed);

    if (!ok) {
        fprintf(stderr, "[rebax_fs] XZ decode failed: ret=%d compressed=%ld output=%zu capacity=%zu\n",
                (int)ret, compressed_size, out_used, cap);
        free(out);
        return NULL;
    }

    *out_size = out_used;
    return out;
}

/* يحوّل حقل رقم tar الثماني (نص ASCII، مسافات بادئة، NUL أو مسافة
 * لاحقة) لـlong - نفس تنسيق كل حقول الحجم/الصلاحيات برأس tar */
static long parse_octal_field(const char *field, int len) {
    long value = 0;
    for (int i = 0; i < len; i++) {
        char c = field[i];
        if (c < '0' || c > '7') break;
        value = value * 8 + (c - '0');
    }
    return value;
}

#define TAR_BLOCK_SIZE 512

static int tar_checksum_ok(const unsigned char *h) {
    unsigned long stored=0, sum=0;
    for (int i=148;i<156;i++) if (h[i]>='0'&&h[i]<='7') stored=stored*8+(h[i]-'0');
    for (int i=0;i<512;i++) sum += (i>=148&&i<156) ? (unsigned char)' ' : h[i];
    return stored==sum;
}
static int tar_name_safe(const char *name) {
    if (!name || !name[0] || name[0]=='/' || name[0]=='\\') return 0;
    if (((name[0]>='A'&&name[0]<='Z')||(name[0]>='a'&&name[0]<='z')) && name[1]==':') return 0;
    const char *p=name;
    while (*p) {
        while (*p=='/') p++;
        const char *b=p; while (*p && *p!='/') p++;
        if ((size_t)(p-b)==2 && b[0]=='.' && b[1]=='.') return 0;
    }
    return 1;
}

int rebax_fs_extract_tar_xz(const char *xz_path, const char *dest_dir) {
    size_t data_size = 0;
    unsigned char *data = decompress_xz_file(xz_path, &data_size);
    if (data == NULL) return 0;

    if (!rebax_fs_mkdir_p(dest_dir)) { free(data); return 0; }

    int ok = 1;
    size_t pos = 0;
    char pending_long_name[1600];
    int has_pending_long_name = 0;

    while (pos + TAR_BLOCK_SIZE <= data_size) {
        const char *header = (const char *)(data + pos);

        /* كتلتا صفر متتاليتان = نهاية الأرشيف الطبيعية */
        int all_zero = 1;
        for (int i = 0; i < TAR_BLOCK_SIZE; i++) {
            if (header[i] != '\0') { all_zero = 0; break; }
        }
        if (all_zero) break;

        pos += TAR_BLOCK_SIZE;

        char typeflag = header[156];
        long size = parse_octal_field(header + 124, 12);
        if (!tar_checksum_ok((const unsigned char *)header)) { ok=0; break; }

        char name[1600];
        if (has_pending_long_name) {
            strncpy(name, pending_long_name, sizeof(name) - 1);
            name[sizeof(name) - 1] = '\0';
            has_pending_long_name = 0;
        } else {
            /* ustar: name[100] + prefix[155] (لو موجود) مفصولين بـ/ -
             * يغطي مسارات لحد ~255 محرف، كافي لأغلب الحالات الحقيقية */
            char name_field[101], prefix_field[156];
            memcpy(name_field, header, 100); name_field[100] = '\0';
            memcpy(prefix_field, header + 345, 155); prefix_field[155] = '\0';
            if (prefix_field[0] != '\0') {
                snprintf(name, sizeof(name), "%s/%s", prefix_field, name_field);
            } else {
                strncpy(name, name_field, sizeof(name) - 1);
                name[sizeof(name) - 1] = '\0';
            }
        }        if (size < 0 || !tar_name_safe(name)) { ok=0; break; }
        size_t usize=(size_t)size;
        if (usize > data_size-pos) { ok=0; break; }
        size_t padded_size=(usize+TAR_BLOCK_SIZE-1)/TAR_BLOCK_SIZE*TAR_BLOCK_SIZE;
        if (padded_size > data_size-pos) { ok=0; break; }

        if (typeflag == 'L') {
            /* امتداد GNU longname - محتوى هذي الكتلة (الاسم الطويل
             * الحقيقي) ينطبق على *الرأس التالي*، مو هذا */
            if (usize < sizeof(pending_long_name) && pos+usize<=data_size) {
                memcpy(pending_long_name,data+pos,usize);
                pending_long_name[usize]='\0';
                has_pending_long_name = 1;
            }
            pos += padded_size;
            continue;
        }

        if (pos + padded_size > data_size) { ok = 0; break; }

        char full_path[1800];
        snprintf(full_path, sizeof(full_path), "%s/%s", dest_dir, name);

        if (typeflag == '5') {
            /* مجلد */
            if (!rebax_fs_mkdir_p(full_path)) { ok = 0; break; }
        } else if (typeflag == '0' || typeflag == '\0') {
            /* ملف عادي - أنشئ المجلد الأصل أولاً (مسارات عميقة كثيرة
             * بأرشيف toolchain كامل) */
            char parent[1800];
            strncpy(parent, full_path, sizeof(parent) - 1);
            parent[sizeof(parent) - 1] = '\0';
            char *last_slash = strrchr(parent, '/');
            if (last_slash != NULL) {
                *last_slash = '\0';
                if (!rebax_fs_mkdir_p(parent)) { ok = 0; break; }
            }

            FILE *out = fopen(full_path, "wb");
            if (out == NULL) { ok = 0; break; }
            if (usize > 0) {
                if (fwrite(data+pos,1,usize,out) != usize) {
                    fclose(out);
                    ok = 0;
                    break;
                }
            }
            fclose(out);
        }
        /* أي typeflag ثاني (رابط رمزي، PAX header...) - يُتجاوز بصمت،
         * نفس ما نحتاجه فعلياً بأرشيف ps2dev/node_sources (ملفات
         * وهيدرات ومجلدات عادية بس) */

        pos += padded_size;
    }

    free(data);
    return ok;
}
