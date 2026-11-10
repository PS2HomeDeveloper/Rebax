#ifndef RESOURCE_FILES_H
#define RESOURCE_FILES_H

#include <stddef.h>

unsigned char *resource_file_read(const char *relative_path, size_t *out_size);

#endif
