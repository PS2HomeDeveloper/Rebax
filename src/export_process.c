/* Export process/logging module. Owns non-blocking toolchain I/O only. */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdarg.h>
#if defined(_WIN32)
#include <io.h>
#endif
#include "export_internal.h"

#include <signal.h>
#include <stdint.h>
#include <sys/stat.h>
#include "rebax_paths.h"
#if defined(_WIN32)
#include <windows.h>
#endif
#ifndef O_BINARY
#define O_BINARY 0
#endif

static int g_trace_fd = -1;

static void trace_write_raw(const char *text) {
    if (g_trace_fd < 0) return;
    size_t n = strlen(text);
    if (write(g_trace_fd, text, (unsigned int)n) < 0) return;
}

static int trace_append(char *line, int len, const char *text) {
    while (*text) line[len++] = *text++;
    return len;
}

static int trace_append_hex(char *line, int len, unsigned long long value, int digits) {
    const char *table = "0123456789abcdef";
    for (int shift = (digits - 1) * 4; shift >= 0; shift -= 4) line[len++] = table[(value >> shift) & 0xF];
    return len;
}

#if defined(_WIN32)
static LONG WINAPI trace_crash_filter(EXCEPTION_POINTERS *info) {
    char line[96];
    int len = trace_append(line, 0, "CRASH exception=0x");
    len = trace_append_hex(line, len, (unsigned long long)info->ExceptionRecord->ExceptionCode, 8);
    len = trace_append(line, len, " address=0x");
    len = trace_append_hex(line, len, (unsigned long long)(uintptr_t)info->ExceptionRecord->ExceptionAddress, 16);
    line[len++] = '\n';
    line[len] = '\0';
    trace_write_raw(line);
    return EXCEPTION_CONTINUE_SEARCH;
}
#else
static void trace_crash_handler(int sig, siginfo_t *info, void *ctx) {
    (void)ctx;
    char line[96];
    int len = trace_append(line, 0, "CRASH signal=");
    line[len++] = (char)('0' + (sig / 10) % 10);
    line[len++] = (char)('0' + sig % 10);
    len = trace_append(line, len, " address=0x");
    len = trace_append_hex(line, len, (unsigned long long)(uintptr_t)info->si_addr, 16);
    line[len++] = '\n';
    line[len] = '\0';
    trace_write_raw(line);
    signal(sig, SIG_DFL);
    raise(sig);
}
#endif

void export_trace_begin(void) {
    char path[1700];
#if defined(__ANDROID__)
    snprintf(path, sizeof(path), "%s", "/storage/emulated/0/Rebax_export_log.txt");
#else
    snprintf(path, sizeof(path), "%s/export_log.txt", rebax_root_dir());
#endif
    if (g_trace_fd >= 0) close(g_trace_fd);
    g_trace_fd = open(path, O_WRONLY | O_CREAT | O_TRUNC | O_BINARY, 0666);
#if defined(_WIN32)
    SetUnhandledExceptionFilter(trace_crash_filter);
#else
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = trace_crash_handler;
    sa.sa_flags = SA_SIGINFO;
    sigaction(SIGSEGV, &sa, NULL);
    sigaction(SIGBUS, &sa, NULL);
    sigaction(SIGABRT, &sa, NULL);
    sigaction(SIGFPE, &sa, NULL);
    sigaction(SIGILL, &sa, NULL);
    sigaction(SIGPIPE, &sa, NULL);
#endif
}

void export_trace(const char *text) {
    trace_write_raw(text);
    trace_write_raw("\n");
}

/* ------------------------------------------------------------
 * Output lines queue - one line per item, circular with fixed size. Reading
 * is sequential (ps2_export_poll_next_line) with no need for threads (everything
 * here is single-threaded, polled each frame from the UI) */
#define LOG_LINE_MAX EXPORT_LOG_LINE_MAX
#define LOG_QUEUE_SIZE EXPORT_LOG_QUEUE_SIZE

static char g_log_lines[LOG_QUEUE_SIZE][LOG_LINE_MAX];
static int  g_log_head = 0;   /* The first line has not yet been consumed */
static int  g_log_tail = 0;   /* First empty slot for the next write */
static int  g_log_count = 0;

void log_reset(void) {
    g_log_head = g_log_tail = g_log_count = 0;
}

void log_push(const char *text) {
    /* Printed immediately to the program's own output (the terminal you launched it from) -
     * completely independent of any window rendering issues, useful for debugging during
     * long operations (e.g. unpacking the large ps2dev archive) */
    printf("%s\n", text);
    fflush(stdout);
    export_trace(text);

    /* If the queue fills, drop the oldest line (instead of stopping export) - diagnostic
     * log, not critical data that must be preserved verbatim forever */
    if (g_log_count == LOG_QUEUE_SIZE) {
        g_log_head = (g_log_head + 1) % LOG_QUEUE_SIZE;
        g_log_count--;
    }
    strncpy(g_log_lines[g_log_tail], text, LOG_LINE_MAX - 1);
    g_log_lines[g_log_tail][LOG_LINE_MAX - 1] = '\0';
    g_log_tail = (g_log_tail + 1) % LOG_QUEUE_SIZE;
    g_log_count++;
}

void log_pushf(const char *fmt, ...) {
    char buf[LOG_LINE_MAX];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    log_push(buf);
}

const char *ps2_export_poll_next_line(void) {
    if (g_log_count == 0) return NULL;
    const char *line = g_log_lines[g_log_head];
    g_log_head = (g_log_head + 1) % LOG_QUEUE_SIZE;
    g_log_count--;
    return line;
}

/* ------------------------------------------------------------
 * Run a single shell step (one command) non-blocking - its output
 * (stdout+stderr combined, "2>&1" at the end of each command) is read
 * line-by-line on each poll call, with no waiting. This is the only
 * approach used for slow processes in this file (unpacking archives, make)
 * — no threads
 * ------------------------------------------------------------ */
/* g_step is owned by ps2_exporter.c and shared through export_internal.h. */

int shell_step_start(export_shell_step_t *step, const char *command) {
    step->pipe = popen(command, "r");
    if (step->pipe == NULL) {
        return 0;
    }
    step->fd = fileno(step->pipe);
    #if defined(_WIN32)
    int flags = _setmode(step->fd, _O_BINARY);
#else
    int flags = fcntl(step->fd, F_GETFL, 0);
    fcntl(step->fd, F_SETFL, flags | O_NONBLOCK);
#endif
    step->active = 1;
    step->partial_len = 0;
    step->partial[0] = '\0';
    return 1;
}

/* Returns: 1 = still running (no final result yet), 0 = finished (out_ok
 * carries success/failure according to exit code) */
int shell_step_poll(export_shell_step_t *step, int *out_ok) {
    if (!step->active) {
        *out_ok = 0;
        return 0;
    }

    char chunk[512];
    ssize_t n;
    while ((n = read(step->fd, chunk, sizeof(chunk) - 1)) > 0) {
        chunk[n] = '\0';
        for (ssize_t i = 0; i < n; i++) {
            char c = chunk[i];
            if (c == '\n') {
                step->partial[step->partial_len] = '\0';
                log_push(step->partial);
                step->partial_len = 0;
            } else if (step->partial_len < LOG_LINE_MAX - 1) {
                step->partial[step->partial_len++] = c;
            }
        }
    }

    if (n == 0) {
        /* Pipe actually ended - process finished */
        if (step->partial_len > 0) {
            step->partial[step->partial_len] = '\0';
            log_push(step->partial);
            step->partial_len = 0;
        }
        int status = pclose(step->pipe);
        step->active = 0;
        *out_ok = (status == 0);
        return 0;
    }

    /* n < 0: either EAGAIN/EWOULDBLOCK (no data ready now - normal
     * for non-blocking reads, return "still running") or some other real error */
    if (errno != EAGAIN && errno != EWOULDBLOCK) {
        log_pushf("[exporter] shell read error: %s", strerror(errno));
        pclose(step->pipe);
        step->active = 0;
        *out_ok = 0;
        return 0;
    }

    return 1; /* still running */
}

void shell_step_cancel(export_shell_step_t *step) {
    if (step->active) {
        pclose(step->pipe);
        step->active = 0;
    }
}

