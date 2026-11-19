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

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <stdarg.h>

#include "filesystem/rebax_fs.h"
#include "tool_commands.h"
#include "tool_common.h"

/* ============================================================
 * Node registry generators (port of the two awk scripts)
 * ============================================================ */

static int is_ws(char c) { return c == ' ' || c == '\t'; }

typedef struct {
    char *name;
    sb_t body;
    int count;
} prop_t;

typedef struct {
    char *type, *name, *icon, *props;
} node_entry_t;

typedef struct {
    prop_t *props;
    int prop_count;
    node_entry_t *entries;
    int entry_count;
} registry_t;

static prop_t *find_prop(registry_t *r, const char *name) {
    for (int i = 0; i < r->prop_count; i++) {
        if (strcmp(r->props[i].name, name) == 0) return &r->props[i];
    }
    return NULL;
}

static prop_t *begin_prop(registry_t *r, const char *name) {
    prop_t *p = find_prop(r, name);
    if (p) {
        p->body.len = 0;
        if (p->body.data) p->body.data[0] = 0;
        p->count = 0;
        return p;
    }
    r->props = (prop_t *)xrealloc(r->props, sizeof(prop_t) * (size_t)(r->prop_count + 1));
    p = &r->props[r->prop_count++];
    p->name = xstrdup(name);
    p->body.data = NULL;
    p->body.len = p->body.cap = 0;
    p->count = 0;
    return p;
}

/* ^static[ \t]+const[ \t]+node_property_t[ \t]+NAME\[\][ \t]*=[ \t]*\{ */
static int match_prop_begin(const char *line, char *name_out, size_t name_size) {
    const char *p = line;
    static const char *words[3] = { "static", "const", "node_property_t" };
    for (int i = 0; i < 3; i++) {
        size_t wl = strlen(words[i]);
        if (strncmp(p, words[i], wl) != 0) return 0;
        p += wl;
        if (!is_ws(*p)) return 0;
        while (is_ws(*p)) p++;
    }
    const char *start = p;
    while ((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9') || *p == '_') p++;
    size_t n = (size_t)(p - start);
    if (n == 0 || n >= name_size) return 0;
    if (p[0] != '[' || p[1] != ']') return 0;
    p += 2;
    while (is_ws(*p)) p++;
    if (*p != '=') return 0;
    p++;
    while (is_ws(*p)) p++;
    if (*p != '{') return 0;
    memcpy(name_out, start, n);
    name_out[n] = '\0';
    return 1;
}

/* pointer just after the LAST occurrence of tag (followed by whitespace when
 * need_ws), skipping the whitespace run. Returns line unchanged if none. */
static char *after_last_tag(char *line, const char *tag, int need_ws) {
    size_t tl = strlen(tag);
    char *best = NULL;
    for (char *p = strstr(line, tag); p; p = strstr(p + 1, tag)) {
        if (need_ws && !is_ws(p[tl])) continue;
        best = p;
    }
    if (!best) return line;
    char *q = best + tl;
    while (is_ws(*q)) q++;
    return q;
}

static void cut_comment_end(char *s) {
    char *e = strstr(s, "*/");
    if (!e) return;
    while (e > s && is_ws(e[-1])) e--;
    *e = '\0';
}

/* iterates the lines of a file (CR removed); fn returns nothing, state in ctx */
typedef void (*line_fn)(char *line, int first_line, void *ctx);

static void for_each_line(const char *path, line_fn fn, void *ctx) {
    size_t len = 0;
    unsigned char *data = read_file(path, &len);
    char *p = (char *)data;
    char *end = p + len;
    int first = 1;
    while (p < end) {
        char *nl = memchr(p, '\n', (size_t)(end - p));
        char *line_end = nl ? nl : end;
        char save = *line_end;
        *line_end = '\0';
        size_t ll = (size_t)(line_end - p);
        if (ll > 0 && p[ll - 1] == '\r') p[ll - 1] = '\0';
        fn(p, first, ctx);
        first = 0;
        *line_end = save;
        p = nl ? nl + 1 : end;
    }
    free(data);
}

typedef struct {
    registry_t reg;
    int in_prop_block;
    prop_t *cur_prop;
} node_parse_t;

static void node_line(char *line, int first_line, void *vctx) {
    node_parse_t *c = (node_parse_t *)vctx;

    if (first_line) {
        c->in_prop_block = 0;
        c->cur_prop = NULL;
    }

    char prop_name[512];
    if (match_prop_begin(line, prop_name, sizeof(prop_name))) {
        c->cur_prop = begin_prop(&c->reg, prop_name);
        c->in_prop_block = 1;
        return;
    }

    if (c->in_prop_block && line[0] == '}' && line[1] == ';') {
        c->in_prop_block = 0;
        return;
    }

    if (c->in_prop_block) {
        const char *t = line;
        while (is_ws(*t)) t++;
        if (*t == '{') c->cur_prop->count++;
        sb_puts(&c->cur_prop->body, line);
        sb_putc(&c->cur_prop->body, '\n');
        return;
    }

    if (strstr(line, "@NODE")) {
        char *s = after_last_tag(line, "@NODE", 1);
        cut_comment_end(s);

        const char *type = "", *name = "", *icon = "", *props = "";
        for (;;) {
            while (is_ws(*s)) s++;
            if (!*s) break;
            char *tok = s;
            while (*s && !is_ws(*s)) s++;
            if (*s) *s++ = '\0';

            char *eq = strchr(tok, '=');
            if (!eq) continue;
            *eq = '\0';
            const char *key = tok;
            char *val = eq + 1;
            size_t vl = strlen(val);
            if (val[0] == '"') { val++; vl--; }
            if (vl > 0 && val[vl - 1] == '"') val[vl - 1] = '\0';

            if (strcmp(key, "type") == 0) type = val;
            else if (strcmp(key, "name") == 0) name = val;
            else if (strcmp(key, "icon") == 0) icon = val;
            else if (strcmp(key, "properties") == 0) props = val;
        }

        c->reg.entries = (node_entry_t *)xrealloc(c->reg.entries,
                                                  sizeof(node_entry_t) * (size_t)(c->reg.entry_count + 1));
        node_entry_t *e = &c->reg.entries[c->reg.entry_count++];
        e->type = xstrdup(type);
        e->name = xstrdup(name);
        e->icon = xstrdup(icon);
        e->props = xstrdup(props);
    }
}

int cmd_node_registry(int argc, char **argv) {
    if (argc < 5) die("usage: node-registry <registry.h> <types.h> <node.c>...");
    node_parse_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    for (int i = 4; i < argc; i++) for_each_line(argv[i], node_line, &ctx);
    registry_t *r = &ctx.reg;

    sb_t out = { 0, 0, 0 };
    sb_puts(&out,
            "/* ============================================================\n"
            " * node_registry_generated.h\n"
            " * ============================================================\n"
            " * Automatically generated during the build from each @NODE line\n"
            " * in src/nodes .c files and their declared property arrays.\n"
            " * ============================================================\n"
            " */\n"
            "\n"
            "#ifndef NODE_REGISTRY_GENERATED_H\n"
            "#define NODE_REGISTRY_GENERATED_H\n"
            "\n");

    for (int e = 0; e < r->entry_count; e++) {
        const char *pv = r->entries[e].props;
        if (strcmp(pv, "NULL") != 0) {
            prop_t *p = find_prop(r, pv);
            sb_printf(&out, "static const node_property_t %s_gen[] = {\n", pv);
            if (p && p->body.len) sb_append(&out, p->body.data, p->body.len);
            sb_puts(&out, "};\n\n");
        }
    }

    sb_puts(&out, "static const node_registry_entry_t g_node_registry_table[] = {\n");
    for (int e = 0; e < r->entry_count; e++) {
        node_entry_t *en = &r->entries[e];
        if (strcmp(en->props, "NULL") == 0) {
            sb_printf(&out, "    { %s, \"%s\", ICON_%s, NULL, 0 },\n", en->type, en->name, en->icon);
        } else {
            prop_t *p = find_prop(r, en->props);
            sb_printf(&out, "    { %s, \"%s\", ICON_%s, %s_gen, %d },\n", en->type, en->name,
                      en->icon, en->props, p ? p->count : 0);
        }
    }
    sb_puts(&out, "};\n");
    sb_printf(&out, "#define NODE_REGISTRY_GENERATED_COUNT %d\n", r->entry_count);
    sb_puts(&out, "\n");
    sb_puts(&out, "#endif /* NODE_REGISTRY_GENERATED_H */\n");

    /* node_types.h: unique node types in first-seen order */
    sb_t types = { 0, 0, 0 };
    sb_puts(&types,
            "/* ============================================================\n"
            " * node_types.h\n"
            " * ============================================================\n"
            " * Automatically generated during the build from each @NODE line\n"
            " * in src/nodes .c files. Do not edit manually.\n"
            " * A new node adds a new @NODE line and appears automatically.\n"
            " * ============================================================\n"
            " */\n"
            "\n"
            "#ifndef NODE_TYPES_H\n"
            "#define NODE_TYPES_H\n"
            "\n"
            "typedef enum {\n");

    int unique = 0;
    const char **seen = (const char **)xmalloc(sizeof(char *) * (size_t)(r->entry_count + 1));
    for (int e = 0; e < r->entry_count; e++) {
        int dup = 0;
        for (int k = 0; k < unique; k++) {
            if (strcmp(seen[k], r->entries[e].type) == 0) { dup = 1; break; }
        }
        if (!dup) seen[unique++] = r->entries[e].type;
    }
    for (int u = 0; u < unique; u++) {
        sb_printf(&types, "    %s%s\n", seen[u], (u < unique - 1) ? "," : "");
    }
    sb_puts(&types, "} node_type_t;\n\n#endif /* NODE_TYPES_H */\n");

    write_file(argv[2], out.data, out.len);
    write_file(argv[3], types.data, types.len);
    sb_free(&out);
    sb_free(&types);
    free(seen);
    return 0;
}

typedef struct {
    char *type, *draw_2d, *draw_3d;
} editor_entry_t;

typedef struct {
    editor_entry_t *entries;
    int count;
} editor_parse_t;

static void editor_line(char *line, int first_line, void *vctx) {
    (void)first_line;
    editor_parse_t *c = (editor_parse_t *)vctx;
    if (!strstr(line, "@NODE_EDITOR")) return;

    char *s = after_last_tag(line, "@NODE_EDITOR", 0);
    cut_comment_end(s);

    const char *type = "", *d2 = "NULL", *d3 = "NULL";
    for (;;) {
        while (is_ws(*s)) s++;
        if (!*s) break;
        char *tok = s;
        while (*s && !is_ws(*s)) s++;
        if (*s) *s++ = '\0';

        char *eq = strchr(tok, '=');
        if (!eq) continue;
        *eq = '\0';
        const char *key = tok;
        const char *val = eq + 1;
        if (strcmp(key, "type") == 0) type = val;
        else if (strcmp(key, "draw_2d") == 0) d2 = val;
        else if (strcmp(key, "draw_3d") == 0) d3 = val;
    }
    if (type[0] == '\0') return;

    c->entries = (editor_entry_t *)xrealloc(c->entries, sizeof(editor_entry_t) * (size_t)(c->count + 1));
    editor_entry_t *e = &c->entries[c->count++];
    e->type = xstrdup(type);
    e->draw_2d = xstrdup(d2);
    e->draw_3d = xstrdup(d3);
}

int cmd_node_editor_registry(int argc, char **argv) {
    if (argc < 4) die("usage: node-editor-registry <out.h> <node_editor.c>...");
    editor_parse_t ctx = { NULL, 0 };
    for (int i = 3; i < argc; i++) for_each_line(argv[i], editor_line, &ctx);

    static const char *proto =
        "(const node_property_value_t *values, int property_count, int cam_x, int cam_y, int cam_w, int cam_h, int selected);\n";

    sb_t out = { 0, 0, 0 };
    sb_puts(&out,
            "/* ============================================================\n"
            " * node_editor_registry_generated.h\n"
            " * ============================================================\n"
            " * Automatically generated during the build from each @NODE_EDITOR\n"
            " * line in src/nodes_editor .c files.\n"
            " * ============================================================\n"
            " */\n"
            "\n"
            "#ifndef NODE_EDITOR_REGISTRY_GENERATED_H\n"
            "#define NODE_EDITOR_REGISTRY_GENERATED_H\n"
            "\n");

    for (int e = 0; e < ctx.count; e++) {
        if (strcmp(ctx.entries[e].draw_2d, "NULL") != 0) sb_printf(&out, "void %s%s", ctx.entries[e].draw_2d, proto);
        if (strcmp(ctx.entries[e].draw_3d, "NULL") != 0) sb_printf(&out, "void %s%s", ctx.entries[e].draw_3d, proto);
    }
    sb_puts(&out, "\n");
    sb_puts(&out, "static const node_editor_registry_entry_t g_node_editor_registry_table[] = {\n");
    for (int e = 0; e < ctx.count; e++) {
        sb_printf(&out, "    { %s, %s, %s },\n", ctx.entries[e].type, ctx.entries[e].draw_2d, ctx.entries[e].draw_3d);
    }
    sb_puts(&out, "};\n");
    sb_printf(&out, "#define NODE_EDITOR_REGISTRY_GENERATED_COUNT %d\n", ctx.count);
    sb_puts(&out, "\n#endif /* NODE_EDITOR_REGISTRY_GENERATED_H */\n");

    write_file(argv[2], out.data, out.len);
    sb_free(&out);
    return 0;
}

