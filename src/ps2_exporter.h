/*
 * Public Native PS2 exporter API.
 *
 * The implementation is split into private export_* modules. Callers must
 * use this header only; the internal module layout may evolve without
 * changing editor integration. The exporter analyzes Rebax project data on
 * the host, generates ordinary native PS2 C/headers/Makefile inputs, and
 * invokes the PS2SDK toolchain to produce the final ELF. The PS2 executable
 * does not parse .rscene files or require an editor-side interpreter.
 */
#ifndef PS2_EXPORTER_H
#define PS2_EXPORTER_H

typedef enum {
    PS2_EXPORT_STATUS_IDLE,
    PS2_EXPORT_STATUS_RUNNING,
    PS2_EXPORT_STATUS_SUCCESS,
    PS2_EXPORT_STATUS_FAILED
} ps2_export_status_t;

/* Start a new export. exe_name is used without an extension; ".elf" is
 * added automatically. is_release=1 enables post-link symbol stripping.
 * output_dir is the absolute destination directory for the final ELF.
 * Returns 1 when the export starts, otherwise 0. */
int ps2_export_start(const char *exe_name, int is_release, const char *output_dir);

/* Advance the current export by one non-blocking step. Call once per frame
 * while the export dialog is active. */
void ps2_export_update(void);

ps2_export_status_t ps2_export_get_status(void);

/* Return the oldest unread exporter log line, or NULL when no line is ready.
 * The returned pointer is owned internally and remains valid until the next
 * exporter log operation. Copy it if it must outlive that operation. */
const char *ps2_export_poll_next_line(void);

/* Cancel the current export and return to the idle state. */
void ps2_export_cancel(void);

#endif /* PS2_EXPORTER_H */
