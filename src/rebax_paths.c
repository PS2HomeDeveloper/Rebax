#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <SDL.h>

#include "rebax_paths.h"
#include "rebax_fs.h"
#include "embedded_resources.h"

#pragma GCC diagnostic ignored "-Wformat-truncation"
#define REBAX_PATH_MAX 1536

static char g_root_dir[REBAX_PATH_MAX];
static char g_toolchains_dir[REBAX_PATH_MAX];
static char g_toolchain_dir[REBAX_PATH_MAX];
static char g_make_path[REBAX_PATH_MAX];
static char g_node_resources_dir[REBAX_PATH_MAX];
static char g_temp_export_dir[REBAX_PATH_MAX];
static char g_settings_dir[REBAX_PATH_MAX];
static int g_paths_ready;

typedef enum { SETUP_NONE, SETUP_TOOLCHAINS, SETUP_PS2DEV, SETUP_NODES, SETUP_DONE, SETUP_FAILED } setup_state_t;
static volatile setup_state_t g_setup_state = SETUP_NONE;
static SDL_Thread *g_setup_thread;

static void mkdir_recursive(const char *path) {
    char buf[REBAX_PATH_MAX];
    strncpy(buf, path, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    for (char *p = buf + 1; *p; ++p) {
        if (*p == '/' || *p == '\\') {
            char sep = *p;
            *p = '\0';
            mkdir(buf, 0755);
            *p = sep;
        }
    }
    mkdir(buf, 0755);
}

static void compute_paths(void) {
    if (g_paths_ready) return;
#if defined(_WIN32)
    const char *base = getenv("LOCALAPPDATA");
    if (!base) base = ".";
    snprintf(g_root_dir, sizeof(g_root_dir), "%s\\Rebax", base);
    snprintf(g_toolchains_dir, sizeof(g_toolchains_dir), "%s\\Engine\\toolchains", g_root_dir);
    snprintf(g_toolchain_dir, sizeof(g_toolchain_dir), "%s\\ps2dev", g_toolchains_dir);
    snprintf(g_make_path, sizeof(g_make_path), "%s\\make.exe", g_toolchains_dir);
    snprintf(g_node_resources_dir, sizeof(g_node_resources_dir), "%s\\Engine\\resources\\nodes", g_root_dir);
    snprintf(g_temp_export_dir, sizeof(g_temp_export_dir), "%s\\Temp\\export", g_root_dir);
    snprintf(g_settings_dir, sizeof(g_settings_dir), "%s\\Settings", g_root_dir);
#elif defined(__ANDROID__)
    const char *base = SDL_AndroidGetInternalStoragePath();
    if (!base) { fprintf(stderr, "[rebax] Android storage path is unavailable.\n"); abort(); }
    snprintf(g_root_dir, sizeof(g_root_dir), "%s/Rebax", base);
    snprintf(g_toolchains_dir, sizeof(g_toolchains_dir), "%s/Engine/toolchains", g_root_dir);
    snprintf(g_toolchain_dir, sizeof(g_toolchain_dir), "%s/ps2dev", g_toolchains_dir);
    snprintf(g_make_path, sizeof(g_make_path), "%s/make", g_toolchains_dir);
    snprintf(g_node_resources_dir, sizeof(g_node_resources_dir), "%s/Engine/resources/nodes", g_root_dir);
    snprintf(g_temp_export_dir, sizeof(g_temp_export_dir), "%s/Temp/export", g_root_dir);
    snprintf(g_settings_dir, sizeof(g_settings_dir), "%s/Settings", g_root_dir);
#elif defined(__linux__)
    const char *home = getenv("HOME");
    if (!home) home = ".";
    snprintf(g_root_dir, sizeof(g_root_dir), "%s/.local/share/Rebax", home);
    snprintf(g_toolchains_dir, sizeof(g_toolchains_dir), "%s/Engine/toolchains", g_root_dir);
    snprintf(g_toolchain_dir, sizeof(g_toolchain_dir), "%s/ps2dev", g_toolchains_dir);
    snprintf(g_make_path, sizeof(g_make_path), "%s/make", g_toolchains_dir);
    snprintf(g_node_resources_dir, sizeof(g_node_resources_dir), "%s/Engine/resources/nodes", g_root_dir);
    snprintf(g_temp_export_dir, sizeof(g_temp_export_dir), "%s/Temp/export", g_root_dir);
    snprintf(g_settings_dir, sizeof(g_settings_dir), "%s/Settings", g_root_dir);
#else
#error "Unsupported operating system"
#endif
    char engine_dir[REBAX_PATH_MAX], resources_dir[REBAX_PATH_MAX];
    snprintf(engine_dir, sizeof(engine_dir), "%s/Engine", g_root_dir);
    snprintf(resources_dir, sizeof(resources_dir), "%s/Engine/resources", g_root_dir);
    mkdir_recursive(g_toolchains_dir);
    mkdir_recursive(resources_dir);
    mkdir_recursive(g_node_resources_dir);
    mkdir_recursive(g_temp_export_dir);
    mkdir_recursive(g_settings_dir);
    (void)engine_dir;
    g_paths_ready = 1;
}

const char *rebax_root_dir(void) { compute_paths(); return g_root_dir; }
const char *rebax_toolchains_dir(void) { compute_paths(); return g_toolchains_dir; }
const char *rebax_toolchain_dir(void) { compute_paths(); return g_toolchain_dir; }
const char *rebax_make_path(void) { compute_paths(); return g_make_path; }
const char *rebax_node_resources_dir(void) { compute_paths(); return g_node_resources_dir; }
const char *rebax_temp_export_dir(void) { compute_paths(); return g_temp_export_dir; }
const char *rebax_settings_dir(void) { compute_paths(); return g_settings_dir; }

static int has_setup_marker(void) {
    char marker[REBAX_PATH_MAX];
    struct stat st;
    snprintf(marker, sizeof(marker), "%s/.setup_ok", g_root_dir);
    return stat(marker, &st) == 0;
}

int rebax_paths_is_setup_needed(void) {
    compute_paths();
    return !has_setup_marker();
}

static int write_blob(const unsigned char *data, size_t size, const char *path) {
    if (!data || size == 0) return 0;
    FILE *f = fopen(path, "wb");
    if (!f) return 0;
    int ok = fwrite(data, 1, size, f) == size;
    if (fclose(f) != 0) ok = 0;
#ifndef _WIN32
    if (ok) chmod(path, 0755);
#endif
    return ok;
}

void rebax_paths_setup_start(void) {
    compute_paths();
    g_setup_thread = NULL;
    if (!rebax_paths_is_setup_needed()) { g_setup_state = SETUP_DONE; return; }
    printf("[rebax] first run - preparing PS2 toolchains and node resources...\n");
    g_setup_state = SETUP_TOOLCHAINS;
}

static void setup_step(void) {
    char archive[REBAX_PATH_MAX], resources_dir[REBAX_PATH_MAX];
    snprintf(resources_dir, sizeof(resources_dir), "%s/Engine/resources", g_root_dir);
    switch (g_setup_state) {
    case SETUP_NONE: case SETUP_DONE: case SETUP_FAILED: return;
    case SETUP_TOOLCHAINS:
        printf("[rebax] preparing bundled make and ps2dev archive...\n");
        snprintf(archive, sizeof(archive), "%s/ps2dev.tar.xz", g_toolchains_dir);
        if (!write_blob(_binary_embedded_toolchains_make_start,
                        embedded_make_size(), g_make_path)
            || !write_blob(_binary_embedded_toolchains_ps2dev_tar_xz_start,
                           embedded_ps2dev_archive_size(), archive)) {
            fprintf(stderr, "[rebax] FAILED: could not write bundled toolchain files.\n");
            remove(g_make_path);
            remove(archive);
            g_setup_state = SETUP_FAILED;
            return;
        }
#ifndef _WIN32
        if (chmod(g_make_path, 0755) != 0) {
            fprintf(stderr, "[rebax] FAILED: could not mark make executable.\n");
            g_setup_state = SETUP_FAILED;
            return;
        }
#endif
        g_setup_state = SETUP_PS2DEV;
        return;
    case SETUP_PS2DEV:
        printf("[rebax] extracting ps2dev...\n");
        snprintf(archive, sizeof(archive), "%s/ps2dev.tar.xz", g_toolchains_dir);
        if (!rebax_fs_exists(archive) || !rebax_fs_extract_tar_xz(archive, g_toolchains_dir)) {
            fprintf(stderr, "[rebax] FAILED: could not extract ps2dev.tar.xz.\n");
            remove(archive); g_setup_state = SETUP_FAILED; return;
        }
        remove(archive);
        g_setup_state = SETUP_NODES;
        return;
    case SETUP_NODES:
        printf("[rebax] extracting node resources...\n");
        snprintf(archive, sizeof(archive), "%s/.nodes.tar.xz", g_root_dir);
        if (!write_blob(_binary_embedded_nodes_tar_xz_start, embedded_node_archive_size(), archive)
            || !rebax_fs_extract_tar_xz(archive, resources_dir)) {
            fprintf(stderr, "[rebax] FAILED: could not extract nodes.tar.xz.\n");
            remove(archive); g_setup_state = SETUP_FAILED; return;
        }
        remove(archive);
        snprintf(archive, sizeof(archive), "%s/.setup_ok", g_root_dir);
        { FILE *f = fopen(archive, "wb"); if (!f) { g_setup_state = SETUP_FAILED; return; } fclose(f); }
        g_setup_state = SETUP_DONE;
        return;
    }
}

static int setup_worker(void *unused) {
    (void)unused;
    while (g_setup_state != SETUP_DONE && g_setup_state != SETUP_FAILED) {
        setup_step();
        SDL_Delay(1);
    }
    return 0;
}

void rebax_paths_setup_update(void) {
    if (g_setup_thread == NULL && g_setup_state != SETUP_DONE && g_setup_state != SETUP_FAILED) {
        g_setup_thread = SDL_CreateThread(setup_worker, "rebax_setup", NULL);
        if (!g_setup_thread) { fprintf(stderr, "[rebax] could not start setup worker: %s\n", SDL_GetError()); g_setup_state = SETUP_FAILED; }
    }
    if (g_setup_thread && (g_setup_state == SETUP_DONE || g_setup_state == SETUP_FAILED)) {
        SDL_WaitThread(g_setup_thread, NULL);
        g_setup_thread = NULL;
    }
}

int rebax_paths_setup_done(void) { return g_setup_state == SETUP_DONE || g_setup_state == SETUP_FAILED || g_setup_state == SETUP_NONE; }
int rebax_paths_setup_failed(void) { return g_setup_state == SETUP_FAILED; }
