#define _POSIX_C_SOURCE 200809L
#include "exporters/common/native_discovery.h"
#include "exporters/common/export_internal.h"
#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <ctype.h>

#define DISCOVERY_MAX_SECTIONS 64
#define DISCOVERY_MAX_HEADERS 512
#define DISCOVERY_MAX_PATH 1536

typedef struct {
    char section[128];
    char root[DISCOVERY_MAX_PATH];
    char include[DISCOVERY_MAX_PATH];
    char manifest[DISCOVERY_MAX_PATH];
} discovery_section_t;

typedef struct {
    char name[256];
    char section[128];
    char libraries[512];
} discovery_header_t;

static discovery_section_t g_sections[DISCOVERY_MAX_SECTIONS];
static discovery_header_t g_headers[DISCOVERY_MAX_HEADERS];
static int g_section_count;
static int g_header_count;
static char g_unresolved[DISCOVERY_MAX_HEADERS][256];
static int g_unresolved_count;
static char g_source_root[DISCOVERY_MAX_PATH];
static int has_suffix(const char *name, const char *suffix) {
    size_t n = strlen(name), s = strlen(suffix);
    return n >= s && strcmp(name + n - s, suffix) == 0;
}
static int is_source_file(const char *name) {
    return has_suffix(name, ".c") || has_suffix(name, ".h") || has_suffix(name, ".cc") ||
           has_suffix(name, ".hh") || has_suffix(name, ".cpp") || has_suffix(name, ".hpp") ||
           has_suffix(name, ".cxx") || has_suffix(name, ".hxx");
}
static int is_system_header(const char *name) {
    static const char *const names[] = {"assert.h", "ctype.h", "errno.h", "float.h", "limits.h",
        "locale.h", "math.h", "setjmp.h", "signal.h", "stdarg.h", "stdbool.h", "stddef.h",
        "stdint.h", "stdio.h", "stdlib.h", "string.h", "time.h", "wchar.h", "iostream",
        "string", "vector", "array", "algorithm", "utility", "memory", "cmath", "cstdint",
        "cstdio", "cstdlib", "cstring", "stdint.h"};
    size_t i;
    for (i = 0; i < sizeof(names) / sizeof(names[0]); i++) if (strcmp(name, names[i]) == 0) return 1;
    return 0;
}
static void add_unique(char values[][160], int *count, const char *value) {
    int i;
    if (!value || !value[0]) return;
    for (i = 0; i < *count; i++) if (strcmp(values[i], value) == 0) return;
    if (*count < EXPORT_MAX_EXTRA_FLAGS) {
        snprintf(values[*count], 160, "%s", value);
        (*count)++;
    }
}
static void trim(char *text) {
    char *end;
    while (*text && isspace((unsigned char)*text)) memmove(text, text + 1, strlen(text));
    end = text + strlen(text);
    while (end > text && isspace((unsigned char)end[-1])) *--end = '\0';
}
static void add_unresolved(const char *name) {
    int i;
    if (!name || !name[0] || is_system_header(name) || strchr(name, '.') == NULL) return;
    for (i = 0; i < g_unresolved_count; i++) if (strcmp(g_unresolved[i], name) == 0) return;
    if (g_unresolved_count < DISCOVERY_MAX_HEADERS) snprintf(g_unresolved[g_unresolved_count++], 256, "%s", name);
}
static int file_exists(const char *path) {
    struct stat st;
    return path && stat(path, &st) == 0 && S_ISREG(st.st_mode);
}
static void add_manifest_header(const char *section, const char *name, const char *libraries) {
    int i;
    if (!name || !name[0]) return;
    for (i = 0; i < g_header_count; i++) {
        if (strcmp(g_headers[i].name, name) == 0) return;
    }
    if (g_header_count >= DISCOVERY_MAX_HEADERS) return;
    snprintf(g_headers[g_header_count].name, sizeof(g_headers[g_header_count].name), "%s", name);
    snprintf(g_headers[g_header_count].section, sizeof(g_headers[g_header_count].section), "%s", section);
    snprintf(g_headers[g_header_count].libraries, sizeof(g_headers[g_header_count].libraries), "%s", libraries ? libraries : "");
    g_header_count++;
}
static void parse_manifest(discovery_section_t *section) {
    FILE *file = fopen(section->manifest, "r");
    char line[1024];
    if (!file) return;
    while (fgets(line, sizeof(line), file)) {
        char *value, *sep;
        trim(line);
        if (strncmp(line, "header=", 7) != 0) continue;
        value = line + 7;
        sep = strchr(value, '|');
        if (sep) *sep++ = '\0'; else sep = (char *)"";
        trim(value); trim(sep);
        add_manifest_header(section->section, value, sep);
    }
    fclose(file);
}
static void discover_sections(const char *sdk_root) {
    DIR *dir = opendir(sdk_root);
    struct dirent *entry;
    if (!dir) return;
    while ((entry = readdir(dir)) != NULL && g_section_count < DISCOVERY_MAX_SECTIONS) {
        char root[DISCOVERY_MAX_PATH], include[DISCOVERY_MAX_PATH], manifest[DISCOVERY_MAX_PATH];
        struct stat st;
        if (entry->d_name[0] == '.') continue;
        snprintf(root, sizeof(root), "%s/%s", sdk_root, entry->d_name);
        snprintf(include, sizeof(include), "%s/include", root);
        snprintf(manifest, sizeof(manifest), "%s/rebax-libs.manifest", root);
        if (stat(root, &st) != 0 || !S_ISDIR(st.st_mode) || stat(include, &st) != 0 || !S_ISDIR(st.st_mode)) continue;
        snprintf(g_sections[g_section_count].section, sizeof(g_sections[g_section_count].section), "%s", entry->d_name);
        snprintf(g_sections[g_section_count].root, sizeof(g_sections[g_section_count].root), "%s", root);
        snprintf(g_sections[g_section_count].include, sizeof(g_sections[g_section_count].include), "%s", include);
        snprintf(g_sections[g_section_count].manifest, sizeof(g_sections[g_section_count].manifest), "%s", manifest);
        parse_manifest(&g_sections[g_section_count]);
        g_section_count++;
    }
    closedir(dir);
}
static int find_in_sections(const char *name, int *section_index) {
    char path[DISCOVERY_MAX_PATH];
    size_t n = strlen(name);
    int i, j;
    for (i = 0; i < g_section_count; i++) {
        snprintf(path, sizeof(path), "%s/%s", g_sections[i].include, name);
        if (file_exists(path)) { *section_index = i; return 1; }
    }
    for (j = 0; j < g_header_count; j++) {
        size_t h = strlen(g_headers[j].name);
        if (h >= n && strcmp(g_headers[j].name + h - n, name) == 0 && (h == n || g_headers[j].name[h - n - 1] == '/')) {
            for (i = 0; i < g_section_count; i++) {
                if (strcmp(g_sections[i].section, g_headers[j].section) == 0) { *section_index = i; return 1; }
            }
        }
    }
    return 0;
}
static int quoted_header_exists(const char *source, const char *name) {
    char path[DISCOVERY_MAX_PATH], directory[DISCOVERY_MAX_PATH];
    const char *slash = strrchr(source, '/');
    size_t n;
    if (!slash) return 0;
    n = (size_t)(slash - source);
    if (n >= sizeof(directory)) return 0;
    memcpy(directory, source, n); directory[n] = '\0';
    snprintf(path, sizeof(path), "%s/%s", directory, name);
    if (file_exists(path)) return 1;
    snprintf(path, sizeof(path), "%s/%s", (const char *)g_source_root, name);
    return file_exists(path);
}
static void use_header(const char *source, const char *name, int quoted) {
    int section_index = -1, j;
    const char *libs = "";
    if (quoted && quoted_header_exists(source, name)) return;
    if (!find_in_sections(name, &section_index)) { add_unresolved(name); return; }
    if (strcmp(g_sections[section_index].section, "standard") != 0) {
        char flag[160];
        snprintf(flag, sizeof(flag), "${REBAX_SDK_ROOT}/%s/include", g_sections[section_index].section);
        add_unique(g_extra_incs, &g_extra_incs_count, flag);
    }
    for (j = 0; j < g_header_count; j++) {
        if (strcmp(g_headers[j].section, g_sections[section_index].section) == 0 && g_headers[j].libraries[0]) { libs = g_headers[j].libraries; break; }
    }
    if (libs[0]) {
        char copy[512], *token, *save = NULL;
        snprintf(copy, sizeof(copy), "%s", libs);
        token = strtok_r(copy, ",; ", &save);
        while (token) {
            char flag[160];
            if (token[0] == '-') snprintf(flag, sizeof(flag), "%s", token);
            else snprintf(flag, sizeof(flag), "-l%s", token);
            add_unique(g_extra_libs, &g_extra_libs_count, flag);
            token = strtok_r(NULL, ",; ", &save);
        }
    }
}
static void scan_file(const char *path) {
    FILE *file = fopen(path, "r");
    char line[2048];
    if (!file) return;
    while (fgets(line, sizeof(line), file)) {
        char *p = strstr(line, "#include");
        char name[256];
        int quoted;
        const char *end;
        if (!p) continue;
        p += 8;
        while (*p && isspace((unsigned char)*p)) p++;
        quoted = *p == '"';
        if (!quoted && *p != '<') continue;
        p++;
        end = strchr(p, quoted ? '"' : '>');
        if (!end || (size_t)(end - p) >= sizeof(name)) continue;
        memcpy(name, p, (size_t)(end - p)); name[end - p] = '\0';
        use_header(path, name, quoted);
    }
    fclose(file);
}
static void scan_tree(const char *root) {
    DIR *dir = opendir(root);
    struct dirent *entry;
    if (!dir) return;
    while ((entry = readdir(dir)) != NULL) {
        char path[DISCOVERY_MAX_PATH];
        struct stat st;
        if (entry->d_name[0] == '.') continue;
        snprintf(path, sizeof(path), "%s/%s", root, entry->d_name);
        if (stat(path, &st) != 0) continue;
        if (S_ISDIR(st.st_mode)) scan_tree(path);
        else if (S_ISREG(st.st_mode) && is_source_file(entry->d_name)) scan_file(path);
    }
    closedir(dir);
}
int export_native_discover(const char *source_root, const char *sdk_root) {
    if (!source_root || !sdk_root) return 0;
    g_section_count = g_header_count = g_unresolved_count = 0;
    memset(g_sections, 0, sizeof(g_sections));
    memset(g_headers, 0, sizeof(g_headers));
    memset(g_unresolved, 0, sizeof(g_unresolved));
    g_source_root[0] = '\0';
    g_extra_incs_count = 0;
    g_extra_libs_count = 0;
    discover_sections(sdk_root);
    snprintf(g_source_root, sizeof(g_source_root), "%s", source_root);
    {
        char project_dir[DISCOVERY_MAX_PATH];
        struct stat st;
        snprintf(project_dir, sizeof(project_dir), "%s/project", source_root);
        scan_tree(stat(project_dir, &st) == 0 && S_ISDIR(st.st_mode) ? project_dir : source_root);
    }
    if (g_unresolved_count > 0) {
        int i;
        for (i = 0; i < g_unresolved_count; i++) log_pushf("[native] unresolved header: %s", g_unresolved[i]);
    }
    return g_unresolved_count == 0;
}
int export_native_unresolved_count(void) { return g_unresolved_count; }
const char *export_native_unresolved_at(int index) {
    if (index < 0 || index >= g_unresolved_count) return NULL;
    return g_unresolved[index];
}

static int hook_seen[3];
static const char *const HOOK_NAMES[3] = { "rebax_game_start", "rebax_game_update", "rebax_game_stop" };
static void scan_hooks(const char *root) {
    DIR *dir = opendir(root); struct dirent *e;
    if (!dir) return;
    while ((e = readdir(dir)) != NULL) {
        char path[DISCOVERY_MAX_PATH]; struct stat st;
        if (e->d_name[0] == '.') continue;
        snprintf(path, sizeof(path), "%s/%s", root, e->d_name);
        if (stat(path, &st) != 0) continue;
        if (S_ISDIR(st.st_mode)) { scan_hooks(path); continue; }
        if (!S_ISREG(st.st_mode) || !(has_suffix(e->d_name, ".c") || has_suffix(e->d_name, ".cpp") || has_suffix(e->d_name, ".cc") || has_suffix(e->d_name, ".cxx"))) continue;
        { FILE *f = fopen(path, "r"); char line[2048];
          if (!f) continue;
          while (fgets(line, sizeof(line), f)) { int i; for (i = 0; i < 3; i++) { const char *p = strstr(line, HOOK_NAMES[i]); size_t n = strlen(HOOK_NAMES[i]); if (p && (p[n] == '(' || p[n] == ' ') && (p == line || p[-1] == ' ' || p[-1] == '\t')) hook_seen[i] = 1; } }
          fclose(f); }
    }
    closedir(dir);
}
int export_native_finish(const char *src_dir) {
    char project_dir[DISCOVERY_MAX_PATH], out_path[DISCOVERY_MAX_PATH]; FILE *out;
    memset(hook_seen, 0, sizeof(hook_seen));
    if (!src_dir) return 0;
    snprintf(project_dir, sizeof(project_dir), "%s/project", src_dir);
    scan_hooks(project_dir);
    snprintf(out_path, sizeof(out_path), "%s/rebax_hooks.c", src_dir);
    out = fopen(out_path, "w"); if (!out) return 0;
    fputs("#include \"rebax.h\"\n", out);
    if (!hook_seen[0]) fputs("void rebax_game_start(void) {}\n", out);
    if (!hook_seen[1]) fputs("void rebax_game_update(float delta_time) { (void)delta_time; }\n", out);
    if (!hook_seen[2]) fputs("void rebax_game_stop(void) {}\n", out);
    return fclose(out) == 0;
}
int export_native_section_count(void) { return g_section_count; }
