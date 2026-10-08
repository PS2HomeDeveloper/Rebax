#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#if defined(__APPLE__)
#include <TargetConditionals.h>
#if TARGET_OS_IPHONE
#define REBAX_IOS 1
#endif
#endif


#if defined(_WIN32)
#include <direct.h>
#define REBAX_MKDIR(path, mode) _mkdir(path)
#else
#define REBAX_MKDIR(path, mode) mkdir(path, mode)
#endif

#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <SDL.h>

#include "rebax_paths.h"
#include "rebax_fs.h"
#if defined(__ANDROID__)
#include "asset_files.h"
#else
#include "embedded_resources.h"
#endif

#pragma GCC diagnostic ignored "-Wformat-truncation"
#define REBAX_PATH_MAX 1536

static char g_root_dir[REBAX_PATH_MAX];
static char g_toolchains_dir[REBAX_PATH_MAX];
static char g_toolchain_dir[REBAX_PATH_MAX];
static char g_node_resources_dir[REBAX_PATH_MAX];
static char g_temp_export_dir[REBAX_PATH_MAX];
static char g_Editor_dir[REBAX_PATH_MAX];
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
            REBAX_MKDIR(buf, 0755);
            *p = sep;
        }
    }
    REBAX_MKDIR(buf, 0755);
}

static void compute_paths(void) {
    if (g_paths_ready) return;
#if defined(_WIN32)
    const char *base = getenv("LOCALAPPDATA");
    if (!base) base = ".";
    snprintf(g_root_dir, sizeof(g_root_dir), "%s\\Rebax", base);
    snprintf(g_toolchains_dir, sizeof(g_toolchains_dir), "%s\\Engine\\ps2\\toolchains", g_root_dir);
    snprintf(g_toolchain_dir, sizeof(g_toolchain_dir), "%s", g_toolchains_dir);
    snprintf(g_node_resources_dir, sizeof(g_node_resources_dir), "%s\\Engine\\ps2\\sdk\\nodes", g_root_dir);
    snprintf(g_temp_export_dir, sizeof(g_temp_export_dir), "%s\\Temp\\export", g_root_dir);
    snprintf(g_Editor_dir, sizeof(g_Editor_dir), "%s\\Editor", g_root_dir);
#elif defined(__ANDROID__)
    const char *base = SDL_AndroidGetInternalStoragePath();
    if (!base) { fprintf(stderr, "[rebax] Android storage path is unavailable.\n"); abort(); }
    snprintf(g_root_dir, sizeof(g_root_dir), "%s/Rebax", base);
    snprintf(g_toolchains_dir, sizeof(g_toolchains_dir), "%s/Engine/ps2/toolchains", g_root_dir);
    snprintf(g_toolchain_dir, sizeof(g_toolchain_dir), "%s", g_toolchains_dir);
    snprintf(g_node_resources_dir, sizeof(g_node_resources_dir), "ps2/sdk/nodes");
    snprintf(g_temp_export_dir, sizeof(g_temp_export_dir), "%s/Temp/export", g_root_dir);
    snprintf(g_Editor_dir, sizeof(g_Editor_dir), "%s/Editor", g_root_dir);
#elif defined(__linux__)
    const char *home = getenv("HOME");
    if (!home) home = ".";
    snprintf(g_root_dir, sizeof(g_root_dir), "%s/.local/share/Rebax", home);
    snprintf(g_toolchains_dir, sizeof(g_toolchains_dir), "%s/Engine/ps2/toolchains", g_root_dir);
    snprintf(g_toolchain_dir, sizeof(g_toolchain_dir), "%s", g_toolchains_dir);
    snprintf(g_node_resources_dir, sizeof(g_node_resources_dir), "%s/Engine/ps2/sdk/nodes", g_root_dir);
    snprintf(g_temp_export_dir, sizeof(g_temp_export_dir), "%s/Temp/export", g_root_dir);
    snprintf(g_Editor_dir, sizeof(g_Editor_dir), "%s/Editor", g_root_dir);
    #elif defined(__APPLE__)
#if defined(REBAX_IOS)
    const char *home = getenv("HOME");
    if (!home) { fprintf(stderr, "[rebax] iOS storage path is unavailable.\n"); abort(); }
    snprintf(g_root_dir, sizeof(g_root_dir), "%s/Documents/Rebax", home);
#else
    const char *home = getenv("HOME");
    if (!home) home = ".";
    snprintf(g_root_dir, sizeof(g_root_dir), "%s/Library/Application Support/Rebax", home);
#endif
    snprintf(g_toolchains_dir, sizeof(g_toolchains_dir), "%s/Engine/ps2/toolchains", g_root_dir);
    snprintf(g_toolchain_dir, sizeof(g_toolchain_dir), "%s", g_toolchains_dir);
    snprintf(g_node_resources_dir, sizeof(g_node_resources_dir), "%s/Engine/ps2/sdk/nodes", g_root_dir);
    snprintf(g_temp_export_dir, sizeof(g_temp_export_dir), "%s/Temp/export", g_root_dir);
    snprintf(g_Editor_dir, sizeof(g_Editor_dir), "%s/Editor", g_root_dir);
#else
#error "Unsupported operating system"
#endif
    char engine_toolchains_dir[REBAX_PATH_MAX], node_sdk_dir[REBAX_PATH_MAX];
    snprintf(engine_toolchains_dir, sizeof(engine_toolchains_dir), "%s/Engine/toolchains", g_root_dir);
    snprintf(node_sdk_dir, sizeof(node_sdk_dir), "%s/Engine/ps2/sdk", g_root_dir);
#if !defined(REBAX_IOS)
    mkdir_recursive(g_toolchains_dir);
    mkdir_recursive(g_toolchain_dir);
    mkdir_recursive(engine_toolchains_dir);
#endif
#if !defined(__ANDROID__) && !defined(REBAX_IOS)
    mkdir_recursive(node_sdk_dir);
    mkdir_recursive(g_node_resources_dir);
#endif
    mkdir_recursive(g_temp_export_dir);
    mkdir_recursive(g_Editor_dir);
    g_paths_ready = 1;
}

const char *rebax_root_dir(void) { compute_paths(); return g_root_dir; }
const char *rebax_toolchains_dir(void) { compute_paths(); return g_toolchains_dir; }
const char *rebax_toolchain_dir(void) { compute_paths(); return g_toolchain_dir; }
const char *rebax_node_resources_dir(void) { compute_paths(); return g_node_resources_dir; }
const char *rebax_temp_export_dir(void) { compute_paths(); return g_temp_export_dir; }
const char *rebax_Editor_dir(void) { compute_paths(); return g_Editor_dir; }
const char *rebax_editor_dir(void) { compute_paths(); return g_Editor_dir; }

static int has_setup_marker(void) {
    char marker[REBAX_PATH_MAX];
    struct stat st;
    snprintf(marker, sizeof(marker), "%s/.setup_ok", g_root_dir);
    if (stat(marker, &st) != 0) return 0;
#if !defined(_WIN32)
    {
        FILE *mf = fopen(marker, "rb");
        int version = mf ? fgetc(mf) : EOF;
        if (mf) fclose(mf);
        if (version != '2') return 0;
    }
#endif

    char ps2dev_dir[REBAX_PATH_MAX];
    snprintf(ps2dev_dir, sizeof(ps2dev_dir), "%s/ps2dev", g_toolchain_dir);
#if defined(__ANDROID__)
    return rebax_fs_exists(ps2dev_dir);
#else
    return rebax_fs_exists(ps2dev_dir)
        && rebax_fs_exists(g_node_resources_dir);
#endif
}

int rebax_paths_is_setup_needed(void) {
    compute_paths();
    return !has_setup_marker();
}

#if !defined(__ANDROID__) && !defined(REBAX_IOS)
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
#endif

void rebax_paths_setup_start(void) {
    compute_paths();
    g_setup_thread = NULL;
#if defined(REBAX_IOS)
    g_setup_state = SETUP_DONE;
    return;
#endif
    if (!rebax_paths_is_setup_needed()) { g_setup_state = SETUP_DONE; return; }
    printf("[rebax] first run - preparing PS2 toolchains and node resources...\n");
    g_setup_state = SETUP_TOOLCHAINS;
}

#if defined(__ANDROID__)
static void copy_engine_tool_cb(const char *name, void *user) {
    int *ok = (int *)user;
    char asset[REBAX_PATH_MAX], dest[REBAX_PATH_MAX];
    snprintf(asset, sizeof(asset), "toolchains/%s", name);
    snprintf(dest, sizeof(dest), "%s/Engine/toolchains/%s", g_root_dir, name);
    if (!asset_file_copy(asset, dest) || chmod(dest, 0755) != 0) *ok = 0;
}
#endif

#if defined(REBAX_IOS)
static void setup_step(void) {
    g_setup_state = SETUP_DONE;
}
#else
static void setup_step(void) {
    char archive[REBAX_PATH_MAX], node_sdk_dir[REBAX_PATH_MAX];
    snprintf(node_sdk_dir, sizeof(node_sdk_dir), "%s/Engine/ps2/sdk", g_root_dir);
    switch (g_setup_state) {
    case SETUP_NONE: case SETUP_DONE: case SETUP_FAILED: return;
    case SETUP_TOOLCHAINS:
        printf("[rebax] preparing ps2dev archive...\n");
        snprintf(archive, sizeof(archive), "%s/ps2dev.tar.xz", g_toolchain_dir);
#if defined(__ANDROID__)
        int engine_tools_ok = 1;
        asset_file_list("toolchains/", copy_engine_tool_cb, &engine_tools_ok);
        if (!engine_tools_ok
            || !asset_file_copy("ps2/toolchains/ps2dev.tar.xz", archive)) {
#else
        if (!write_blob(_binary_embedded_ps2_toolchains_ps2dev_tar_xz_start,
                        embedded_ps2dev_archive_size(), archive)) {
#endif
            fprintf(stderr, "[rebax] FAILED: could not write bundled toolchain files.\n");
            remove(archive);
            g_setup_state = SETUP_FAILED;
            return;
        }
        g_setup_state = SETUP_PS2DEV;
        return;
    case SETUP_PS2DEV:
        printf("[rebax] extracting ps2dev...\n");
        snprintf(archive, sizeof(archive), "%s/ps2dev.tar.xz", g_toolchain_dir);
        if (!rebax_fs_exists(archive) || !rebax_fs_extract_tar_xz(archive, g_toolchain_dir)) {
            fprintf(stderr, "[rebax] FAILED: could not extract ps2dev.tar.xz.\n");
            remove(archive); g_setup_state = SETUP_FAILED; return;
        }
        remove(archive);
        g_setup_state = SETUP_NODES;
        return;
    case SETUP_NODES:
#if !defined(__ANDROID__)
        printf("[rebax] extracting node resources...\n");
        snprintf(archive, sizeof(archive), "%s/.nodes.tar.xz", g_root_dir);
        if (!write_blob(_binary_embedded_ps2_sdk_nodes_tar_xz_start, embedded_node_archive_size(), archive)
            || !rebax_fs_extract_tar_xz(archive, node_sdk_dir)) {
            fprintf(stderr, "[rebax] FAILED: could not extract nodes.tar.xz.\n");
            remove(archive); g_setup_state = SETUP_FAILED; return;
        }
        remove(archive);
#endif
        snprintf(archive, sizeof(archive), "%s/.setup_ok", g_root_dir);
        { FILE *f = fopen(archive, "wb"); if (!f) { g_setup_state = SETUP_FAILED; return; } fputs("2", f); fclose(f); }
        g_setup_state = SETUP_DONE;
        return;
    }
}
#endif

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
