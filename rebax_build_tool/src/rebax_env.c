#define _POSIX_C_SOURCE 200809L

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "network/https_client.h"
#include "network/package_fetch.h"
#include "rebax_env.h"
#include "rebax_fs.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

#define TOOLCHAINS_URL "https://github.com/PS2HomeDeveloper/Rebax-Toolchains/releases/latest/download"
#define SDKS_URL "https://github.com/PS2HomeDeveloper/Rebax-SDKs/releases/latest/download"
#define PATH_CAP 1024
#define FLAGS_CAP 8192

typedef struct {
    const char *platform;
    const char *arch;
    const char *sdk;
    const char *triple;
    const char *libdir;
    const char *gcc[3];
} target_t;

static const target_t TARGETS[] = {
    { "linux", "x86_64", "linux-x86_64", "x86_64-linux-musl", "", { "x86_64-linux-gnu-gcc", "x86_64-linux-musl-gcc", NULL } },
    { "linux", "arm64", "linux-arm64", "aarch64-linux-musl", "", { "aarch64-linux-gnu-gcc", "aarch64-linux-musl-gcc", NULL } },
    { "linux", "x86", "linux-x86", "i686-linux-musl", "", { "i686-linux-gnu-gcc", "i686-linux-musl-gcc", NULL } },
    { "linux", "arm", "linux-arm", "armv7-linux-musleabihf", "", { "arm-linux-gnueabihf-gcc", "arm-linux-musleabihf-gcc", NULL } },
    { "windows", "x86_64", "windows-x86_64", "x86_64-w64-mingw32", "", { "x86_64-w64-mingw32-gcc", NULL, NULL } },
    { "windows", "x86", "windows-x86", "i686-w64-mingw32", "", { "i686-w64-mingw32-gcc", NULL, NULL } },
    { "windows", "arm64", "windows-arm64", "aarch64-w64-mingw32", "", { "aarch64-w64-mingw32-gcc", NULL, NULL } },
    { "windows", "arm", "windows-arm", "armv7-w64-mingw32", "", { "armv7-w64-mingw32-gcc", NULL, NULL } },
    { "android", "arm64-v8a", "android-arm64-v8a", "aarch64-linux-android21", "aarch64-linux-android", { NULL, NULL, NULL } },
    { "android", "armeabi-v7a", "android-armeabi-v7a", "armv7a-linux-androideabi21", "arm-linux-androideabi", { NULL, NULL, NULL } },
    { "android", "x86", "android-x86", "i686-linux-android21", "i686-linux-android", { NULL, NULL, NULL } },
    { "android", "x86_64", "android-x86_64", "x86_64-linux-android21", "x86_64-linux-android", { NULL, NULL, NULL } },
    { "macos", "x86_64", "macos-x86_64", "x86_64-apple-macos10.13", "", { NULL, NULL, NULL } },
    { "macos", "arm64", "macos-arm64", "arm64-apple-macos11", "", { NULL, NULL, NULL } },
    { "ios", "arm64", "ios-arm64", "arm64-apple-ios13.0", "", { NULL, NULL, NULL } },
    { "ios", "x86_64", "ios-simulator-x86_64", "x86_64-apple-ios13.0-simulator", "", { NULL, NULL, NULL } }
};

static void fail(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, "rebax: ");
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    va_end(ap);
    exit(1);
}

static void append(char *dst, size_t cap, const char *fmt, ...) {
    size_t n = strlen(dst);
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(dst + n, cap - n, fmt, ap);
    va_end(ap);
}

static int run(char *const argv[], const char *out_path) {
#ifdef _WIN32
    char line[FLAGS_CAP * 2];
    line[0] = '\0';
    for (int i = 0; argv[i]; i++) append(line, sizeof(line), "%s\"%s\"", i ? " " : "", argv[i]);
    SECURITY_ATTRIBUTES sa = { sizeof(sa), NULL, TRUE };
    HANDLE out = CreateFileA(out_path ? out_path : "NUL", GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa,
                             CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (out == INVALID_HANDLE_VALUE) return -1;
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    si.hStdOutput = out;
    si.hStdError = out_path ? GetStdHandle(STD_ERROR_HANDLE) : out;
    if (!CreateProcessA(NULL, line, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)) { CloseHandle(out); return -1; }
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD code = 1;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    CloseHandle(out);
    return (int)code;
#else
    fflush(stdout);
    pid_t pid = fork();
    if (pid < 0) return -1;
    if (pid == 0) {
        int fd = open(out_path ? out_path : "/dev/null", O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd >= 0) {
            dup2(fd, 1);
            if (!out_path) dup2(fd, 2);
            close(fd);
        }
        execvp(argv[0], argv);
        _exit(127);
    }
    int status = 0;
    if (waitpid(pid, &status, 0) < 0) return -1;
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
#endif
}

static int run_visible(char *const argv[]) {
#ifdef _WIN32
    char line[FLAGS_CAP];
    line[0] = '\0';
    for (int i = 0; argv[i]; i++) append(line, sizeof(line), "%s\"%s\"", i ? " " : "", argv[i]);
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    if (!CreateProcessA(NULL, line, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)) return -1;
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD code = 1;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return (int)code;
#else
    fflush(stdout);
    pid_t pid = fork();
    if (pid < 0) return -1;
    if (pid == 0) {
        execvp(argv[0], argv);
        _exit(127);
    }
    int status = 0;
    if (waitpid(pid, &status, 0) < 0) return -1;
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
#endif
}

static int find_in_path(const char *name, char *out, size_t cap) {
    const char *path = getenv("PATH");
    if (!path) return 0;
#ifdef _WIN32
    const char sep = ';';
    const char *ext = ".exe";
#else
    const char sep = ':';
    const char *ext = "";
#endif
    const char *p = path;
    while (*p) {
        const char *end = strchr(p, sep);
        size_t len = end ? (size_t)(end - p) : strlen(p);
        if (len > 0 && len < cap - 64) {
            snprintf(out, cap, "%.*s/%s%s", (int)len, p, name, ext);
            if (rebax_fs_exists(out) && !rebax_fs_is_dir(out)) return 1;
        }
        if (!end) break;
        p = end + 1;
    }
    return 0;
}

static const target_t *find_target(const char *platform, const char *arch) {
    for (size_t i = 0; i < sizeof(TARGETS) / sizeof(TARGETS[0]); i++) {
        if (strcmp(TARGETS[i].platform, platform) == 0 && strcmp(TARGETS[i].arch, arch) == 0) return &TARGETS[i];
    }
    return NULL;
}

static int has_children(const char *dir, const char *const *names) {
    char path[PATH_CAP];
    for (int i = 0; names[i]; i++) {
        snprintf(path, sizeof(path), "%s/%s", dir, names[i]);
        if (!rebax_fs_is_dir(path)) return 0;
    }
    return 1;
}

static void show_progress(unsigned long long done, unsigned long long total, void *user) {
    static int last = -1;
    const char *label = (const char *)user;
    int pct = total ? (int)(done * 100 / total) : -1;
    if (pct == last) return;
    if (pct >= 0 && pct != 100 && pct % 10 != 0) return;
    last = pct;
    if (pct >= 0) printf("  %s %d%%\n", label, pct);
    else printf("  %s %llu KB\n", label, done / 1024);
    fflush(stdout);
}

static void show_warning(const char *text, void *user) {
    (void)user;
    printf("Warning: %s\n", text);
    fflush(stdout);
}

static void fetch(const char *base_url, const char *file_name, const char *extract_dir, const char *check_dir, const char *const *required) {
    if (rebax_fs_is_dir(check_dir) && has_children(check_dir, required)) return;
    rebax_fs_remove_recursive(check_dir);
    printf("Downloading %s/%s\n", base_url, file_name);
    fflush(stdout);
    rebax_https_set_ca_file("resources/certs/cacert.pem");
    rebax_fetch_hooks_t hooks;
    memset(&hooks, 0, sizeof(hooks));
    hooks.on_download = show_progress;
    hooks.on_extract = NULL;
    hooks.on_warning = show_warning;
    hooks.user = (void *)"downloaded";
    char err[512];
    char work[PATH_CAP];
    snprintf(work, sizeof(work), "%s", extract_dir);
    if (!rebax_package_fetch(base_url, file_name, work, extract_dir, &hooks, err, sizeof(err))) {
        rebax_fs_remove_recursive(check_dir);
        fail("%s (%s)", err, file_name);
    }
    if (!rebax_fs_is_dir(check_dir) || !has_children(check_dir, required)) {
        rebax_fs_remove_recursive(check_dir);
        fail("the package %s is incomplete", file_name);
    }
}

typedef struct {
    const char *libdir;
    char *out;
    size_t cap;
    int found;
} builtins_ctx_t;

static void find_builtins_cb(const char *path, void *user) {
    builtins_ctx_t *c = (builtins_ctx_t *)user;
    if (c->found) return;
    const char *name = path;
    for (const char *p = path; *p; p++) if (*p == '/' || *p == '\\') name = p + 1;
    size_t n = strlen(name);
    if (strncmp(name, "libclang_rt.builtins", 20) != 0 || n < 2 || strcmp(name + n - 2, ".a") != 0) return;
    if (c->libdir && c->libdir[0]) {
        char needle[PATH_CAP];
        snprintf(needle, sizeof(needle), "/%s/", c->libdir);
        if (!strstr(path, needle)) return;
    }
    snprintf(c->out, c->cap, "%s", path);
    c->found = 1;
}

static int find_builtins(const char *sdk, const char *libdir, char *out, size_t cap) {
    char root[PATH_CAP];
    snprintf(root, sizeof(root), "%s/runtime/lib", sdk);
    out[0] = '\0';
    builtins_ctx_t c = { libdir, out, cap, 0 };
    rebax_fs_walk_files(root, find_builtins_cb, &c);
    return c.found;
}

typedef struct {
    char cflags[FLAGS_CAP];
    char ldpre[FLAGS_CAP];
    char ldpost[FLAGS_CAP];
    char ldsdl[FLAGS_CAP];
} flags_t;

static void framework_list(flags_t *f, const char *sdk, const char *const *names) {
    char path[PATH_CAP];
    for (int i = 0; names[i]; i++) {
        snprintf(path, sizeof(path), "%s/standard/Frameworks/%s.framework", sdk, names[i]);
        if (rebax_fs_is_dir(path)) append(f->ldsdl, sizeof(f->ldsdl), " -framework %s", names[i]);
    }
}

static int clang_flags(flags_t *f, const target_t *t, const char *sdk, const char *builtins, const char *empty) {
    memset(f, 0, sizeof(*f));
    const char *p = t->platform;
    if (strcmp(p, "linux") == 0) {
        append(f->cflags, FLAGS_CAP, "--target=%s -nostdlibinc -isystem %s/runtime/include -isystem %s/standard/include -I%s/graphics/include/SDL2", t->triple, sdk, sdk, sdk);
        append(f->ldpre, FLAGS_CAP, "--target=%s -nostdlib -static -fuse-ld=lld -Wl,--eh-frame-hdr %s/standard/lib/crt1.o %s/standard/lib/crti.o", t->triple, sdk, sdk);
        append(f->ldpost, FLAGS_CAP, "-L%s/graphics/lib -L%s/runtime/lib -L%s/standard/lib", sdk, sdk, sdk);
        append(f->ldsdl, FLAGS_CAP, "-lSDL2 -lm -lc -lgcc %s/standard/lib/crtn.o", sdk);
        return 1;
    }
    if (strcmp(p, "windows") == 0) {
        char static_sdl[PATH_CAP];
        snprintf(static_sdl, sizeof(static_sdl), "%s/graphics/lib/libSDL2.a", sdk);
        append(f->cflags, FLAGS_CAP, "--target=%s --sysroot=%s/standard -I%s/graphics/include/SDL2", t->triple, sdk, sdk);
        append(f->ldpre, FLAGS_CAP, "--target=%s --sysroot=%s/standard -fuse-ld=lld -static -nodefaultlibs", t->triple, sdk);
        append(f->ldpost, FLAGS_CAP, "-L%s/runtime/lib -L%s/graphics/lib", sdk, sdk);
        if (rebax_fs_exists(static_sdl))
            append(f->ldsdl, FLAGS_CAP, "-lmingw32 -lSDL2main -lSDL2 -ldinput8 -ldxguid -ldxerr8 -luser32 -lgdi32 -lwinmm -limm32 -lole32 -loleaut32 -lshell32 -lsetupapi -lversion -luuid -lws2_32 -lbcrypt -lmingwex -lmoldname -lmsvcrt -lkernel32 -ladvapi32 %s", builtins);
        else
            append(f->ldsdl, FLAGS_CAP, "-lmingw32 -lSDL2main %s/graphics/lib/libSDL2.dll.a -lws2_32 -lbcrypt -lmingwex -lmoldname -lmsvcrt -lkernel32 -ladvapi32 -lshell32 -luser32 %s", sdk, builtins);
        return 1;
    }
    if (strcmp(p, "android") == 0) {
        append(f->cflags, FLAGS_CAP, "--target=%s --sysroot=%s/standard -fPIC -I%s/graphics/include/SDL2", t->triple, sdk, sdk);
        append(f->ldpre, FLAGS_CAP, "--target=%s --sysroot=%s/standard -fuse-ld=lld -nodefaultlibs", t->triple, sdk);
        append(f->ldpost, FLAGS_CAP, "-L%s/runtime/lib/%s -L%s/standard/usr/lib/%s/21 -L%s/standard/usr/lib/%s -L%s/graphics/lib", sdk, t->libdir, sdk, t->libdir, sdk, t->libdir, sdk);
        append(f->ldsdl, FLAGS_CAP, "-lSDL2 -liconv -lm -llog -landroid -lOpenSLES -lc -ldl %s", builtins);
        return 1;
    }
    append(f->cflags, FLAGS_CAP, "--target=%s -isysroot %s -nostdlibinc -isystem %s/standard/include -F%s/standard/Frameworks -F%s/graphics/Frameworks -I%s/graphics/include/SDL2", t->triple, empty, sdk, sdk, sdk, sdk);
    append(f->ldpre, FLAGS_CAP, "--target=%s -isysroot %s -fuse-ld=lld -nostdlib", t->triple, empty);
    append(f->ldpost, FLAGS_CAP, "-L%s/standard/lib -L%s/runtime/lib -L%s/graphics/lib", sdk, sdk, sdk);
    append(f->ldsdl, FLAGS_CAP, "-lSDL2 -liconv");
    if (strcmp(p, "macos") == 0) {
        static const char *const names[] = { "Cocoa", "CoreAudio", "AudioToolbox", "CoreVideo", "IOKit", "ForceFeedback", "Carbon", "CoreServices", "CoreFoundation", "Foundation", "AppKit", "Metal", "QuartzCore", "OpenGL", "CoreHaptics", "GameController", NULL };
        framework_list(f, sdk, names);
        append(f->ldsdl, FLAGS_CAP, " -lSystem %s/runtime/lib/libclang_rt.osx.a", sdk);
    } else {
        static const char *const names[] = { "UIKit", "Foundation", "CoreGraphics", "QuartzCore", "OpenGLES", "Metal", "AudioToolbox", "AVFoundation", "CoreAudio", "CoreMotion", "CoreHaptics", "GameController", "CoreBluetooth", "CoreFoundation", NULL };
        framework_list(f, sdk, names);
        char lib[PATH_CAP];
        snprintf(lib, sizeof(lib), "%s/runtime/lib/libclang_rt.iossim.a", sdk);
        if (!rebax_fs_exists(lib)) snprintf(lib, sizeof(lib), "%s/runtime/lib/libclang_rt.ios.a", sdk);
        append(f->ldsdl, FLAGS_CAP, " -lSystem %s", lib);
    }
    return 1;
}

static void gcc_flags(flags_t *f, const target_t *t, const char *sdk) {
    memset(f, 0, sizeof(*f));
    append(f->cflags, FLAGS_CAP, "-I%s/graphics/include/SDL2", sdk);
    append(f->ldpost, FLAGS_CAP, "-L%s/graphics/lib", sdk);
    if (strcmp(t->platform, "windows") == 0) append(f->ldsdl, FLAGS_CAP, "-lmingw32 -lSDL2main -lSDL2 -lws2_32 -lbcrypt");
    else append(f->ldsdl, FLAGS_CAP, "-lSDL2 -lm");
}

static int probe(const char *cc, const flags_t *f, const char *dir) {
    char src[PATH_CAP], obj[PATH_CAP], exe[PATH_CAP];
    rebax_fs_mkdir_p(dir);
    snprintf(src, sizeof(src), "%s/probe.c", dir);
    snprintf(obj, sizeof(obj), "%s/probe.o", dir);
    snprintf(exe, sizeof(exe), "%s/probe.out", dir);
    FILE *s = fopen(src, "wb");
    if (!s) return 0;
    fputs("int main(void) { return 0; }\n", s);
    fclose(s);
    char *argv[160];
    char copy[FLAGS_CAP * 3];
    int n;
    snprintf(copy, sizeof(copy), "%s %s -c %s -o %s", cc, f->cflags, src, obj);
    n = 0;
    for (char *tok = strtok(copy, " "); tok && n < 158; tok = strtok(NULL, " ")) argv[n++] = tok;
    argv[n] = NULL;
    if (run(argv, NULL) != 0) return 0;
    snprintf(copy, sizeof(copy), "%s %s %s %s %s -o %s", cc, f->ldpre, obj, f->ldpost, f->ldsdl, exe);
    n = 0;
    for (char *tok = strtok(copy, " "); tok && n < 158; tok = strtok(NULL, " ")) argv[n++] = tok;
    argv[n] = NULL;
    int rc = run(argv, NULL);
    rebax_fs_remove_recursive(dir);
    return rc == 0;
}

static void write_env(const char *out, const char *cc, const char *kind, const flags_t *f, const char *sdk, const char *clang_dir, const target_t *t) {
    char dir[PATH_CAP];
    snprintf(dir, sizeof(dir), "%s", out);
    for (size_t i = strlen(dir); i > 0; i--) {
        if (dir[i - 1] == '/' || dir[i - 1] == '\\') { dir[i - 1] = '\0'; break; }
    }
    rebax_fs_mkdir_p(dir);
    FILE *o = fopen(out, "wb");
    if (!o) fail("cannot write '%s'", out);
    fprintf(o, "TARGET_CC := %s\n", cc);
    fprintf(o, "TARGET_KIND := %s\n", kind);
    fprintf(o, "TARGET_CFLAGS := %s\n", f->cflags);
    fprintf(o, "TARGET_LDPRE := %s\n", f->ldpre);
    fprintf(o, "TARGET_LDPOST := %s %s\n", f->ldpost, f->ldsdl);
    fprintf(o, "SDK_DIR := %s\n", sdk);
    fprintf(o, "CLANG_DIR := %s\n", clang_dir);
    fprintf(o, "TARGET_LIBDIR := %s\n", t->libdir);
    fclose(o);
}

int rebax_env_main(int argc, char **argv) {
    if (argc != 7) fail("usage: env <target_platform> <target_arch> <host_platform> <host_arch> <out.mk>");
    const char *tp = argv[2], *ta = argv[3], *hp = argv[4], *ha = argv[5], *out = argv[6];
    const target_t *t = find_target(tp, ta);
    if (!t) fail("unsupported target %s-%s", tp, ta);
    int ios = strcmp(tp, "ios") == 0;
    int self = strcmp(tp, hp) == 0 && strcmp(ta, ha) == 0;
    if (self && ios) fail("iOS cannot build on itself");

    char sdk[PATH_CAP], clang_dir[PATH_CAP], extract[PATH_CAP], url[PATH_CAP], builtins[PATH_CAP], empty[PATH_CAP], probe_dir[PATH_CAP];
    snprintf(sdk, sizeof(sdk), "sdks/%s", t->sdk);
    static const char *const sdk_need[] = { "standard", "runtime", "graphics", NULL };
    snprintf(url, sizeof(url), "rebax-sdk-%s.tar.xz", t->sdk);
    fetch(SDKS_URL, url, "sdks", sdk, sdk_need);

    clang_dir[0] = '\0';
    if (!ios) {
        static const char *const clang_need[] = { "bin", NULL };
        snprintf(clang_dir, sizeof(clang_dir), "toolchains/%s-%s/clang", tp, ta);
        snprintf(extract, sizeof(extract), "toolchains/%s-%s", tp, ta);
        snprintf(url, sizeof(url), "rebax-clang-%s-%s.tar.xz", tp, ta);
        fetch(TOOLCHAINS_URL, url, extract, clang_dir, clang_need);
    }
    char host_clang_dir[PATH_CAP];
    if (self) {
        snprintf(host_clang_dir, sizeof(host_clang_dir), "%s", clang_dir);
    } else {
        static const char *const clang_need[] = { "bin", NULL };
        snprintf(host_clang_dir, sizeof(host_clang_dir), "toolchains/%s-%s/clang", hp, ha);
        (void)clang_need;
    }

    builtins[0] = '\0';
    if ((strcmp(tp, "windows") == 0 || strcmp(tp, "android") == 0) && !find_builtins(sdk, strcmp(tp, "android") == 0 ? t->libdir : "", builtins, sizeof(builtins)))
        fail("the compiler runtime library is missing in %s/runtime/lib", sdk);
    snprintf(empty, sizeof(empty), "build/empty_sysroot");
    snprintf(probe_dir, sizeof(probe_dir), "build/probe");
    rebax_fs_mkdir_p(empty);

    flags_t f;
    char cc[PATH_CAP];
    const char *exe_ext =
#ifdef _WIN32
        ".exe";
#else
        "";
#endif

    if (self) {
        snprintf(cc, sizeof(cc), "%s/bin/clang%s", host_clang_dir, exe_ext);
        if (!rebax_fs_exists(cc)) fail("the downloaded clang is missing: %s", cc);
        clang_flags(&f, t, sdk, builtins, empty);
        write_env(out, cc, "clang", &f, sdk, clang_dir, t);
        printf("Compiler: %s (downloaded clang)\n", cc);
        return 0;
    }

    if (find_in_path("clang", cc, sizeof(cc))) {
        snprintf(cc, sizeof(cc), "clang");
        clang_flags(&f, t, sdk, builtins, empty);
        if (probe(cc, &f, probe_dir)) {
            write_env(out, cc, "clang", &f, sdk, clang_dir, t);
            printf("Compiler: clang from the system\n");
            return 0;
        }
        printf("The system clang cannot build for %s-%s.\n", tp, ta);
    }

    for (int i = 0; i < 3 && t->gcc[i]; i++) {
        char found[PATH_CAP];
        if (!find_in_path(t->gcc[i], found, sizeof(found))) continue;
        gcc_flags(&f, t, sdk);
        if (probe(t->gcc[i], &f, probe_dir)) {
            write_env(out, t->gcc[i], "gcc", &f, sdk, clang_dir, t);
            printf("Compiler: %s from the system\n", t->gcc[i]);
            return 0;
        }
    }

    fail("no compiler found for %s-%s: install clang with lld, or a gcc that builds for that target", tp, ta);
    return 1;
}

int rebax_package_archive_main(int argc, char **argv) {
    if (argc != 5) fail("usage: package-archive <toolchains|sdks> <file_name> <out_file>");
    const char *base = strcmp(argv[2], "sdks") == 0 ? SDKS_URL : TOOLCHAINS_URL;
    const char *file = argv[3];
    const char *out = argv[4];
    if (rebax_fs_exists(out)) return 0;
    char dir[PATH_CAP];
    snprintf(dir, sizeof(dir), "%s", out);
    for (size_t i = strlen(dir); i > 0; i--) {
        if (dir[i - 1] == '/' || dir[i - 1] == '\\') { dir[i - 1] = '\0'; break; }
    }
    rebax_fs_mkdir_p(dir);
    printf("Downloading %s/%s\n", base, file);
    fflush(stdout);
    rebax_https_set_ca_file("resources/certs/cacert.pem");
    rebax_fetch_hooks_t hooks;
    memset(&hooks, 0, sizeof(hooks));
    hooks.on_download = show_progress;
    hooks.on_warning = show_warning;
    hooks.user = (void *)"downloaded";
    char err[512];
    char partial[PATH_CAP + 8];
    snprintf(partial, sizeof(partial), "%s.part", out);
    if (!rebax_package_download(base, file, dir, partial, &hooks, err, sizeof(err))) {
        remove(partial);
        fail("%s (%s)", err, file);
    }
    if (rename(partial, out) != 0) {
        remove(partial);
        fail("cannot write '%s'", out);
    }
    return 0;
}
