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

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <ctype.h>
#include <unistd.h>

#if defined(_WIN32)
#include <windows.h>
#define PS2_PATH_LIST_SEP ";"
#else
#define PS2_PATH_LIST_SEP ":"
#endif

#include "exporters/compiler/ps2/ps2_build.h"
#include "exporters/common/export_internal.h"
#include "filesystem/rebax_fs.h"

#pragma GCC diagnostic ignored "-Wformat-truncation"

#define PS2_MAX_PARALLEL 4
#define PS2_MAX_STAGES 8
#define PS2_PATH 1600

typedef struct {
    char *data;
    size_t len;
    size_t cap;
} sb_t;

typedef struct {
    char *command;
    char *label;
} job_t;

typedef struct {
    job_t *jobs;
    int count;
    int cap;
} stage_t;

typedef struct {
    char *path;
    int kind;
} source_t;

enum { SRC_C, SRC_CXX, SRC_ASM_PP, SRC_ASM };

static stage_t g_stages[PS2_MAX_STAGES];
static int g_stage_count;
static int g_stage_current;
static int g_next_job;
static int g_running;
static int g_failed;
static int g_active;
static int g_parallel;
static export_shell_step_t g_slots[PS2_MAX_PARALLEL];

static char g_toolchain[PS2_PATH];
static char g_sdk_standard[PS2_PATH];
static char g_sdk_graphics[PS2_PATH];
static char *g_saved_path;

static void sb_init(sb_t *sb) {
    sb->cap = 256;
    sb->len = 0;
    sb->data = (char *)malloc(sb->cap);
    if (sb->data) sb->data[0] = '\0';
}

static void sb_puts(sb_t *sb, const char *text) {
    if (!sb->data) return;
    size_t n = strlen(text);
    if (sb->len + n + 1 > sb->cap) {
        while (sb->len + n + 1 > sb->cap) sb->cap *= 2;
        char *grown = (char *)realloc(sb->data, sb->cap);
        if (!grown) { free(sb->data); sb->data = NULL; return; }
        sb->data = grown;
    }
    memcpy(sb->data + sb->len, text, n + 1);
    sb->len += n;
}

static int is_safe_char(char c) {
    if (isalnum((unsigned char)c)) return 1;
    if (strchr("_-./:=+,@%", c) != NULL) return 1;
#if defined(_WIN32)
    if (c == '\\') return 1;
#endif
    return 0;
}

static void sb_quoted(sb_t *sb, const char *text) {
    int safe = text[0] != '\0';
    for (const char *p = text; *p; p++) {
        if (!is_safe_char(*p)) { safe = 0; break; }
    }
    if (safe) { sb_puts(sb, text); return; }
#if defined(_WIN32)
    sb_puts(sb, "\"");
    for (const char *p = text; *p; p++) {
        char one[3] = { *p, '\0', '\0' };
        if (*p == '"') { one[0] = '\\'; one[1] = '"'; }
        sb_puts(sb, one);
    }
    sb_puts(sb, "\"");
#else
    sb_puts(sb, "'");
    for (const char *p = text; *p; p++) {
        if (*p == '\'') sb_puts(sb, "'\\''");
        else { char one[2] = { *p, '\0' }; sb_puts(sb, one); }
    }
    sb_puts(sb, "'");
#endif
}

static void sb_arg(sb_t *sb, const char *text) {
    if (sb->len > 0) sb_puts(sb, " ");
    sb_quoted(sb, text);
}

static void sb_argf(sb_t *sb, const char *fmt, const char *a) {
    char buf[PS2_PATH * 2];
    snprintf(buf, sizeof(buf), fmt, a);
    sb_arg(sb, buf);
}

static const char *variable_value(const char *name) {
    if (strcmp(name, "GCC_PS2") == 0 || strcmp(name, "PS2DEV") == 0) return g_toolchain;
    if (strcmp(name, "SDK_STANDARD") == 0 || strcmp(name, "PS2SDK") == 0) return g_sdk_standard;
    if (strcmp(name, "SDK_GRAPHICS") == 0 || strcmp(name, "GSKIT") == 0) return g_sdk_graphics;
    const char *v = getenv(name);
    return v ? v : "";
}

static char *expand_token(const char *token) {
    size_t cap = strlen(token) + 1;
    for (const char *p = token; *p; p++) if (*p == '$') cap += PS2_PATH;
    char *out = (char *)malloc(cap);
    if (!out) return NULL;
    size_t n = 0;
    for (const char *p = token; *p; ) {
        if (p[0] == '$' && (p[1] == '(' || p[1] == '{')) {
            char close = p[1] == '(' ? ')' : '}';
            const char *end = strchr(p + 2, close);
            if (end != NULL && (size_t)(end - p - 2) < 64) {
                char name[64];
                size_t len = (size_t)(end - p - 2);
                memcpy(name, p + 2, len);
                name[len] = '\0';
                const char *value = variable_value(name);
                size_t vlen = strlen(value);
                if (n + vlen + 1 > cap) { free(out); return NULL; }
                memcpy(out + n, value, vlen);
                n += vlen;
                p = end + 1;
                continue;
            }
        }
        out[n++] = *p++;
    }
    out[n] = '\0';
    return out;
}

static void sb_flag_line(sb_t *sb, const char *line) {
    const char *p = line;
    while (*p) {
        while (*p == ' ' || *p == '\t') p++;
        if (!*p) break;
        const char *start = p;
        while (*p && *p != ' ' && *p != '\t') p++;
        char token[PS2_PATH];
        size_t len = (size_t)(p - start);
        if (len >= sizeof(token)) len = sizeof(token) - 1;
        memcpy(token, start, len);
        token[len] = '\0';
        char *expanded = expand_token(token);
        if (expanded != NULL) {
            if (expanded[0] != '\0') sb_arg(sb, expanded);
            free(expanded);
        }
    }
}

static void sb_flag_list(sb_t *sb, const char *const *list, int count) {
    for (int i = 0; i < count; i++) sb_flag_line(sb, list[i]);
}

#if defined(_WIN32)
#define PS2_EXE_SUFFIX ".exe"
#else
#define PS2_EXE_SUFFIX ""
#endif

static int tool_path(char *out, size_t size, const char *group, const char *prefix, const char *name) {
    snprintf(out, size, "%s/%s/bin/%s%s" PS2_EXE_SUFFIX, g_toolchain, group, prefix, name);
    if (rebax_fs_exists(out)) return 1;
    log_push("[build] FAILED: toolchain program is missing:");
    log_push(out);
    return 0;
}

static int require_directory(const char *path, const char *what) {
    if (rebax_fs_exists(path)) return 1;
    char line[PS2_PATH + 64];
    snprintf(line, sizeof(line), "[build] FAILED: %s is missing:", what);
    log_push(line);
    log_push(path);
    return 0;
}

static void stages_free(void) {
    for (int s = 0; s < g_stage_count; s++) {
        for (int j = 0; j < g_stages[s].count; j++) {
            free(g_stages[s].jobs[j].command);
            free(g_stages[s].jobs[j].label);
        }
        free(g_stages[s].jobs);
        g_stages[s].jobs = NULL;
        g_stages[s].count = 0;
        g_stages[s].cap = 0;
    }
    g_stage_count = 0;
}

static int stage_new(void) {
    if (g_stage_count >= PS2_MAX_STAGES) return -1;
    g_stages[g_stage_count].jobs = NULL;
    g_stages[g_stage_count].count = 0;
    g_stages[g_stage_count].cap = 0;
    return g_stage_count++;
}

static int stage_add(int stage, const char *label, sb_t *command) {
    if (stage < 0 || command->data == NULL) return 0;
    stage_t *s = &g_stages[stage];
    if (s->count == s->cap) {
        int cap = s->cap ? s->cap * 2 : 16;
        job_t *grown = (job_t *)realloc(s->jobs, sizeof(job_t) * (size_t)cap);
        if (!grown) return 0;
        s->jobs = grown;
        s->cap = cap;
    }
    s->jobs[s->count].command = command->data;
    s->jobs[s->count].label = strdup(label);
    command->data = NULL;
    s->count++;
    return 1;
}

static int has_suffix(const char *name, const char *suffix) {
    size_t n = strlen(name), s = strlen(suffix);
    return n >= s && strcmp(name + n - s, suffix) == 0;
}

static int classify_source(const char *name) {
    if (has_suffix(name, ".c")) return SRC_C;
    if (has_suffix(name, ".cc") || has_suffix(name, ".cpp") || has_suffix(name, ".cxx") ||
        has_suffix(name, ".cp") || has_suffix(name, ".c++")) return SRC_CXX;
    if (has_suffix(name, ".S")) return SRC_ASM_PP;
    if (has_suffix(name, ".s")) return SRC_ASM;
    return -1;
}

static int name_compare(const void *a, const void *b) {
    return strcmp((const char *)a, (const char *)b);
}

static void collect_sources(const char *root, const char *relative, source_t **list,
                            int *count, int *cap, const char *skip_dir) {
    char directory[PS2_PATH];
    snprintf(directory, sizeof(directory), "%s%s%s", root, relative[0] ? "/" : "", relative);
    DIR *dir = opendir(directory);
    if (dir == NULL) return;
    int name_cap = 256, name_count = 0;
    char (*names)[256] = (char (*)[256])malloc((size_t)name_cap * 256);
    if (names == NULL) { closedir(dir); return; }
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
        if (name_count == name_cap) {
            name_cap *= 2;
            char (*grown)[256] = (char (*)[256])realloc(names, (size_t)name_cap * 256);
            if (grown == NULL) break;
            names = grown;
        }
        snprintf(names[name_count], 256, "%s", entry->d_name);
        name_count++;
    }
    closedir(dir);
    qsort(names, (size_t)name_count, 256, name_compare);
    for (int i = 0; i < name_count; i++) {
        char child[PS2_PATH];
        snprintf(child, sizeof(child), "%s%s%s", relative, relative[0] ? "/" : "", names[i]);
        char full[PS2_PATH * 2];
        snprintf(full, sizeof(full), "%s/%s", root, child);
        struct stat st;
        if (stat(full, &st) != 0) continue;
        if (S_ISDIR(st.st_mode)) {
            if (strcmp(names[i], ".git") == 0 || strcmp(names[i], ".svn") == 0) continue;
            if (skip_dir != NULL && strcmp(child, skip_dir) == 0) continue;
            collect_sources(root, child, list, count, cap, skip_dir);
        } else if (S_ISREG(st.st_mode)) {
            int kind = classify_source(names[i]);
            if (kind < 0) continue;
            if (*count == *cap) {
                int grown_cap = *cap ? *cap * 2 : 64;
                source_t *grown = (source_t *)realloc(*list, sizeof(source_t) * (size_t)grown_cap);
                if (grown == NULL) continue;
                *list = grown;
                *cap = grown_cap;
            }
            (*list)[*count].path = strdup(child);
            (*list)[*count].kind = kind;
            (*count)++;
        }
    }
    free(names);
}

static void sources_free(source_t *list, int count) {
    for (int i = 0; i < count; i++) free(list[i].path);
    free(list);
}

static void object_path(char *out, size_t size, const char *base_dir, const char *relative) {
    snprintf(out, size, "%s/%s.o", base_dir, relative);
}

static void ensure_parent_directory(const char *file_path) {
    char dir[PS2_PATH * 2];
    snprintf(dir, sizeof(dir), "%s", file_path);
    char *slash = strrchr(dir, '/');
#if defined(_WIN32)
    char *back = strrchr(dir, '\\');
    if (back != NULL && (slash == NULL || back > slash)) slash = back;
#endif
    if (slash == NULL) return;
    *slash = '\0';
    rebax_fs_mkdir_p(dir);
}

static void set_env(const char *name, const char *value) {
#if defined(_WIN32)
    _putenv_s(name, value);
#else
    setenv(name, value, 1);
#endif
}

static void setup_environment(const ps2_build_config_t *cfg) {
    snprintf(g_toolchain, sizeof(g_toolchain), "%s", cfg->toolchain_root);
    snprintf(g_sdk_standard, sizeof(g_sdk_standard), "%s", cfg->sdk_standard_root);
    snprintf(g_sdk_graphics, sizeof(g_sdk_graphics), "%s", cfg->sdk_graphics_root);
    if (g_saved_path == NULL) {
        const char *current = getenv("PATH");
        g_saved_path = strdup(current ? current : "");
    }
    size_t cap = strlen(g_saved_path) + PS2_PATH * 4;
    char *path = (char *)malloc(cap);
    if (path == NULL) return;
    snprintf(path, cap, "%s/ee/bin" PS2_PATH_LIST_SEP "%s/iop/bin" PS2_PATH_LIST_SEP
             "%s/dvp/bin" PS2_PATH_LIST_SEP "%s",
             g_toolchain, g_toolchain, g_toolchain, g_saved_path);
    set_env("PATH", path);
    free(path);
}

typedef struct {
    char cc[PS2_PATH], cxx[PS2_PATH], as[PS2_PATH], ar[PS2_PATH], strip[PS2_PATH];
    sb_t cflags;
    sb_t cxxflags;
    sb_t incs;
    sb_t ldflags;
    sb_t asflags;
    char linkfile[PS2_PATH];
    char opt[128];
} ee_toolset_t;

static const char *or_default(const char *value, const char *fallback) {
    return (value != NULL && value[0] != '\0') ? value : fallback;
}

static int ee_toolset_init(ee_toolset_t *t, const ps2_build_config_t *cfg) {
    const char *prefix = "mips64r5900el-ps2-elf-";
    int found = 1;
    found &= tool_path(t->cc, sizeof(t->cc), "ee", prefix, "gcc");
    found &= tool_path(t->cxx, sizeof(t->cxx), "ee", prefix, "g++");
    found &= tool_path(t->as, sizeof(t->as), "ee", prefix, "as");
    found &= tool_path(t->ar, sizeof(t->ar), "ee", prefix, "ar");
    found &= tool_path(t->strip, sizeof(t->strip), "ee", prefix, "strip");
    if (!found) return 0;
    snprintf(t->opt, sizeof(t->opt), "%s", or_default(cfg->opt_flags, "-O2"));
    const char *warn = or_default(cfg->warn_flags, "-Wall");
    const char *debug = or_default(cfg->debug_flags, "-gdwarf-2 -gz");

    sb_init(&t->incs);
    char inc[PS2_PATH];
    snprintf(inc, sizeof(inc), "-I%s/ee/include", g_sdk_standard); sb_arg(&t->incs, inc);
    snprintf(inc, sizeof(inc), "-I%s/common/include", g_sdk_standard); sb_arg(&t->incs, inc);
    snprintf(inc, sizeof(inc), "-I%s/libc/include", g_sdk_standard); sb_arg(&t->incs, inc);
    snprintf(inc, sizeof(inc), "-I%s/include", g_sdk_graphics); sb_arg(&t->incs, inc);
    snprintf(inc, sizeof(inc), "-I%s", cfg->work_dir); sb_arg(&t->incs, inc);
    sb_flag_list(&t->incs, cfg->incs, cfg->inc_count);

    sb_init(&t->cflags);
    sb_arg(&t->cflags, "-D_EE");
    sb_arg(&t->cflags, "-G0");
    sb_flag_line(&t->cflags, t->opt);
    sb_flag_line(&t->cflags, warn);
    sb_flag_line(&t->cflags, debug);
    sb_flag_list(&t->cflags, cfg->cflags, cfg->cflag_count);

    sb_init(&t->cxxflags);
    sb_puts(&t->cxxflags, t->cflags.data ? t->cflags.data : "");

    sb_init(&t->asflags);
    sb_arg(&t->asflags, "-G0");

    sb_init(&t->ldflags);
    char lib[PS2_PATH];
    snprintf(lib, sizeof(lib), "-B%s/ee/mips64r5900el-ps2-elf/lib/", g_toolchain); sb_arg(&t->ldflags, lib);
    snprintf(lib, sizeof(lib), "-L%s/ee/mips64r5900el-ps2-elf/lib", g_toolchain); sb_arg(&t->ldflags, lib);
    snprintf(lib, sizeof(lib), "-B%s/ee/startup/", g_sdk_standard); sb_arg(&t->ldflags, lib);
    snprintf(lib, sizeof(lib), "-L%s/ee/startup", g_sdk_standard); sb_arg(&t->ldflags, lib);
    snprintf(lib, sizeof(lib), "-L%s/ee/lib", g_sdk_standard); sb_arg(&t->ldflags, lib);
    snprintf(lib, sizeof(lib), "-L%s/libc/lib", g_sdk_standard); sb_arg(&t->ldflags, lib);
    snprintf(lib, sizeof(lib), "-L%s/lib", g_sdk_graphics); sb_arg(&t->ldflags, lib);
    sb_arg(&t->ldflags, "-Wl,-zmax-page-size=128");
    sb_flag_list(&t->ldflags, cfg->ldflags, cfg->ldflag_count);

    if (cfg->linkfile != NULL && cfg->linkfile[0] != '\0') snprintf(t->linkfile, sizeof(t->linkfile), "%s", cfg->linkfile);
    else snprintf(t->linkfile, sizeof(t->linkfile), "%s/ee/startup/linkfile", g_sdk_standard);
    return 1;
}

static void ee_toolset_free(ee_toolset_t *t) {
    free(t->cflags.data); free(t->cxxflags.data); free(t->incs.data);
    free(t->ldflags.data); free(t->asflags.data);
}

static void ee_extra_ldflags(sb_t *sb, const ps2_build_config_t *cfg) {
    if (!cfg->newlib_nano) return;
    sb_arg(sb, "-nodefaultlibs");
    sb_arg(sb, "-lm_nano");
    sb_arg(sb, "-lgcc");
    sb_arg(sb, "-Wl,--start-group");
    sb_arg(sb, "-lc_nano");
    sb_arg(sb, "-lcdvd");
    sb_arg(sb, "-lcglue");
    sb_arg(sb, "-lpthread");
    sb_arg(sb, "-lpthreadglue");
    sb_arg(sb, "-lkernel");
    sb_arg(sb, "-Wl,--end-group");
}

static int plan_ee(const ps2_build_config_t *cfg) {
    if (!require_directory(g_sdk_standard, "the PlayStation 2 standard SDK")) return 0;
    char probe[PS2_PATH];
    snprintf(probe, sizeof(probe), "%s/ee/include", g_sdk_standard);
    if (!require_directory(probe, "the standard SDK headers")) return 0;
    snprintf(probe, sizeof(probe), "%s/libc/include", g_sdk_standard);
    if (!require_directory(probe, "the standard SDK C library headers")) return 0;

    if (cfg->kind == PS2_BUILD_EE_ELF) {
        char crt[PS2_PATH * 2];
        snprintf(crt, sizeof(crt), "%s/ee/startup/crt0.o", g_sdk_standard);
        if (!rebax_fs_exists(crt)) {
            log_push("[build] FAILED: the PlayStation 2 SDK does not contain its startup file.");
            log_pushf("[build] Missing: %s", crt);
            log_push("[build] The SDK package must ship the startup object built by the PlayStation 2 SDK.");
            return 0;
        }
    }

    source_t *sources = NULL;
    int source_count = 0, source_cap = 0;
    collect_sources(cfg->work_dir, "", &sources, &source_count, &source_cap, NULL);
    {
        int c_count = 0, cpp_count = 0;
        for (int i = 0; i < source_count; i++) {
            if (sources[i].kind == SRC_CXX) cpp_count++;
            else c_count++;
        }
        char summary[128];
        snprintf(summary, sizeof(summary), "[build] %d C/assembly and %d C++ source files to compile.", c_count, cpp_count);
        log_push(summary);
    }
    if (source_count == 0 && cfg->kind != PS2_BUILD_IOPRP) {
        log_push("[build] FAILED: no source files were found.");
        sources_free(sources, source_count);
        return 0;
    }

    ee_toolset_t t;
    if (!ee_toolset_init(&t, cfg)) {
        sources_free(sources, source_count);
        return 0;
    }
    int has_cpp = 0;
    int compile_stage = stage_new();
    int ok = 1;

    sb_t objects;
    sb_init(&objects);
    for (int i = 0; i < source_count && ok; i++) {
        char object[PS2_PATH * 2], source[PS2_PATH * 2], label[PS2_PATH];
        object_path(object, sizeof(object), cfg->work_dir, sources[i].path);
        snprintf(source, sizeof(source), "%s/%s", cfg->work_dir, sources[i].path);
        sb_arg(&objects, object);
        sb_t cmd;
        sb_init(&cmd);
        if (sources[i].kind == SRC_ASM) {
            sb_arg(&cmd, t.as);
            sb_puts(&cmd, " "); sb_puts(&cmd, t.asflags.data);
            sb_arg(&cmd, source);
        } else if (sources[i].kind == SRC_CXX) {
            has_cpp = 1;
            sb_arg(&cmd, t.cxx);
            sb_puts(&cmd, " "); sb_puts(&cmd, t.cxxflags.data);
            sb_puts(&cmd, " "); sb_puts(&cmd, t.incs.data);
            sb_arg(&cmd, "-c");
            sb_arg(&cmd, source);
        } else {
            sb_arg(&cmd, t.cc);
            sb_puts(&cmd, " "); sb_puts(&cmd, t.cflags.data);
            sb_puts(&cmd, " "); sb_puts(&cmd, t.incs.data);
            sb_arg(&cmd, "-c");
            sb_arg(&cmd, source);
        }
        sb_arg(&cmd, "-o");
        sb_arg(&cmd, object);
        ensure_parent_directory(object);
        snprintf(label, sizeof(label), "[CC] %s", sources[i].path);
        ok = stage_add(compile_stage, label, &cmd);
        free(cmd.data);
    }

    if (ok) {
        int link_stage = stage_new();
        char name[PS2_PATH], label[PS2_PATH];
        const char *base = strrchr(cfg->output, '/');
        snprintf(name, sizeof(name), "%s", base ? base + 1 : cfg->output);
        ensure_parent_directory(cfg->output);
        sb_t cmd;
        sb_init(&cmd);
        if (cfg->kind == PS2_BUILD_EE_LIB) {
            sb_arg(&cmd, t.ar);
            sb_arg(&cmd, "cru");
            sb_arg(&cmd, cfg->output);
            sb_puts(&cmd, " "); sb_puts(&cmd, objects.data);
            snprintf(label, sizeof(label), "[AR] %s", name);
            ok = stage_add(link_stage, label, &cmd);
        } else if (cfg->kind == PS2_BUILD_EE_ERL) {
            sb_arg(&cmd, t.cc);
            sb_flag_line(&cmd, t.opt);
            sb_arg(&cmd, "-o");
            sb_arg(&cmd, cfg->output);
            sb_puts(&cmd, " "); sb_puts(&cmd, objects.data);
            sb_puts(&cmd, " "); sb_puts(&cmd, t.ldflags.data);
            ee_extra_ldflags(&cmd, cfg);
            sb_arg(&cmd, "-Wl,-r");
            sb_arg(&cmd, "-Wl,-d");
            snprintf(label, sizeof(label), "[LD] %s", name);
            ok = stage_add(link_stage, label, &cmd);
            if (ok) {
                int strip_stage = stage_new();
                sb_t strip;
                sb_init(&strip);
                sb_arg(&strip, t.strip);
                sb_arg(&strip, "--strip-unneeded");
                sb_arg(&strip, "-R"); sb_arg(&strip, ".mdebug.eabi64");
                sb_arg(&strip, "-R"); sb_arg(&strip, ".reginfo");
                sb_arg(&strip, "-R"); sb_arg(&strip, ".comment");
                sb_arg(&strip, cfg->output);
                snprintf(label, sizeof(label), "[STRIP] %s", name);
                ok = stage_add(strip_stage, label, &strip);
                free(strip.data);
            }
        } else {
            sb_arg(&cmd, has_cpp ? t.cxx : t.cc);
            {
                char crt[PS2_PATH * 2];
                snprintf(crt, sizeof(crt), "%s/ee/startup/crt0.o", g_sdk_standard);
                sb_arg(&cmd, crt);
            }
            char tf[PS2_PATH + 4];
            snprintf(tf, sizeof(tf), "-T%s", t.linkfile);
            sb_arg(&cmd, tf);
            sb_flag_line(&cmd, t.opt);
            sb_arg(&cmd, "-o");
            sb_arg(&cmd, cfg->output);
            sb_puts(&cmd, " "); sb_puts(&cmd, objects.data);
            sb_puts(&cmd, " "); sb_puts(&cmd, t.ldflags.data);
            if (!has_cpp) ee_extra_ldflags(&cmd, cfg);
            sb_arg(&cmd, "-Wl,--start-group");
            sb_flag_list(&cmd, cfg->libs, cfg->lib_count);
            if (!cfg->newlib_nano) {
                sb_arg(&cmd, "-lc");
                sb_arg(&cmd, "-lcdvd");
                sb_arg(&cmd, "-lcglue");
                sb_arg(&cmd, "-lpthread");
                sb_arg(&cmd, "-lpthreadglue");
                sb_arg(&cmd, "-lkernel");
            }
            sb_arg(&cmd, "-Wl,--end-group");
            if (has_cpp) ee_extra_ldflags(&cmd, cfg);
            snprintf(label, sizeof(label), "[LD] %s", name);
            ok = stage_add(link_stage, label, &cmd);
            if (ok && cfg->release) {
                int strip_stage = stage_new();
                sb_t strip;
                sb_init(&strip);
                sb_arg(&strip, t.strip);
                sb_arg(&strip, "--strip-all");
                sb_arg(&strip, cfg->output);
                snprintf(label, sizeof(label), "[STRIP] %s", name);
                ok = stage_add(strip_stage, label, &strip);
                free(strip.data);
            }
        }
        free(cmd.data);
    }

    free(objects.data);
    ee_toolset_free(&t);
    sources_free(sources, source_count);
    return ok;
}

static int capture_line(const char *command, char *out, size_t size) {
    out[0] = '\0';
    sb_t cmd;
    sb_init(&cmd);
    sb_puts(&cmd, command);
    sb_puts(&cmd, " 2>&1");
    FILE *pipe = popen(cmd.data ? cmd.data : command, "r");
    free(cmd.data);
    if (pipe == NULL) return 0;
    if (fgets(out, (int)size, pipe) == NULL) out[0] = '\0';
    pclose(pipe);
    size_t n = strlen(out);
    while (n > 0 && (out[n - 1] == '\n' || out[n - 1] == '\r' || out[n - 1] == ' ')) out[--n] = '\0';
    return out[0] != '\0';
}

static int write_wrapped_source(const char *dest, const char *header, const char *source_file) {
    long size = 0;
    char *content = export_read_whole_file(source_file, &size);
    if (content == NULL) return 0;
    ensure_parent_directory(dest);
    FILE *f = fopen(dest, "wb");
    if (f == NULL) { free(content); return 0; }
    fprintf(f, "%s\n", header);
    fwrite(content, 1, (size_t)size, f);
    fclose(f);
    free(content);
    return 1;
}

static int plan_ioprp(const ps2_build_config_t *cfg) {
    if (cfg->ioprp_count <= 0) {
        log_push("[build] FAILED: cannot generate IOPRP when the contents list is empty.");
        return 0;
    }
    int stage = stage_new();
    sb_t cmd;
    sb_init(&cmd);
    sb_arg(&cmd, "romimg");
    sb_arg(&cmd, "-C");
    sb_arg(&cmd, cfg->output);
    for (int i = 0; i < cfg->ioprp_count; i++) sb_arg(&cmd, cfg->ioprp_contents[i]);
    ensure_parent_directory(cfg->output);
    int ok = stage_add(stage, "[IOPRP] romimg", &cmd);
    free(cmd.data);
    return ok;
}

static int plan_iop(const ps2_build_config_t *cfg) {
    char cc[PS2_PATH], as[PS2_PATH], ar[PS2_PATH], strip_tool[PS2_PATH];
    const char *iop_prefix = "mipsel-none-elf-";
    int iop_found = 1;
    iop_found &= tool_path(cc, sizeof(cc), "iop", iop_prefix, "gcc");
    iop_found &= tool_path(as, sizeof(as), "iop", iop_prefix, "as");
    iop_found &= tool_path(ar, sizeof(ar), "iop", iop_prefix, "ar");
    iop_found &= tool_path(strip_tool, sizeof(strip_tool), "iop", iop_prefix, "strip");
    if (!iop_found) return 0;

    char version[64], version_cmd[256];
    snprintf(version_cmd, sizeof(version_cmd), "%s -dumpversion", cc);
    if (!capture_line(version_cmd, version, sizeof(version))) {
        log_push("[build] FAILED: could not run the IOP compiler.");
        return 0;
    }
    int old_gcc = strcmp(version, "3.2.2") == 0 || strcmp(version, "3.2.3") == 0;

    const char *opt = or_default(cfg->opt_flags, "-Os");
    const char *warn = or_default(cfg->warn_flags, "-Wall");
    const char *debug = or_default(cfg->debug_flags, "-gdwarf-2 -gz");

    char obj_dir[PS2_PATH];
    snprintf(obj_dir, sizeof(obj_dir), "%s/obj", cfg->work_dir);

    sb_t cflags;
    sb_init(&cflags);
    sb_arg(&cflags, "-D_IOP");
    sb_arg(&cflags, "-fno-builtin");
    sb_arg(&cflags, "-G0");
    sb_flag_line(&cflags, opt);
    sb_flag_line(&cflags, warn);
    sb_flag_line(&cflags, debug);
    char inc[PS2_PATH];
    sb_flag_list(&cflags, cfg->incs, cfg->inc_count);
    snprintf(inc, sizeof(inc), "-I%s/iop/include", g_sdk_standard); sb_arg(&cflags, inc);
    snprintf(inc, sizeof(inc), "-I%s/common/include", g_sdk_standard); sb_arg(&cflags, inc);
    snprintf(inc, sizeof(inc), "-I%s", cfg->work_dir); sb_arg(&cflags, inc);
    snprintf(inc, sizeof(inc), "-I%s/include", cfg->work_dir); sb_arg(&cflags, inc);
    sb_flag_list(&cflags, cfg->cflags, cfg->cflag_count);
    if (!old_gcc) {
        sb_arg(&cflags, "-msoft-float");
        sb_arg(&cflags, "-mno-explicit-relocs");
    } else if (cfg->iop_gpopt_size != NULL && cfg->iop_gpopt_size[0] != '\0') {
        sb_arg(&cflags, "-DUSE_GP_REGISTER=1");
        sb_arg(&cflags, "-mgpopt");
        sb_argf(&cflags, "-G%s", cfg->iop_gpopt_size);
    }

    sb_t asflags;
    sb_init(&asflags);
    if (old_gcc) sb_arg(&asflags, "-march=r3000");
    sb_arg(&asflags, "-EL");
    sb_arg(&asflags, "-G0");

    source_t *sources = NULL;
    int source_count = 0, source_cap = 0;
    collect_sources(cfg->work_dir, "", &sources, &source_count, &source_cap, "obj");

    char imports_lst[PS2_PATH * 2], exports_tab[PS2_PATH * 2];
    snprintf(imports_lst, sizeof(imports_lst), "%s/imports.lst", cfg->work_dir);
    snprintf(exports_tab, sizeof(exports_tab), "%s/exports.tab", cfg->work_dir);
    int has_imports = rebax_fs_exists(imports_lst);
    int has_exports = rebax_fs_exists(exports_tab);

    int compile_stage = stage_new();
    int ok = 1;
    sb_t objects;
    sb_init(&objects);

    char exports_obj[PS2_PATH * 2], imports_obj[PS2_PATH * 2];
    snprintf(exports_obj, sizeof(exports_obj), "%s/exports.o", obj_dir);
    snprintf(imports_obj, sizeof(imports_obj), "%s/imports.o", obj_dir);

    if (has_exports) {
        char gen[PS2_PATH * 2];
        snprintf(gen, sizeof(gen), "%s/build-exports.c", obj_dir);
        ok = write_wrapped_source(gen, "#include \"irx.h\"", exports_tab);
        if (ok) {
            sb_t cmd;
            sb_init(&cmd);
            sb_arg(&cmd, cc);
            sb_puts(&cmd, " "); sb_puts(&cmd, cflags.data);
            if (!old_gcc) sb_arg(&cmd, "-fno-toplevel-reorder");
            sb_arg(&cmd, "-c"); sb_arg(&cmd, gen);
            sb_arg(&cmd, "-o"); sb_arg(&cmd, exports_obj);
            ok = stage_add(compile_stage, "[CC] exports.tab", &cmd);
            free(cmd.data);
            sb_arg(&objects, exports_obj);
        }
    }
    if (ok && has_imports) {
        char gen[PS2_PATH * 2];
        snprintf(gen, sizeof(gen), "%s/build-imports.c", obj_dir);
        ok = write_wrapped_source(gen, "#include \"irx_imports.h\"", imports_lst);
        if (ok) {
            sb_t cmd;
            sb_init(&cmd);
            sb_arg(&cmd, cc);
            sb_puts(&cmd, " "); sb_puts(&cmd, cflags.data);
            if (!old_gcc) sb_arg(&cmd, "-fno-toplevel-reorder");
            sb_arg(&cmd, "-c"); sb_arg(&cmd, gen);
            sb_arg(&cmd, "-o"); sb_arg(&cmd, imports_obj);
            ok = stage_add(compile_stage, "[CC] imports.lst", &cmd);
            free(cmd.data);
            sb_arg(&objects, imports_obj);
        }
    }
    for (int i = 0; i < source_count && ok; i++) {
        char object[PS2_PATH * 2], source[PS2_PATH * 2], label[PS2_PATH];
        object_path(object, sizeof(object), obj_dir, sources[i].path);
        snprintf(source, sizeof(source), "%s/%s", cfg->work_dir, sources[i].path);
        sb_arg(&objects, object);
        ensure_parent_directory(object);
        sb_t cmd;
        sb_init(&cmd);
        if (sources[i].kind == SRC_ASM) {
            sb_arg(&cmd, as);
            sb_puts(&cmd, " "); sb_puts(&cmd, asflags.data);
            sb_arg(&cmd, source);
        } else {
            sb_arg(&cmd, cc);
            sb_puts(&cmd, " "); sb_puts(&cmd, cflags.data);
            sb_arg(&cmd, "-c");
            sb_arg(&cmd, source);
        }
        sb_arg(&cmd, "-o");
        sb_arg(&cmd, object);
        snprintf(label, sizeof(label), "[CC] %s", sources[i].path);
        ok = stage_add(compile_stage, label, &cmd);
        free(cmd.data);
    }

    if (ok && cfg->kind == PS2_BUILD_IOP_LIB) {
        int stage = stage_new();
        sb_t cmd;
        sb_init(&cmd);
        sb_arg(&cmd, ar);
        sb_arg(&cmd, "cru");
        sb_arg(&cmd, cfg->output);
        sb_puts(&cmd, " "); sb_puts(&cmd, objects.data);
        ensure_parent_directory(cfg->output);
        ok = stage_add(stage, "[AR] iop library", &cmd);
        free(cmd.data);
    } else if (ok) {
        char linkfile[PS2_PATH], elf[PS2_PATH * 2], stripped[PS2_PATH * 2];
        if (cfg->linkfile != NULL && cfg->linkfile[0] != '\0') snprintf(linkfile, sizeof(linkfile), "%s", cfg->linkfile);
        else snprintf(linkfile, sizeof(linkfile), "%s/iop/startup/linkfile", g_sdk_standard);
        snprintf(elf, sizeof(elf), "%s", cfg->output);
        char *ext = strstr(elf, ".irx");
        if (ext != NULL) strcpy(ext, ".notiopmod.elf"); else strcat(elf, ".notiopmod.elf");
        snprintf(stripped, sizeof(stripped), "%s", cfg->output);
        ext = strstr(stripped, ".irx");
        if (ext != NULL) strcpy(ext, ".notiopmod.stripped.elf"); else strcat(stripped, ".notiopmod.stripped.elf");
        ensure_parent_directory(cfg->output);

        int link_stage = stage_new();
        sb_t cmd;
        sb_init(&cmd);
        sb_arg(&cmd, cc);
        sb_puts(&cmd, " "); sb_puts(&cmd, cflags.data);
        char tf[PS2_PATH + 4];
        snprintf(tf, sizeof(tf), "-T%s", linkfile);
        sb_arg(&cmd, tf);
        sb_flag_line(&cmd, opt);
        sb_arg(&cmd, "-o"); sb_arg(&cmd, elf);
        sb_puts(&cmd, " "); sb_puts(&cmd, objects.data);
        sb_arg(&cmd, "-nostdlib"); sb_arg(&cmd, "-dc"); sb_arg(&cmd, "-r");
        sb_flag_list(&cmd, cfg->ldflags, cfg->ldflag_count);
        sb_flag_list(&cmd, cfg->libs, cfg->lib_count);
        ok = stage_add(link_stage, "[LD] iop elf", &cmd);
        free(cmd.data);

        if (ok) {
            int strip_stage = stage_new();
            sb_init(&cmd);
            sb_arg(&cmd, strip_tool);
            sb_arg(&cmd, "--strip-unneeded");
            sb_arg(&cmd, "--remove-section=.pdr");
            sb_arg(&cmd, "--remove-section=.comment");
            sb_arg(&cmd, "--remove-section=.mdebug.abi32");
            sb_arg(&cmd, "--remove-section=.gnu.attributes");
            sb_arg(&cmd, "-o"); sb_arg(&cmd, stripped);
            sb_arg(&cmd, elf);
            ok = stage_add(strip_stage, "[STRIP] iop elf", &cmd);
            free(cmd.data);
        }
        if (ok) {
            int fix_stage = stage_new();
            sb_init(&cmd);
            sb_arg(&cmd, "iopfixup");
            sb_arg(&cmd, "--rb");
            sb_arg(&cmd, "--irx1");
            if (!has_exports) sb_arg(&cmd, "--allow-zero-text");
            sb_arg(&cmd, "-o"); sb_arg(&cmd, cfg->output);
            sb_arg(&cmd, stripped);
            ok = stage_add(fix_stage, "[IOPFIXUP] irx", &cmd);
            free(cmd.data);
        }
    }

    free(objects.data);
    free(cflags.data);
    free(asflags.data);
    sources_free(sources, source_count);
    return ok;
}

static int detect_parallel_jobs(int requested) {
    int n = requested;
    if (n <= 0) {
#if defined(_WIN32)
        SYSTEM_INFO info;
        GetSystemInfo(&info);
        n = (int)info.dwNumberOfProcessors;
#elif defined(_SC_NPROCESSORS_ONLN)
        n = (int)sysconf(_SC_NPROCESSORS_ONLN);
#else
        n = 2;
#endif
    }
    if (n < 1) n = 1;
    if (n > PS2_MAX_PARALLEL) n = PS2_MAX_PARALLEL;
    return n;
}

static int launch(int slot, const job_t *job) {
    sb_t full;
    sb_init(&full);
#if defined(_WIN32)
    sb_puts(&full, "\"");
#endif
    sb_puts(&full, job->command);
    sb_puts(&full, " 2>&1");
#if defined(_WIN32)
    sb_puts(&full, "\"");
#endif
    if (full.data == NULL) return 0;
    log_push(job->label);
    export_trace(full.data);
    int started = shell_step_start(&g_slots[slot], full.data);
    free(full.data);
    return started;
}

int ps2_build_start(const ps2_build_config_t *config) {
    ps2_build_cancel();
    stages_free();
    g_stage_current = 0;
    g_next_job = 0;
    g_running = 0;
    g_failed = 0;
    memset(g_slots, 0, sizeof(g_slots));

    setup_environment(config);
    g_parallel = detect_parallel_jobs(config->parallel_jobs);

    int ok;
    switch (config->kind) {
        case PS2_BUILD_IOP_IRX:
        case PS2_BUILD_IOP_LIB: ok = plan_iop(config); break;
        case PS2_BUILD_IOPRP:   ok = plan_ioprp(config); break;
        default:                ok = plan_ee(config); break;
    }
    if (!ok) { stages_free(); return 0; }
    g_active = 1;
    return 1;
}

int ps2_build_poll(int *out_ok) {
    if (!g_active) { *out_ok = 0; return 0; }

    for (int s = 0; s < PS2_MAX_PARALLEL; s++) {
        if (!g_slots[s].active) continue;
        int step_ok = 0;
        if (!shell_step_poll(&g_slots[s], &step_ok)) {
            g_running--;
            if (!step_ok) {
                if (!g_failed) log_push("[build] a build command failed.");
                g_failed = 1;
            }
        }
    }

    if (g_stage_current >= g_stage_count) {
        g_active = 0;
        *out_ok = !g_failed;
        return 0;
    }

    stage_t *stage = &g_stages[g_stage_current];
    while (!g_failed && g_next_job < stage->count && g_running < g_parallel) {
        int slot = -1;
        for (int s = 0; s < g_parallel; s++) if (!g_slots[s].active) { slot = s; break; }
        if (slot < 0) break;
        if (!launch(slot, &stage->jobs[g_next_job])) {
            log_push("[build] FAILED: could not start a build command.");
            g_failed = 1;
            break;
        }
        g_next_job++;
        g_running++;
    }

    if (g_running == 0 && (g_failed || g_next_job >= stage->count)) {
        if (g_failed) {
            g_active = 0;
            stages_free();
            *out_ok = 0;
            return 0;
        }
        g_stage_current++;
        g_next_job = 0;
        if (g_stage_current >= g_stage_count) {
            g_active = 0;
            stages_free();
            *out_ok = 1;
            return 0;
        }
    }
    return 1;
}

void ps2_build_cancel(void) {
    for (int s = 0; s < PS2_MAX_PARALLEL; s++) {
        if (g_slots[s].active) shell_step_cancel(&g_slots[s]);
    }
    g_running = 0;
    g_active = 0;
}
