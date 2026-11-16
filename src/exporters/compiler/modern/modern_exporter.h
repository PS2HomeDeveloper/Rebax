/*
 *  ____  _____ ____    _    __  __
 * |  _ \| ____| __ )  / \   \ \/ /
 * | |_) |  _| |  _ \ / _ \   \  /
 * |  _ <| |___| |_) / ___ \  /  \
 * |_| \_\\_____|____/_/   \_\/_/\_\
 *
 * Rebax Engine
 * Copyright (c) 2026 PS2HomeDeveloper
 *
 * Licensed under the MIT License.
 * See the LICENSE file for details.
 */

#ifndef REBAX_MODERN_EXPORTER_H
#define REBAX_MODERN_EXPORTER_H

typedef enum {
    MODERN_EXPORT_IDLE,
    MODERN_EXPORT_RUNNING,
    MODERN_EXPORT_SUCCESS,
    MODERN_EXPORT_FAILED
} modern_export_status_t;

int modern_export_start(const char *name, int release, const char *output_dir,
                        const char *platform, const char *architecture);
int modern_export_start_multi(const char *name, int release, const char *output_dir,
                              const char *platform, const char *const *architectures, int count);
void modern_export_update(void);
void modern_export_cancel(void);
modern_export_status_t modern_export_get_status(void);
const char *modern_export_current_architecture(void);
int modern_export_architecture_index(void);
int modern_export_architecture_count(void);

#endif
