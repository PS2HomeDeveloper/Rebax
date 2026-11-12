#include "libraries/apk/apk_axml.h"

#include <stdlib.h>
#include <string.h>

#define NS_URI "http://schemas.android.com/apk/res/android"

typedef struct {
    const char *name;
    uint32_t id;
} known_attr_t;

static const known_attr_t KNOWN[] = {
    {"theme", 0x01010000}, {"label", 0x01010001}, {"icon", 0x01010002}, {"name", 0x01010003},
    {"hasCode", 0x0101000c}, {"exported", 0x01010010}, {"launchMode", 0x0101001d},
    {"screenOrientation", 0x0101001e}, {"configChanges", 0x0101001f}, {"value", 0x01010024},
    {"minSdkVersion", 0x0101020c}, {"versionCode", 0x0101021b}, {"versionName", 0x0101021c},
    {"targetSdkVersion", 0x01010270}, {"allowBackup", 0x01010280}, {"glEsVersion", 0x01010281},
    {"required", 0x0101028e}, {"extractNativeLibs", 0x010104ea}, {"requestLegacyExternalStorage", 0x01010603},
};

enum { K_STR, K_DEC, K_HEX, K_BOOL, K_REF };

typedef struct {
    const char *name;
    uint32_t id;
    int name_str;
    int kind;
    int sidx;
    uint32_t val;
} attr_t;

typedef struct {
    int start;
    int tag;
    int first;
    int count;
} event_t;

struct apk_axml {
    char **strs;
    int nstr;
    int cstr;
    attr_t *attrs;
    int nattr;
    int cattr;
    event_t *evs;
    int nev;
    int cev;
    int *stack;
    int depth;
    int cstack;
    int failed;
};

apk_axml_t *apk_axml_new(void) {
    apk_axml_t *a = (apk_axml_t *)calloc(1, sizeof(*a));
    return a;
}

void apk_axml_free(apk_axml_t *a) {
    if (!a) return;
    for (int i = 0; i < a->nstr; i++) free(a->strs[i]);
    free(a->strs);
    free(a->attrs);
    free(a->evs);
    free(a->stack);
    free(a);
}

static int intern(apk_axml_t *a, const char *s) {
    for (int i = 0; i < a->nstr; i++) if (strcmp(a->strs[i], s) == 0) return i;
    if (a->nstr == a->cstr) {
        int c = a->cstr ? a->cstr * 2 : 32;
        char **p = (char **)realloc(a->strs, (size_t)c * sizeof(char *));
        if (!p) { a->failed = 1; return 0; }
        a->strs = p;
        a->cstr = c;
    }
    a->strs[a->nstr] = strdup(s);
    if (!a->strs[a->nstr]) { a->failed = 1; return 0; }
    return a->nstr++;
}

static event_t *add_event(apk_axml_t *a) {
    if (a->nev == a->cev) {
        int c = a->cev ? a->cev * 2 : 32;
        event_t *p = (event_t *)realloc(a->evs, (size_t)c * sizeof(event_t));
        if (!p) { a->failed = 1; return NULL; }
        a->evs = p;
        a->cev = c;
    }
    return &a->evs[a->nev++];
}

int apk_axml_start(apk_axml_t *a, const char *tag) {
    int t = intern(a, tag);
    event_t *e = add_event(a);
    if (!e) return 0;
    e->start = 1;
    e->tag = t;
    e->first = a->nattr;
    e->count = 0;
    if (a->depth == a->cstack) {
        int c = a->cstack ? a->cstack * 2 : 16;
        int *p = (int *)realloc(a->stack, (size_t)c * sizeof(int));
        if (!p) { a->failed = 1; return 0; }
        a->stack = p;
        a->cstack = c;
    }
    a->stack[a->depth++] = a->nev - 1;
    return 1;
}

int apk_axml_end(apk_axml_t *a) {
    if (a->depth == 0) { a->failed = 1; return 0; }
    int si = a->stack[--a->depth];
    int tag = a->evs[si].tag;
    event_t *e = add_event(a);
    if (!e) return 0;
    e->start = 0;
    e->tag = tag;
    e->first = 0;
    e->count = 0;
    return 1;
}

static int add_attr(apk_axml_t *a, const char *name, int kind, const char *sval, uint32_t val) {
    if (a->depth == 0) { a->failed = 1; return 0; }
    event_t *cur = &a->evs[a->stack[a->depth - 1]];
    if (cur->first + cur->count != a->nattr) { a->failed = 1; return 0; }
    uint32_t id = 0;
    if (strcmp(name, "package") != 0) {
        size_t i;
        for (i = 0; i < sizeof(KNOWN) / sizeof(KNOWN[0]); i++) {
            if (strcmp(KNOWN[i].name, name) == 0) { id = KNOWN[i].id; break; }
        }
        if (id == 0) { a->failed = 1; return 0; }
    }
    if (a->nattr == a->cattr) {
        int c = a->cattr ? a->cattr * 2 : 64;
        attr_t *p = (attr_t *)realloc(a->attrs, (size_t)c * sizeof(attr_t));
        if (!p) { a->failed = 1; return 0; }
        a->attrs = p;
        a->cattr = c;
    }
    attr_t *t = &a->attrs[a->nattr++];
    t->name = name;
    t->id = id;
    t->name_str = id ? -1 : intern(a, name);
    t->kind = kind;
    t->sidx = kind == K_STR ? intern(a, sval) : -1;
    t->val = val;
    cur->count++;
    return 1;
}

int apk_axml_attr_string(apk_axml_t *a, const char *name, const char *value) { return add_attr(a, name, K_STR, value, 0); }
int apk_axml_attr_int(apk_axml_t *a, const char *name, uint32_t value) { return add_attr(a, name, K_DEC, NULL, value); }
int apk_axml_attr_hex(apk_axml_t *a, const char *name, uint32_t value) { return add_attr(a, name, K_HEX, NULL, value); }
int apk_axml_attr_bool(apk_axml_t *a, const char *name, int value) { return add_attr(a, name, K_BOOL, NULL, value ? 0xffffffffu : 0u); }
int apk_axml_attr_ref(apk_axml_t *a, const char *name, uint32_t resource_id) { return add_attr(a, name, K_REF, NULL, resource_id); }

static int cmp_id(const void *x, const void *y) {
    uint32_t a = *(const uint32_t *)x, b = *(const uint32_t *)y;
    return a < b ? -1 : a > b;
}

static int cmp_attr(const void *x, const void *y) {
    const attr_t *a = (const attr_t *)x, *b = (const attr_t *)y;
    uint32_t ia = a->id ? a->id : 0xffffffffu, ib = b->id ? b->id : 0xffffffffu;
    return ia < ib ? -1 : ia > ib;
}

static int put_utf16(apk_buf_t *b, const char *s) {
    size_t n = strlen(s);
    uint16_t *w = (uint16_t *)malloc((n + 1) * sizeof(uint16_t));
    if (!w) return 0;
    size_t len = 0;
    for (size_t i = 0; i < n;) {
        unsigned char c = (unsigned char)s[i];
        uint32_t cp;
        if (c < 0x80) { cp = c; i += 1; }
        else if ((c & 0xe0) == 0xc0 && i + 1 < n) { cp = ((c & 0x1fu) << 6) | ((unsigned char)s[i + 1] & 0x3fu); i += 2; }
        else if ((c & 0xf0) == 0xe0 && i + 2 < n) { cp = ((c & 0x0fu) << 12) | (((unsigned char)s[i + 1] & 0x3fu) << 6) | ((unsigned char)s[i + 2] & 0x3fu); i += 3; }
        else if (i + 3 < n) { cp = ((c & 0x07u) << 18) | (((unsigned char)s[i + 1] & 0x3fu) << 12) | (((unsigned char)s[i + 2] & 0x3fu) << 6) | ((unsigned char)s[i + 3] & 0x3fu); i += 4; }
        else { cp = '?'; i += 1; }
        if (cp >= 0x10000) {
            cp -= 0x10000;
            w[len++] = (uint16_t)(0xd800 + (cp >> 10));
            w[len++] = (uint16_t)(0xdc00 + (cp & 0x3ff));
        } else {
            w[len++] = (uint16_t)cp;
        }
    }
    int ok = apk_buf_u16(b, (uint32_t)len);
    for (size_t i = 0; ok && i < len; i++) ok = apk_buf_u16(b, w[i]);
    ok = ok && apk_buf_u16(b, 0);
    free(w);
    return ok;
}

static int write_pool(apk_buf_t *out, char **pool, int n) {
    apk_buf_t body;
    apk_buf_init(&body);
    uint32_t *offs = (uint32_t *)calloc((size_t)(n ? n : 1), sizeof(uint32_t));
    if (!offs) return 0;
    int ok = 1;
    for (int i = 0; ok && i < n; i++) {
        offs[i] = (uint32_t)body.size;
        ok = put_utf16(&body, pool[i]);
    }
    while (ok && (body.size & 3)) ok = apk_buf_u8(&body, 0);
    uint32_t start = 28 + 4 * (uint32_t)n;
    uint32_t total = start + (uint32_t)body.size;
    ok = ok && apk_buf_u16(out, 0x0001) && apk_buf_u16(out, 28) && apk_buf_u32(out, total) && apk_buf_u32(out, (uint32_t)n) &&
         apk_buf_u32(out, 0) && apk_buf_u32(out, 0) && apk_buf_u32(out, start) && apk_buf_u32(out, 0);
    for (int i = 0; ok && i < n; i++) ok = apk_buf_u32(out, offs[i]);
    ok = ok && apk_buf_put(out, body.data, body.size);
    free(offs);
    apk_buf_free(&body);
    return ok;
}

static int attr_value_type(int kind) {
    switch (kind) {
        case K_STR: return 0x03;
        case K_DEC: return 0x10;
        case K_HEX: return 0x11;
        case K_BOOL: return 0x12;
        default: return 0x01;
    }
}

int apk_axml_finish(apk_axml_t *a, apk_buf_t *out) {
    if (a->failed || a->depth != 0) return 0;
    uint32_t *ids = (uint32_t *)malloc((size_t)(a->nattr ? a->nattr : 1) * sizeof(uint32_t));
    if (!ids) return 0;
    int nid = 0;
    for (int i = 0; i < a->nattr; i++) {
        if (!a->attrs[i].id) continue;
        int dup = 0;
        for (int j = 0; j < nid; j++) if (ids[j] == a->attrs[i].id) { dup = 1; break; }
        if (!dup) ids[nid++] = a->attrs[i].id;
    }
    qsort(ids, (size_t)nid, sizeof(uint32_t), cmp_id);
    int pi_prefix = intern(a, "android");
    int pi_uri = intern(a, NS_URI);
    int total = nid + a->nstr;
    char **pool = (char **)calloc((size_t)(total ? total : 1), sizeof(char *));
    if (!pool) { free(ids); return 0; }
    for (int i = 0; i < nid; i++) {
        const char *nm = NULL;
        for (int k = 0; k < a->nattr; k++) if (a->attrs[k].id == ids[i]) { nm = a->attrs[k].name; break; }
        pool[i] = (char *)nm;
    }
    for (int i = 0; i < a->nstr; i++) pool[nid + i] = a->strs[i];

    apk_buf_t body;
    apk_buf_init(&body);
    int ok = write_pool(&body, pool, total);
    if (ok && nid > 0) {
        ok = apk_buf_u16(&body, 0x0180) && apk_buf_u16(&body, 8) && apk_buf_u32(&body, 8 + 4 * (uint32_t)nid);
        for (int i = 0; ok && i < nid; i++) ok = apk_buf_u32(&body, ids[i]);
    }
    uint32_t pref = (uint32_t)(nid + pi_prefix), uri = (uint32_t)(nid + pi_uri);
    ok = ok && apk_buf_u16(&body, 0x0100) && apk_buf_u16(&body, 16) && apk_buf_u32(&body, 24) && apk_buf_u32(&body, 1) &&
         apk_buf_u32(&body, 0xffffffffu) && apk_buf_u32(&body, pref) && apk_buf_u32(&body, uri);
    for (int e = 0; ok && e < a->nev; e++) {
        event_t *ev = &a->evs[e];
        uint32_t tag = (uint32_t)(nid + ev->tag);
        if (!ev->start) {
            ok = apk_buf_u16(&body, 0x0103) && apk_buf_u16(&body, 16) && apk_buf_u32(&body, 24) && apk_buf_u32(&body, 1) &&
                 apk_buf_u32(&body, 0xffffffffu) && apk_buf_u32(&body, 0xffffffffu) && apk_buf_u32(&body, tag);
            continue;
        }
        attr_t *list = (attr_t *)malloc((size_t)(ev->count ? ev->count : 1) * sizeof(attr_t));
        if (!list) { ok = 0; break; }
        memcpy(list, &a->attrs[ev->first], (size_t)ev->count * sizeof(attr_t));
        qsort(list, (size_t)ev->count, sizeof(attr_t), cmp_attr);
        ok = apk_buf_u16(&body, 0x0102) && apk_buf_u16(&body, 16) && apk_buf_u32(&body, 36 + 20 * (uint32_t)ev->count) && apk_buf_u32(&body, 1) &&
             apk_buf_u32(&body, 0xffffffffu) && apk_buf_u32(&body, 0xffffffffu) && apk_buf_u32(&body, tag) &&
             apk_buf_u16(&body, 20) && apk_buf_u16(&body, 20) && apk_buf_u16(&body, (uint32_t)ev->count) &&
             apk_buf_u16(&body, 0) && apk_buf_u16(&body, 0) && apk_buf_u16(&body, 0);
        for (int k = 0; ok && k < ev->count; k++) {
            attr_t *t = &list[k];
            uint32_t name_idx;
            if (t->id) {
                int pos = 0;
                while (ids[pos] != t->id) pos++;
                name_idx = (uint32_t)pos;
            } else {
                name_idx = (uint32_t)(nid + t->name_str);
            }
            uint32_t ns = t->id ? uri : 0xffffffffu;
            uint32_t raw = t->kind == K_STR ? (uint32_t)(nid + t->sidx) : 0xffffffffu;
            uint32_t data = t->kind == K_STR ? raw : t->val;
            ok = apk_buf_u32(&body, ns) && apk_buf_u32(&body, name_idx) && apk_buf_u32(&body, raw) &&
                 apk_buf_u16(&body, 8) && apk_buf_u8(&body, 0) && apk_buf_u8(&body, (uint32_t)attr_value_type(t->kind)) && apk_buf_u32(&body, data);
        }
        free(list);
    }
    ok = ok && apk_buf_u16(&body, 0x0101) && apk_buf_u16(&body, 16) && apk_buf_u32(&body, 24) && apk_buf_u32(&body, 1) &&
         apk_buf_u32(&body, 0xffffffffu) && apk_buf_u32(&body, pref) && apk_buf_u32(&body, uri);
    ok = ok && apk_buf_u16(out, 0x0003) && apk_buf_u16(out, 8) && apk_buf_u32(out, (uint32_t)body.size + 8) && apk_buf_put(out, body.data, body.size);
    apk_buf_free(&body);
    free(pool);
    free(ids);
    return ok && !out->failed;
}
