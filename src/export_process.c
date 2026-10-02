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

/* ------------------------------------------------------------
 * طابور أسطر المخرجات - سطر بكل عنصر، دائري بحجم ثابت. القراءة
 * تسلسلية (ps2_export_poll_next_line) بلا أي حاجة لـthreads (كل
 * شيء هنا أحادي الخيط، يُستطلَع كل إطار من الواجهة) */
#define LOG_LINE_MAX EXPORT_LOG_LINE_MAX
#define LOG_QUEUE_SIZE EXPORT_LOG_QUEUE_SIZE

static char g_log_lines[LOG_QUEUE_SIZE][LOG_LINE_MAX];
static int  g_log_head = 0;   /* أول سطر لسه ما استُهلك */
static int  g_log_tail = 0;   /* أول خانة فاضية للكتابة القادمة */
static int  g_log_count = 0;

void log_reset(void) {
    g_log_head = g_log_tail = g_log_count = 0;
}

void log_push(const char *text) {
    /* يُطبَع فوراً لمخرجات البرنامج نفسه (التيرمنال اللي شغّلته منه) -
     * مستقل تماماً عن أي مشكلة برسم النافذة، ومفيد للتشخيص أثناء
     * عمليات طويلة (فك أرشيف ps2dev الضخم مثلاً) */
    printf("%s\n", text);
    fflush(stdout);

    /* لو الطابور امتلأ، نضحّي بأقدم سطر (بدل توقف التصدير) - سجل
     * تشخيصي، مو بيانات حرجة يلزم الاحتفاظ بكل حرف منها للأبد */
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
 * تشغيل خطوة شل واحدة (أمر واحد) بشكل غير حاجز - يُقرأ ناتجها
 * (stdout+stderr مدموجين، "2>&1" بنهاية كل أمر) سطراً سطراً كل
 * استدعاء poll، بلا أي انتظار. هذا الأسلوب الوحيد المستخدَم لكل
 * عملية بطيئة بهذا الملف (فك أرشيف، أمر make) - بلا أي threads
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

/* يرجع: 1 = لسه شغالة (بلا نتيجة نهائية بعد)، 0 = خلصت (out_ok
 * يحمل النجاح/الفشل حسب exit code) */
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
        /* نهاية الأنبوب فعلياً - العملية خلصت */
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

    /* n < 0: إما EAGAIN/EWOULDBLOCK (بلا بيانات جاهزة الآن - طبيعي
     * بقراءة غير حاجزة، نرجع "لسه شغالة") أو خطأ حقيقي آخر */
    if (errno != EAGAIN && errno != EWOULDBLOCK) {
        log_pushf("[exporter] shell read error: %s", strerror(errno));
        pclose(step->pipe);
        step->active = 0;
        *out_ok = 0;
        return 0;
    }

    return 1; /* لسه شغالة */
}

void shell_step_cancel(export_shell_step_t *step) {
    if (step->active) {
        pclose(step->pipe);
        step->active = 0;
    }
}

