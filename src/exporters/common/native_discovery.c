/*
 * Rebax Engine native dependency discovery.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */
#define _POSIX_C_SOURCE 200809L
#include "exporters/common/native_discovery.h"
#include "exporters/common/export_internal.h"
#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

static int seen_section[3];
static void add_section(const char *sdk, int section) {
    const char *names[] = {"standard", "runtime", "graphics"};
    char inc[160];
    if (seen_section[section]) return;
    snprintf(inc, sizeof(inc), "${REBAX_SDK_ROOT}/%s/include", names[section]);
    if (g_extra_incs_count < EXPORT_MAX_EXTRA_FLAGS) {
        snprintf(g_extra_incs[g_extra_incs_count++], 160, "%s", inc);
    }
    seen_section[section] = 1;
    if (section == 2 && g_extra_libs_count < EXPORT_MAX_EXTRA_FLAGS) {
        char manifest[512]; char text[4096]; size_t n;
        snprintf(manifest, sizeof(manifest), "%s/graphics/rebax-libs.manifest", sdk);
        FILE *f = fopen(manifest, "r");
        if (f != NULL) {
            n = fread(text, 1, sizeof(text) - 1, f); fclose(f); text[n] = '\0';
            if (strstr(text, "SDL2") != NULL) snprintf(g_extra_libs[g_extra_libs_count++], 160, "-lSDL2");
        }
    }
}
static void scan_file(const char *path, const char *sdk) {
    FILE *f = fopen(path, "r"); char line[1024];
    if (!f) return;
    while (fgets(line, sizeof(line), f)) {
        char name[256]; const char *p = strstr(line, "#include");
        if (!p) continue;
        p = strchr(p, '<'); if (!p) p = strchr(line, '"');
        if (!p) continue; p++;
        const char *end = strchr(p, p[-1] == '<' ? '>' : '"');
        if (!end || (size_t)(end - p) >= sizeof(name)) continue;
        memcpy(name, p, (size_t)(end - p)); name[end - p] = '\0';
        if (!strncmp(name, "SDL.h", 5) || !strncmp(name, "SDL2/", 5)) add_section(sdk, 2);
        else if (!strncmp(name, "rebax", 5) || !strncmp(name, "node_", 5) || !strncmp(name, "scene_", 6)) add_section(sdk, 1);
        else if (strchr(name, '/') == NULL) add_section(sdk, 0);
    }
    fclose(f);
}
static void walk(const char *root, const char *sdk) {
    DIR *dir = opendir(root); struct dirent *e;
    if (!dir) return;
    while ((e = readdir(dir)) != NULL) {
        if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
        char path[1536]; struct stat st;
        snprintf(path, sizeof(path), "%s/%s", root, e->d_name);
        if (stat(path, &st) != 0) continue;
        if (S_ISDIR(st.st_mode)) walk(path, sdk);
        else if (strstr(e->d_name, ".c") || strstr(e->d_name, ".h") || strstr(e->d_name, ".cpp") || strstr(e->d_name, ".hpp")) scan_file(path, sdk);
    }
    closedir(dir);
}
int export_native_discover(const char *source_root, const char *sdk_root) {
    memset(seen_section, 0, sizeof(seen_section));
    if (!source_root || !sdk_root) return 0;
    walk(source_root, sdk_root);
    return 1;
}

static int hook_seen[3];
static const char *const HOOK_NAMES[3] = { "rebax_game_start", "rebax_game_update", "rebax_game_stop" };
static void scan_hooks(const char *root) {
    DIR *dir = opendir(root); struct dirent *e;
    if (!dir) return;
    while ((e = readdir(dir)) != NULL) {
        char path[1536]; struct stat st;
        if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
        snprintf(path, sizeof(path), "%s/%s", root, e->d_name);
        if (stat(path, &st) != 0) continue;
        if (S_ISDIR(st.st_mode)) { scan_hooks(path); continue; }
        const char *dot = strrchr(e->d_name, '.');
        if (!dot || !(!strcmp(dot, ".c") || !strcmp(dot, ".cpp") || !strcmp(dot, ".cc") || !strcmp(dot, ".cxx"))) continue;
        FILE *f = fopen(path, "r"); char line[2048];
        if (!f) continue;
        while (fgets(line, sizeof(line), f)) {
            for (int i = 0; i < 3; i++) {
                const char *p = strstr(line, HOOK_NAMES[i]);
                size_t n = strlen(HOOK_NAMES[i]);
                if (p && (p[n] == '(' || p[n] == ' ') && (p == line || (p[-1] == ' ' || p[-1] == '\t'))) hook_seen[i] = 1;
            }
        }
        fclose(f);
    }
    closedir(dir);
}
int export_native_finish(const char *src_dir) {
    char project_dir[1536], out_path[1536];
    FILE *out;
    memset(hook_seen, 0, sizeof(hook_seen));
    if (!src_dir) return 0;
    snprintf(project_dir, sizeof(project_dir), "%s/project", src_dir);
    scan_hooks(project_dir);
    snprintf(out_path, sizeof(out_path), "%s/rebax_hooks.c", src_dir);
    out = fopen(out_path, "w");
    if (!out) return 0;
    fputs("#include \"rebax.h\"\n", out);
    if (!hook_seen[0]) fputs("void rebax_game_start(void) {}\n", out);
    if (!hook_seen[1]) fputs("void rebax_game_update(float delta_time) { (void)delta_time; }\n", out);
    if (!hook_seen[2]) fputs("void rebax_game_stop(void) {}\n", out);
    return fclose(out) == 0;
}
