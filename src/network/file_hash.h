#ifndef REBAX_FILE_HASH_H
#define REBAX_FILE_HASH_H

int rebax_file_sha256(const char *path, char hex[65], unsigned long long *size_out);

#endif
