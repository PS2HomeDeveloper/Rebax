#ifndef REBAX_HTTPS_CLIENT_H
#define REBAX_HTTPS_CLIENT_H

#include "filesystem/rebax_archive.h"

void rebax_https_set_ca_file(const char *path);
void rebax_https_set_ca_pem(const unsigned char *pem, unsigned long length);

int rebax_https_download(const char *url, const char *dest_path, rebax_progress_fn progress, void *user, const volatile int *cancel, char *error, unsigned long error_cap);

#endif
