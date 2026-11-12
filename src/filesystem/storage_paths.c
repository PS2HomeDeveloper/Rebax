#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL.h>

#include "filesystem/storage_paths.h"
#include "filesystem/rebax_fs.h"
#include "filesystem/rebax_fs_tree.h"
#include "filesystem/rebax_archive.h"
#include "filesystem/assets.h"

#define REBAX_PATH_MAX 1536

static char g_root_dir[REBAX_PATH_MAX];
static char g_toolchains_dir[REBAX_PATH_MAX];
static char g_sdks_dir[REBAX_PATH_MAX];
static char g_tools_dir[REBAX_PATH_MAX];
static char g_node_resources_dir[REBAX_PATH_MAX];
static char g_temp_export_dir[REBAX_PATH_MAX];
static char g_editor_dir[REBAX_PATH_MAX];
static char g_shipped_dir[REBAX_PATH_MAX];
static int g_paths_ready;

typedef enum { SETUP_NONE, SETUP_MOVE, SETUP_DONE, SETUP_FAILED } setup_state_t;
static volatile setup_state_t g_setup_state = SETUP_NONE;

static void strip_trailing_sep(char *path) {
    size_t n = strlen(path);
    while (n > 1 && (path[n - 1] == '/' || path[n - 1] == '\\')) path[--n] = '\0';
}

static void compute_paths(void) {
    if (g_paths_ready) return;
#if defined(_WIN32)
    const char *local = getenv("LOCALAPPDATA");
    snprintf(g_root_dir, sizeof(g_root_dir), "%s/Rebax/Engine", local ? local : ".");
#elif defined(__ANDROID__)
    const char *internal = SDL_AndroidGetInternalStoragePath();
    if (!internal) { fprintf(stderr, "[rebax] storage path is unavailable.\n"); abort(); }
    snprintf(g_root_dir, sizeof(g_root_dir), "%s/Rebax/Engine", internal);
#else
    char *pref = SDL_GetPrefPath("Rebax", "Engine");
    if (!pref) { fprintf(stderr, "[rebax] storage path is unavailable.\n"); abort(); }
    snprintf(g_root_dir, sizeof(g_root_dir), "%s", pref);
    SDL_free(pref);
#endif
    strip_trailing_sep(g_root_dir);
    snprintf(g_toolchains_dir, sizeof(g_toolchains_dir), "%s/toolchains", g_root_dir);
    snprintf(g_sdks_dir, sizeof(g_sdks_dir), "%s/sdks", g_root_dir);
    snprintf(g_tools_dir, sizeof(g_tools_dir), "%s/tools", g_root_dir);
    snprintf(g_node_resources_dir, sizeof(g_node_resources_dir), "%s/sdks/rebax", g_root_dir);
    snprintf(g_temp_export_dir, sizeof(g_temp_export_dir), "%s/Temp/export", g_root_dir);
    snprintf(g_editor_dir, sizeof(g_editor_dir), "%s/Editor", g_root_dir);

    g_shipped_dir[0] = '\0';
#if !defined(__ANDROID__)
    char *base = SDL_GetBasePath();
    if (base) {
        snprintf(g_shipped_dir, sizeof(g_shipped_dir), "%s", base);
        SDL_free(base);
        strip_trailing_sep(g_shipped_dir);
    }
#endif

    rebax_fs_mkdir_p(g_toolchains_dir);
    rebax_fs_mkdir_p(g_sdks_dir);
    rebax_fs_mkdir_p(g_tools_dir);
    rebax_fs_mkdir_p(g_temp_export_dir);
    rebax_fs_mkdir_p(g_editor_dir);
    g_paths_ready = 1;
}

const char *rebax_root_dir(void) { compute_paths(); return g_root_dir; }
const char *rebax_toolchains_dir(void) { compute_paths(); return g_toolchains_dir; }
const char *rebax_toolchain_dir(void) { compute_paths(); return g_toolchains_dir; }
const char *rebax_sdks_dir(void) { compute_paths(); return g_sdks_dir; }
const char *rebax_tools_dir(void) { compute_paths(); return g_tools_dir; }
const char *rebax_node_resources_dir(void) { compute_paths(); return g_node_resources_dir; }
const char *rebax_temp_export_dir(void) { compute_paths(); return g_temp_export_dir; }
const char *rebax_editor_dir(void) { compute_paths(); return g_editor_dir; }

#if !defined(__ANDROID__)
static int shipped_path(const char *name, char *out, size_t cap) {
    if (g_shipped_dir[0] == '\0') return 0;
    snprintf(out, cap, "%s/%s", g_shipped_dir, name);
    return rebax_fs_is_dir(out);
}
#endif

#if defined(__ANDROID__)

#define ANDROID_STAMP_ASSET "packages/stamp.txt"

static void android_stamp_path(char *out, size_t cap) {
    snprintf(out, cap, "%s/.setup-stamp", g_root_dir);
}

static int android_setup_needed(void) {
    size_t shipped_size = 0;
    unsigned char *shipped = asset_file_read(ANDROID_STAMP_ASSET, &shipped_size);
    if (!shipped) return 0;
    char path[REBAX_PATH_MAX];
    android_stamp_path(path, sizeof(path));
    FILE *f = fopen(path, "rb");
    int same = 0;
    if (f) {
        char current[128];
        size_t n = fread(current, 1, sizeof(current) - 1, f);
        current[n] = '\0';
        fclose(f);
        same = strcmp(current, (const char *)shipped) == 0;
    }
    free(shipped);
    return !same;
}

static int android_install_package(const char *asset, const char *dest_dir) {
    char archive[REBAX_PATH_MAX], error[256];
    snprintf(archive, sizeof(archive), "%s/package.tar.xz", g_temp_export_dir);
    if (!asset_file_copy(asset, archive)) {
        fprintf(stderr, "[rebax] FAILED: cannot read the packaged asset '%s'.\n", asset);
        return 0;
    }
    int ok = rebax_archive_install(archive, dest_dir, NULL, NULL, error, sizeof(error));
    remove(archive);
    if (!ok) fprintf(stderr, "[rebax] FAILED: cannot install '%s': %s\n", asset, error);
    return ok;
}

static void android_copy_node_file(const char *relative, void *user) {
    int *ok = (int *)user;
    char asset[REBAX_PATH_MAX], dest[REBAX_PATH_MAX], parent[REBAX_PATH_MAX];
    snprintf(asset, sizeof(asset), "sdks/rebax/%s", relative);
    snprintf(dest, sizeof(dest), "%s/%s", g_node_resources_dir, relative);
    snprintf(parent, sizeof(parent), "%s", dest);
    char *slash = strrchr(parent, '/');
    if (slash) {
        *slash = '\0';
        rebax_fs_mkdir_p(parent);
    }
    if (!asset_file_copy(asset, dest)) *ok = 0;
}

typedef struct {
    char sdk_name[96];
} android_listing_t;

static void android_find_sdk(const char *name, void *user) {
    android_listing_t *l = (android_listing_t *)user;
    size_t n = strlen(name);
    if (strncmp(name, "sdk-", 4) == 0 && n > 11 && strcmp(name + n - 7, ".tar.xz") == 0 && n < sizeof(l->sdk_name)) {
        snprintf(l->sdk_name, sizeof(l->sdk_name), "%s", name);
    }
}

static void android_copy_tool_file(const char *relative, void *user) {
    int *ok = (int *)user;
    char asset[REBAX_PATH_MAX], dest[REBAX_PATH_MAX], parent[REBAX_PATH_MAX];
    snprintf(asset, sizeof(asset), "tools/%s", relative);
    snprintf(dest, sizeof(dest), "%s/%s", g_tools_dir, relative);
    snprintf(parent, sizeof(parent), "%s", dest);
    char *slash = strrchr(parent, '/');
    if (slash) {
        *slash = '\0';
        rebax_fs_mkdir_p(parent);
    }
    if (!asset_file_copy(asset, dest)) *ok = 0;
}

static int android_setup(void) {
    char dest[REBAX_PATH_MAX], asset[REBAX_PATH_MAX];
    android_listing_t listing;
    memset(&listing, 0, sizeof(listing));
    if (!rebax_fs_mkdir_p(g_temp_export_dir)) return 0;

    snprintf(dest, sizeof(dest), "%s/clang", g_toolchains_dir);
    if (!android_install_package("packages/clang.tar.xz", dest)) return 0;

    asset_file_list("packages/", android_find_sdk, &listing);
    if (listing.sdk_name[0]) {
        char id[96];
        snprintf(id, sizeof(id), "%s", listing.sdk_name + 4);
        id[strlen(id) - 7] = '\0';
        snprintf(asset, sizeof(asset), "packages/%s", listing.sdk_name);
        snprintf(dest, sizeof(dest), "%s/%s", g_sdks_dir, id);
        if (!android_install_package(asset, dest)) return 0;
    }

    int ok = 1;
    asset_file_list_tree("sdks/rebax/", android_copy_node_file, &ok);
    if (!ok) {
        fprintf(stderr, "[rebax] FAILED: cannot copy the node sources.\n");
        return 0;
    }

    asset_file_list_tree("tools/", android_copy_tool_file, &ok);
    if (!ok) {
        fprintf(stderr, "[rebax] FAILED: cannot copy the tools.\n");
        return 0;
    }

    size_t stamp_size = 0;
    unsigned char *stamp = asset_file_read(ANDROID_STAMP_ASSET, &stamp_size);
    if (stamp) {
        char path[REBAX_PATH_MAX];
        android_stamp_path(path, sizeof(path));
        FILE *f = fopen(path, "wb");
        if (f) {
            fwrite(stamp, 1, stamp_size, f);
            fclose(f);
        }
        free(stamp);
    }
    return 1;
}

#endif

int rebax_paths_is_setup_needed(void) {
    char path[REBAX_PATH_MAX];
    compute_paths();
#if defined(__ANDROID__)
    (void)path;
    return android_setup_needed();
#else
    return shipped_path("sdks", path, sizeof(path)) || shipped_path("toolchains", path, sizeof(path)) || shipped_path("tools", path, sizeof(path));
#endif
}

void rebax_paths_setup_start(void) {
    compute_paths();
    if (!rebax_paths_is_setup_needed()) { g_setup_state = SETUP_DONE; return; }
    printf("[rebax] first run - moving the shipped sdks, toolchains and tools to the Rebax folder...\n");
    g_setup_state = SETUP_MOVE;
}

#if !defined(__ANDROID__)
static void move_shipped(const char *name, const char *dst) {
    char src[REBAX_PATH_MAX];
    if (!shipped_path(name, src, sizeof(src))) return;
    if (!rebax_fs_merge_move(src, dst)) {
        fprintf(stderr, "[rebax] FAILED: could not move '%s' to '%s'.\n", src, dst);
        g_setup_state = SETUP_FAILED;
    }
}
#endif

void rebax_paths_setup_update(void) {
    if (g_setup_state != SETUP_MOVE) return;
#if defined(__ANDROID__)
    g_setup_state = android_setup() ? SETUP_DONE : SETUP_FAILED;
#else
    move_shipped("sdks", g_sdks_dir);
    if (g_setup_state == SETUP_FAILED) return;
    move_shipped("toolchains", g_toolchains_dir);
    if (g_setup_state == SETUP_FAILED) return;
    move_shipped("tools", g_tools_dir);
    if (g_setup_state == SETUP_FAILED) return;
    g_setup_state = SETUP_DONE;
#endif
}

int rebax_paths_setup_done(void) { return g_setup_state == SETUP_DONE || g_setup_state == SETUP_FAILED || g_setup_state == SETUP_NONE; }
int rebax_paths_setup_failed(void) { return g_setup_state == SETUP_FAILED; }
