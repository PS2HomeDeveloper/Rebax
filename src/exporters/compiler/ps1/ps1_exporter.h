/*
 *  ____  _____ ____    _    __  __
 * |  _ \| ____| __ )  / \   \ \/ /
 * | |_) |  _| |  _ \ / _ \   \  / 
 * |  _ <| |___| |_) / ___ \  /  \ 
 * |_| \_\_____|____/_/   \_\/_/\_\
 *
 * Rebax Engine
 * Copyright (c) 2026 PS1HomeDeveloper
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

/*
 * Public Native PS1 exporter API.
 *
 * The implementation is split into private export_* modules. Callers must
 * use this header only; the internal module layout may evolve without
 * changing editor integration. The exporter analyzes Rebax project data on
 * the host, generates ordinary native PS1 C sources/headers, and
 * invokes the PS1 toolchain directly to produce the final ELF. The PS1 executable
 * does not parse .rscene files or require an editor-side interpreter.
 */
#ifndef PS1_EXPORTER_H
#define PS1_EXPORTER_H

typedef enum {
    PS1_EXPORT_STATUS_IDLE,
    PS1_EXPORT_STATUS_RUNNING,
    PS1_EXPORT_STATUS_SUCCESS,
    PS1_EXPORT_STATUS_FAILED
} ps1_export_status_t;

/* Start a new export. exe_name is used without an extension; ".elf" is
 * added automatically. is_release=1 enables post-link symbol stripping.
 * output_dir is the absolute destination directory for the final ELF.
 * Returns 1 when the export starts, otherwise 0. */
int ps1_export_start(const char *exe_name, int is_release, const char *output_dir);

/* Advance the current export by one non-blocking step. Call once per frame
 * while the export dialog is active. */
void ps1_export_update(void);

ps1_export_status_t ps1_export_get_status(void);

/* Return the oldest unread exporter log line, or NULL when no line is ready.
 * The returned pointer is owned internally and remains valid until the next
 * exporter log operation. Copy it if it must outlive that operation. */
const char *ps1_export_poll_next_line(void);

/* Cancel the current export and return to the idle state. */
void ps1_export_cancel(void);

#endif /* PS1_EXPORTER_H */
