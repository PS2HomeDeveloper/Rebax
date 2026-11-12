#ifndef APK_SIGN_H
#define APK_SIGN_H

#include "libraries/apk/apk_buffer.h"

typedef struct apk_key apk_key_t;

apk_key_t *apk_key_open(const char *dir, char *err, size_t err_cap);
void apk_key_close(apk_key_t *k);
int apk_sign_v2_block(apk_key_t *k, const unsigned char top_digest[32], apk_buf_t *block, char *err, size_t err_cap);

#endif
