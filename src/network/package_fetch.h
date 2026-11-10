#ifndef REBAX_PACKAGE_FETCH_H
#define REBAX_PACKAGE_FETCH_H

#include "rebax_archive.h"

typedef struct {
    rebax_progress_fn on_download;
    rebax_progress_fn on_extract;
    void (*on_warning)(const char *text, void *user);
    void *user;
    const volatile int *cancel;
} rebax_fetch_hooks_t;

int rebax_package_download(const char *base_url, const char *file_name, const char *work_dir, const char *dest_path, const rebax_fetch_hooks_t *hooks, char *error, unsigned long error_cap);

int rebax_package_fetch(const char *base_url, const char *file_name, const char *work_dir, const char *extract_dir, const rebax_fetch_hooks_t *hooks, char *error, unsigned long error_cap);

#endif
