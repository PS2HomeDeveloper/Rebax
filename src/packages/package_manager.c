#define _POSIX_C_SOURCE 200809L

#include "package_manager.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <dirent.h>

#if defined(__APPLE__)
#include <TargetConditionals.h>
#endif

#include <SDL.h>

#include "https_client.h"
#include "package_fetch.h"
#include "rebax_fs.h"
#include "rebax_paths.h"
#include "resource_files.h"

#define TOOLCHAINS_URL "https://github.com/PS2HomeDeveloper/Rebax-Toolchains/releases/latest/download"
#define SDKS_URL "https://github.com/PS2HomeDeveloper/Rebax-SDKs/releases/latest/download"
#define PATH_CAP 1536
#define MAX_JOBS 40

typedef enum { KIND_CLANG, KIND_GCC_PS1, KIND_GCC_PS2, KIND_SDK } kind_t;

typedef struct {
    const char *name;
    int arch_count;
    const char *arch[PKG_MAX_ARCHS];
    const char *sdk[PKG_MAX_ARCHS];
    int modern;
} platform_def_t;

static const platform_def_t PLATFORMS[PKG_PLATFORM_COUNT] = {
    { "Android", 4, { "arm64-v8a", "armeabi-v7a", "x86", "x86_64" },
      { "android-arm64-v8a", "android-armeabi-v7a", "android-x86", "android-x86_64" }, 1 },
    { "Linux", 4, { "arm64", "arm", "x86", "x86_64" },
      { "linux-arm64", "linux-arm", "linux-x86", "linux-x86_64" }, 1 },
    { "Windows", 4, { "arm64", "arm", "x86", "x86_64" },
      { "windows-arm64", "windows-arm", "windows-x86", "windows-x86_64" }, 1 },
    { "macOS", 2, { "arm64", "x86_64" }, { "macos-arm64", "macos-x86_64" }, 1 },
    { "iOS", 2, { "arm64 (device)", "x86_64 (simulator)" }, { "ios-arm64", "ios-simulator-x86_64" }, 1 },
    { "PlayStation 2", 0, { NULL }, { "ps2" }, 0 },
    { "PlayStation 1", 0, { NULL }, { "ps1" }, 0 }
};

typedef struct {
    int install;
    kind_t kind;
    char file[160];
    char label[160];
    char dest_parent[PATH_CAP];
    char dest_name[64];
} job_t;

static SDL_mutex *g_lock;
static SDL_Thread *g_thread;
static int g_ready;
static volatile int g_busy;
static volatile int g_cancel;
static int g_percent;
static int g_extracting;
static char g_title[160];
static pkg_msg_kind_t g_msg_kind;
static char g_msg[400];
static unsigned long long g_status_rev;
static unsigned long long g_states_rev;
static job_t g_jobs[MAX_JOBS];
static int g_job_count;

static char g_host_platform[16];
static char g_host_arch[16];
static char g_host_text[64];
static int g_host_supported;

static pkg_state_t g_arch_state[PKG_PLATFORM_COUNT][PKG_MAX_ARCHS];
static pkg_state_t g_host_clang_state;

static void detect_host(void) {
#if defined(__ANDROID__)
    snprintf(g_host_platform, sizeof(g_host_platform), "android");
#elif defined(_WIN32)
    snprintf(g_host_platform, sizeof(g_host_platform), "windows");
#elif defined(__APPLE__)
#if TARGET_OS_IPHONE
    snprintf(g_host_platform, sizeof(g_host_platform), "ios");
#else
    snprintf(g_host_platform, sizeof(g_host_platform), "macos");
#endif
#else
    snprintf(g_host_platform, sizeof(g_host_platform), "linux");
#endif
#if defined(__x86_64__) || defined(_M_X64)
    snprintf(g_host_arch, sizeof(g_host_arch), "x86_64");
#elif defined(__aarch64__) || defined(_M_ARM64)
    snprintf(g_host_arch, sizeof(g_host_arch), "%s", strcmp(g_host_platform, "android") == 0 ? "arm64-v8a" : "arm64");
#elif defined(__i386__) || defined(_M_IX86)
    snprintf(g_host_arch, sizeof(g_host_arch), "x86");
#elif defined(__arm__) || defined(_M_ARM)
    snprintf(g_host_arch, sizeof(g_host_arch), "%s", strcmp(g_host_platform, "android") == 0 ? "armeabi-v7a" : "arm");
#else
    snprintf(g_host_arch, sizeof(g_host_arch), "x86_64");
#endif
    snprintf(g_host_text, sizeof(g_host_text), "%s-%s", g_host_platform, g_host_arch);
    g_host_supported = strcmp(g_host_platform, "ios") != 0;
}

static int dir_has_children(const char *dir, const char *const *names) {
    char path[PATH_CAP];
    if (!rebax_fs_is_dir(dir)) return 0;
    for (int i = 0; names[i]; i++) {
        snprintf(path, sizeof(path), "%s/%s", dir, names[i]);
        if (!rebax_fs_is_dir(path)) return 0;
    }
    return 1;
}

static int dir_not_empty(const char *dir) {
    if (!rebax_fs_is_dir(dir)) return 0;
    DIR *d = opendir(dir);
    if (!d) return 0;
    int found = 0;
    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        if (strcmp(e->d_name, ".") != 0 && strcmp(e->d_name, "..") != 0) { found = 1; break; }
    }
    closedir(d);
    return found;
}

static int component_ok(kind_t kind, const char *dir) {
    static const char *const clang[] = { "bin", NULL };
    if (kind == KIND_CLANG) return dir_has_children(dir, clang);
    return dir_not_empty(dir);
}

static int sdk_ok(int platform, int arch) {
    static const char *const modern[] = { "standard", "runtime", "graphics", NULL };
    static const char *const console[] = { "standard", "graphics", NULL };
    char dir[PATH_CAP];
    const platform_def_t *p = &PLATFORMS[platform];
    snprintf(dir, sizeof(dir), "%s/%s", rebax_sdks_dir(), p->sdk[arch]);
    return dir_has_children(dir, p->modern ? modern : console);
}

static void gcc_dir_for(int platform, char *out, size_t cap) {
    snprintf(out, cap, "%s/%s", rebax_toolchains_dir(), platform == 5 ? "gcc-ps2" : "gcc-ps1");
}

static int host_clang_ok(void) {
    char dir[PATH_CAP];
    snprintf(dir, sizeof(dir), "%s/clang", rebax_toolchains_dir());
    return component_ok(KIND_CLANG, dir);
}

static int gcc_ok(int platform) {
    char dir[PATH_CAP];
    gcc_dir_for(platform, dir, sizeof(dir));
    return component_ok(platform == 5 ? KIND_GCC_PS2 : KIND_GCC_PS1, dir);
}

void package_refresh_states(void) {
    int clang = host_clang_ok();
    g_host_clang_state = !g_host_supported ? PKG_STATE_UNAVAILABLE : (clang ? PKG_STATE_INSTALLED : PKG_STATE_NOT_INSTALLED);
    for (int p = 0; p < PKG_PLATFORM_COUNT; p++) {
        int count = PLATFORMS[p].arch_count ? PLATFORMS[p].arch_count : 1;
        for (int a = 0; a < count; a++) {
            pkg_state_t s;
            if (!g_host_supported) s = PKG_STATE_UNAVAILABLE;
            else {
                int sdk = sdk_ok(p, a);
                int compiler = p >= 5 ? gcc_ok(p) : clang;
                s = (sdk && compiler) ? PKG_STATE_INSTALLED : (sdk || compiler ? PKG_STATE_PARTIAL : PKG_STATE_NOT_INSTALLED);
            }
            g_arch_state[p][a] = s;
        }
    }
    g_states_rev++;
}

static void set_status(const char *title, int percent, int extracting) {
    SDL_LockMutex(g_lock);
    if (title) snprintf(g_title, sizeof(g_title), "%s", title);
    g_percent = percent;
    g_extracting = extracting;
    g_status_rev++;
    SDL_UnlockMutex(g_lock);
}

static void set_message(pkg_msg_kind_t kind, const char *text) {
    SDL_LockMutex(g_lock);
    g_msg_kind = kind;
    snprintf(g_msg, sizeof(g_msg), "%s", text ? text : "");
    g_status_rev++;
    SDL_UnlockMutex(g_lock);
}

static void on_download(unsigned long long done, unsigned long long total, void *user) {
    (void)user;
    int pct = total ? (int)(done * 100 / total) : 0;
    SDL_LockMutex(g_lock);
    if (pct != g_percent || g_extracting) { g_percent = pct; g_extracting = 0; g_status_rev++; }
    SDL_UnlockMutex(g_lock);
}

static void on_extract(unsigned long long done, unsigned long long total, void *user) {
    (void)user;
    int pct = total ? (int)(done * 100 / total) : 0;
    SDL_LockMutex(g_lock);
    if (pct != g_percent || !g_extracting) { g_percent = pct; g_extracting = 1; g_status_rev++; }
    SDL_UnlockMutex(g_lock);
}

static void on_warning(const char *text, void *user) {
    (void)user;
    set_message(PKG_MSG_WARNING, text);
}

static int normalise_into(const char *tmp, const char *dest) {
    char only[PATH_CAP];
    int entries = 0;
    only[0] = '\0';
    DIR *d = opendir(tmp);
    if (!d) return 0;
    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0) continue;
        entries++;
        snprintf(only, sizeof(only), "%s/%s", tmp, e->d_name);
    }
    closedir(d);
    if (entries == 0) return 0;
    if (entries == 1 && rebax_fs_is_dir(only)) {
        if (!rebax_fs_move(only, dest)) return 0;
        rebax_fs_remove_recursive(tmp);
        return 1;
    }
    return rebax_fs_move(tmp, dest);
}

static int run_install(const job_t *j, char *err, size_t cap) {
    char dest[PATH_CAP], tmp[PATH_CAP], work[PATH_CAP];
    snprintf(dest, sizeof(dest), "%s/%s", j->dest_parent, j->dest_name);
    snprintf(tmp, sizeof(tmp), "%s/.rebax-tmp-%s", j->dest_parent, j->dest_name);
    snprintf(work, sizeof(work), "%s/download", rebax_temp_export_dir());
    rebax_fs_remove_recursive(dest);
    rebax_fs_remove_recursive(tmp);
    if (!rebax_fs_mkdir_p(tmp)) { snprintf(err, cap, "cannot create %s", tmp); return 0; }
    rebax_fetch_hooks_t hooks;
    memset(&hooks, 0, sizeof(hooks));
    hooks.on_download = on_download;
    hooks.on_extract = on_extract;
    hooks.on_warning = on_warning;
    hooks.cancel = &g_cancel;
    const char *base = j->kind == KIND_SDK ? SDKS_URL : TOOLCHAINS_URL;
    if (!rebax_package_fetch(base, j->file, work, tmp, &hooks, err, cap)) {
        rebax_fs_remove_recursive(tmp);
        return 0;
    }
    if (!normalise_into(tmp, dest)) {
        rebax_fs_remove_recursive(tmp);
        rebax_fs_remove_recursive(dest);
        snprintf(err, cap, "the package %s is empty or damaged", j->file);
        return 0;
    }
    if (!component_ok(j->kind, dest)) {
        rebax_fs_remove_recursive(dest);
        snprintf(err, cap, "the package %s does not have the expected folders", j->file);
        return 0;
    }
    return 1;
}

static int worker(void *unused) {
    (void)unused;
    int failed = 0;
    char err[400];
    for (int i = 0; i < g_job_count && !failed && !g_cancel; i++) {
        job_t *j = &g_jobs[i];
        char title[200];
        if (j->install) {
            snprintf(title, sizeof(title), "Installing %s (%d of %d)", j->label, i + 1, g_job_count);
            set_status(title, 0, 0);
            if (!run_install(j, err, sizeof(err))) {
                set_message(PKG_MSG_ERROR, err);
                failed = 1;
            }
        } else {
            char dest[PATH_CAP];
            snprintf(title, sizeof(title), "Removing %s", j->label);
            set_status(title, 100, 0);
            snprintf(dest, sizeof(dest), "%s/%s", j->dest_parent, j->dest_name);
            if (!rebax_fs_remove_recursive(dest)) {
                snprintf(err, sizeof(err), "cannot remove %s", j->label);
                set_message(PKG_MSG_ERROR, err);
                failed = 1;
            }
        }
    }
    if (!failed) {
        SDL_LockMutex(g_lock);
        if (g_msg_kind != PKG_MSG_WARNING) { g_msg_kind = PKG_MSG_INFO; snprintf(g_msg, sizeof(g_msg), "Done."); }
        g_status_rev++;
        SDL_UnlockMutex(g_lock);
    }
    SDL_LockMutex(g_lock);
    g_busy = 0;
    g_title[0] = '\0';
    g_status_rev++;
    SDL_UnlockMutex(g_lock);
    return 0;
}

static int add_job(int install, kind_t kind, const char *file, const char *label, const char *parent, const char *name) {
    for (int i = 0; i < g_job_count; i++) {
        if (g_jobs[i].install == install && strcmp(g_jobs[i].dest_name, name) == 0 && strcmp(g_jobs[i].dest_parent, parent) == 0) return 1;
    }
    if (g_job_count >= MAX_JOBS) return 0;
    job_t *j = &g_jobs[g_job_count++];
    j->install = install;
    j->kind = kind;
    snprintf(j->file, sizeof(j->file), "%s", file);
    snprintf(j->label, sizeof(j->label), "%s", label);
    snprintf(j->dest_parent, sizeof(j->dest_parent), "%s", parent);
    snprintf(j->dest_name, sizeof(j->dest_name), "%s", name);
    return 1;
}

static int start_jobs(void) {
    if (g_job_count == 0) return 0;
    if (g_thread) { SDL_WaitThread(g_thread, NULL); g_thread = NULL; }
    SDL_LockMutex(g_lock);
    g_busy = 1;
    g_cancel = 0;
    g_msg_kind = PKG_MSG_NONE;
    g_msg[0] = '\0';
    g_status_rev++;
    SDL_UnlockMutex(g_lock);
    g_thread = SDL_CreateThread(worker, "rebax-packages", NULL);
    if (!g_thread) {
        g_busy = 0;
        set_message(PKG_MSG_ERROR, "cannot start the download thread");
        return 0;
    }
    return 1;
}

static int begin(void) {
    if (!g_ready || g_busy || !g_host_supported) return 0;
    if (g_thread) { SDL_WaitThread(g_thread, NULL); g_thread = NULL; }
    g_job_count = 0;
    return 1;
}

static void queue_host_clang(void) {
    char file[160], parent[PATH_CAP];
    if (host_clang_ok()) return;
    snprintf(file, sizeof(file), "rebax-clang-%s.tar.xz", g_host_text);
    snprintf(parent, sizeof(parent), "%s", rebax_toolchains_dir());
    add_job(1, KIND_CLANG, file, "the compiler (clang)", parent, "clang");
}

static void queue_arch(int p, int a) {
    char file[160], label[160], parent[PATH_CAP];
    const platform_def_t *pd = &PLATFORMS[p];
    if (p >= 5) {
        if (!gcc_ok(p)) {
            snprintf(file, sizeof(file), "rebax-gcc-%s-%s.tar.xz", p == 5 ? "ps2" : "ps1", g_host_text);
            snprintf(label, sizeof(label), "%s compiler (gcc)", pd->name);
            snprintf(parent, sizeof(parent), "%s", rebax_toolchains_dir());
            add_job(1, p == 5 ? KIND_GCC_PS2 : KIND_GCC_PS1, file, label, parent, p == 5 ? "gcc-ps2" : "gcc-ps1");
        }
    } else {
        queue_host_clang();
    }
    if (!sdk_ok(p, a)) {
        snprintf(file, sizeof(file), "rebax-sdk-%s.tar.xz", pd->sdk[a]);
        snprintf(label, sizeof(label), "%s SDK%s%s", pd->name, pd->arch_count ? " " : "", pd->arch_count ? pd->arch[a] : "");
        snprintf(parent, sizeof(parent), "%s", rebax_sdks_dir());
        add_job(1, KIND_SDK, file, label, parent, pd->sdk[a]);
    }
}

static void queue_remove_arch(int p, int a) {
    char label[160], parent[PATH_CAP];
    const platform_def_t *pd = &PLATFORMS[p];
    snprintf(label, sizeof(label), "%s%s%s", pd->name, pd->arch_count ? " " : "", pd->arch_count ? pd->arch[a] : "");
    snprintf(parent, sizeof(parent), "%s", rebax_sdks_dir());
    add_job(0, KIND_SDK, "", label, parent, pd->sdk[a]);
    if (p >= 5) {
        char gparent[PATH_CAP];
        snprintf(gparent, sizeof(gparent), "%s", rebax_toolchains_dir());
        add_job(0, p == 5 ? KIND_GCC_PS2 : KIND_GCC_PS1, "", "the gcc compiler", gparent, p == 5 ? "gcc-ps2" : "gcc-ps1");
    }
}

int package_install_arch(int p, int a) {
    if (!begin()) return 0;
    queue_arch(p, a);
    return start_jobs();
}

int package_install_platform(int p) {
    if (!begin()) return 0;
    int count = PLATFORMS[p].arch_count ? PLATFORMS[p].arch_count : 1;
    for (int a = 0; a < count; a++) queue_arch(p, a);
    return start_jobs();
}

int package_install_host_compiler(void) {
    if (!begin()) return 0;
    queue_host_clang();
    return start_jobs();
}

int package_uninstall_arch(int p, int a) {
    if (!begin()) return 0;
    queue_remove_arch(p, a);
    return start_jobs();
}

int package_uninstall_platform(int p) {
    if (!begin()) return 0;
    int count = PLATFORMS[p].arch_count ? PLATFORMS[p].arch_count : 1;
    for (int a = 0; a < count; a++) queue_remove_arch(p, a);
    return start_jobs();
}

const char *package_platform_name(int p) { return PLATFORMS[p].name; }
int package_platform_arch_count(int p) { return PLATFORMS[p].arch_count; }
int package_platform_has_archs(int p) { return PLATFORMS[p].arch_count > 0; }
const char *package_arch_name(int p, int a) { return PLATFORMS[p].arch[a]; }
const char *package_host_text(void) { return g_host_text; }
int package_host_supported(void) { return g_host_supported; }
pkg_state_t package_host_compiler_state(void) { return g_host_clang_state; }
unsigned long long package_states_revision(void) { return g_states_rev; }
int package_is_busy(void) { return g_busy; }

pkg_state_t package_arch_state(int p, int a) {
    return g_arch_state[p][PLATFORMS[p].arch_count ? a : 0];
}

pkg_state_t package_platform_state(int p) {
    int count = PLATFORMS[p].arch_count ? PLATFORMS[p].arch_count : 1;
    int installed = 0, any = 0;
    for (int a = 0; a < count; a++) {
        pkg_state_t s = g_arch_state[p][a];
        if (s == PKG_STATE_UNAVAILABLE) return PKG_STATE_UNAVAILABLE;
        if (s == PKG_STATE_INSTALLED) installed++;
        if (s != PKG_STATE_NOT_INSTALLED) any = 1;
    }
    if (installed == count) return PKG_STATE_INSTALLED;
    return any ? PKG_STATE_PARTIAL : PKG_STATE_NOT_INSTALLED;
}

void package_get_status(pkg_status_t *out) {
    if (!g_ready) { memset(out, 0, sizeof(*out)); return; }
    SDL_LockMutex(g_lock);
    out->busy = g_busy;
    out->percent = g_percent;
    out->extracting = g_extracting;
    snprintf(out->title, sizeof(out->title), "%s", g_title);
    out->message_kind = g_msg_kind;
    snprintf(out->message, sizeof(out->message), "%s", g_msg);
    out->revision = g_status_rev;
    SDL_UnlockMutex(g_lock);
}

void package_manager_init(void) {
    if (g_ready) return;
    g_lock = SDL_CreateMutex();
    detect_host();
    size_t size = 0;
    unsigned char *pem = resource_file_read("certs/cacert.pem", &size);
    if (pem) {
        rebax_https_set_ca_pem(pem, (unsigned long)size);
        free(pem);
    }
    g_ready = 1;
    package_refresh_states();
}

void package_manager_shutdown(void) {
    if (!g_ready) return;
    g_cancel = 1;
    if (g_thread) { SDL_WaitThread(g_thread, NULL); g_thread = NULL; }
    SDL_DestroyMutex(g_lock);
    g_lock = NULL;
    g_ready = 0;
}
