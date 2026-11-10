#include "file_hash.h"

#include <stdio.h>

#include "mbedtls/sha256.h"

int rebax_file_sha256(const char *path, char hex[65], unsigned long long *size_out) {
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    mbedtls_sha256_context ctx;
    mbedtls_sha256_init(&ctx);
    mbedtls_sha256_starts(&ctx, 0);
    unsigned char buf[65536];
    unsigned long long total = 0;
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
        mbedtls_sha256_update(&ctx, buf, n);
        total += n;
    }
    fclose(f);
    unsigned char digest[32];
    mbedtls_sha256_finish(&ctx, digest);
    mbedtls_sha256_free(&ctx);
    for (int i = 0; i < 32; i++) snprintf(hex + i * 2, 3, "%02x", digest[i]);
    if (size_out) *size_out = total;
    return 1;
}
