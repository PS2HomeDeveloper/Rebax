/*
 * Direct PS2 toolchain driver.
 *
 * Replaces make and the ps2sdk Makefile.pref / Makefile.eeglobal /
 * Makefile.eeglobal_cpp / Makefile.iopglobal / Makefile.ioprp rule files.
 * Every command below is the one those rule files run, with the same flags and
 * in the same order. Nothing is written to disk except the build products.
 *
 * A build is a list of stages. Jobs inside one stage are independent and run in
 * parallel; the next stage starts when the whole stage has finished.
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

#include "ps2_build.h"
#include "export_internal.h"
#include "rebax_fs.h"

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

static char g_ps2dev[PS2_PATH];
static char g_ps2sdk[PS2_PATH];
static char g_gskit[PS2_PATH];
static char *g_saved_path;

/* ---------- string builder ---------- */

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

/* ---------- variable expansion for flag lines ($(NAME) and ${NAME}) ---------- */

static const char *variable_value(const char *name) {
    if (strcmp(name, "PS2DEV") == 0) return g_ps2dev;
    if (strcmp(name, "PS2SDK") == 0) return g_ps2sdk;
    if (strcmp(name, "GSKIT") == 0) return g_gskit;
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

/* ---------- tool names (same overridable variables as Makefile.pref) ---------- */

static void tool_name(char *out, size_t size, const char *override_var,
                      const char *prefix_var, const char *default_prefix,
                      const char *suffix) {
    const char *over = getenv(override_var);
    if (over != NULL && over[0] != '\0') {
        snprintf(out, size, "%s", over);
        return;
    }
    const char *prefix = getenv(prefix_var);
    if (prefix == NULL || prefix[0] == '\0') prefix = default_prefix;
    snprintf(out, size, "%s%s", prefix, suffix);
}

/* ---------- stages and jobs ---------- */

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

/* ---------- source discovery ---------- */

static int has_suffix(const char *name, const char *suffix) {
    size_t n = strlen(name), s = strlen(suffix);
    return n >= s && strcmp(name + n - s, suffix) == 0;
}

static int classify_source(const char *name) {
    if (has_suffix(name, ".c")) return SRC_C;
    if (has_suffix(name, ".cc") || has_suffix(name, ".cpp") || has_suffix(name, ".cxx")) return SRC_CXX;
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
            if (strcmp(names[i], ".git") == 0 || strcmp(names[i], ".svn") == 0 ||
                strcmp(names[i], "build") == 0 || strcmp(names[i], "Temp") == 0) continue;
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
    snprintf(out, size, "%s/%s", base_dir, relative);
    char *dot = strrchr(out, '.');
    char *slash = strrchr(out, '/');
    if (dot != NULL && (slash == NULL || dot > slash)) *dot = '\0';
    size_t n = strlen(out);
    if (n + 3 <= size) strcpy(out + n, ".o");
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

/* ---------- environment (PS2DEV, PS2SDK, GSKIT, PATH) ---------- */

static void set_env(const char *name, const char *value) {
#if defined(_WIN32)
    _putenv_s(name, value);
#else
    setenv(name, value, 1);
#endif
}

static void setup_environment(const char *ps2dev_root) {
    snprintf(g_ps2dev, sizeof(g_ps2dev), "%s", ps2dev_root);
    snprintf(g_ps2sdk, sizeof(g_ps2sdk), "%s/ps2sdk", ps2dev_root);
    snprintf(g_gskit, sizeof(g_gskit), "%s/gsKit", ps2dev_root);
    set_env("PS2DEV", g_ps2dev);
    set_env("PS2SDK", g_ps2sdk);
    set_env("GSKIT", g_gskit);
    if (g_saved_path == NULL) {
        const char *current = getenv("PATH");
        g_saved_path = strdup(current ? current : "");
    }
    char *path = (char *)malloc(strlen(g_saved_path) + PS2_PATH * 5);
    if (path == NULL) return;
    snprintf(path, strlen(g_saved_path) + PS2_PATH * 5,
             "%s/bin" PS2_PATH_LIST_SEP "%s/ee/bin" PS2_PATH_LIST_SEP "%s/iop/bin"
             PS2_PATH_LIST_SEP "%s/bin" PS2_PATH_LIST_SEP "%s",
             g_ps2dev, g_ps2dev, g_ps2dev, g_ps2sdk, g_saved_path);
    set_env("PATH", path);
    free(path);
}

/* ---------- EE targets (Makefile.eeglobal / Makefile.eeglobal_cpp) ---------- */

typedef struct {
    char cc[128], cxx[128], as[128], ar[128], strip[128];
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

static void ee_toolset_init(ee_toolset_t *t, const ps2_build_config_t *cfg) {
    tool_name(t->cc, sizeof(t->cc), "EE_CC", "EE_TOOL_PREFIX", "mips64r5900el-ps2-elf-", "gcc");
    tool_name(t->cxx, sizeof(t->cxx), "EE_CXX", "EE_TOOL_PREFIX", "mips64r5900el-ps2-elf-", "g++");
    tool_name(t->as, sizeof(t->as), "EE_AS", "EE_TOOL_PREFIX", "mips64r5900el-ps2-elf-", "as");
    tool_name(t->ar, sizeof(t->ar), "EE_AR", "EE_TOOL_PREFIX", "mips64r5900el-ps2-elf-", "ar");
    tool_name(t->strip, sizeof(t->strip), "EE_STRIP", "EE_TOOL_PREFIX", "mips64r5900el-ps2-elf-", "strip");
    snprintf(t->opt, sizeof(t->opt), "%s", or_default(cfg->opt_flags, "-O2"));
    const char *warn = or_default(cfg->warn_flags, "-Wall");
    const char *debug = or_default(cfg->debug_flags, "-gdwarf-2 -gz");

    sb_init(&t->incs);
    char inc[PS2_PATH];
    snprintf(inc, sizeof(inc), "-I%s/ee/include", g_ps2sdk); sb_arg(&t->incs, inc);
    snprintf(inc, sizeof(inc), "-I%s/common/include", g_ps2sdk); sb_arg(&t->incs, inc);
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
    snprintf(lib, sizeof(lib), "-L%s/ee/lib", g_ps2sdk); sb_arg(&t->ldflags, lib);
    sb_arg(&t->ldflags, "-Wl,-zmax-page-size=128");
    sb_flag_list(&t->ldflags, cfg->ldflags, cfg->ldflag_count);

    if (cfg->linkfile != NULL && cfg->linkfile[0] != '\0') snprintf(t->linkfile, sizeof(t->linkfile), "%s", cfg->linkfile);
    else snprintf(t->linkfile, sizeof(t->linkfile), "%s/ee/startup/linkfile", g_ps2sdk);
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
    source_t *sources = NULL;
    int source_count = 0, source_cap = 0;
    collect_sources(cfg->work_dir, "", &sources, &source_count, &source_cap, NULL);
    if (source_count == 0 && cfg->kind != PS2_BUILD_IOPRP) {
        log_push("[build] FAILED: no source files were found.");
        sources_free(sources, source_count);
        return 0;
    }

    ee_toolset_t t;
    ee_toolset_init(&t, cfg);
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
            sb_arg(&cmd, "-mno-crt0");
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

/* ---------- IOP targets (Makefile.iopglobal / Makefile.ioprp) ---------- */

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
    char cc[128], as[128], ar[128], strip_tool[128];
    tool_name(cc, sizeof(cc), "IOP_CC", "IOP_TOOL_PREFIX", "mipsel-none-elf-", "gcc");
    tool_name(as, sizeof(as), "IOP_AS", "IOP_TOOL_PREFIX", "mipsel-none-elf-", "as");
    tool_name(ar, sizeof(ar), "IOP_AR", "IOP_TOOL_PREFIX", "mipsel-none-elf-", "ar");
    tool_name(strip_tool, sizeof(strip_tool), "IOP_STRIP", "IOP_TOOL_PREFIX", "mipsel-none-elf-", "strip");

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
    snprintf(inc, sizeof(inc), "-I%s/iop/include", g_ps2sdk); sb_arg(&cflags, inc);
    snprintf(inc, sizeof(inc), "-I%s/common/include", g_ps2sdk); sb_arg(&cflags, inc);
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
        else snprintf(linkfile, sizeof(linkfile), "%s/iop/startup/linkfile", g_ps2sdk);
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

/* ---------- running ---------- */

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

    setup_environment(config->ps2dev_root);
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
