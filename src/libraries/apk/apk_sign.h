/*
 *  ____  _____ ____    _    __  __
 * |  _ \| ____| __ )  / \   \ \/ /
 * | |_) |  _| |  _ \ / _ \   \  / 
 * |  _ <| |___| |_) / ___ \  /  \ 
 * |_| \_\_____|____/_/   \_\/_/\_\
 *
 * Rebax Engine
 * Copyright (c) 2026 PS2HomeDeveloper
 *
 * Licensed under the MIT License.
 * See the LICENSE file for details.
 */

#ifndef APK_SIGN_H
#define APK_SIGN_H

#include "libraries/bytes/byte_buffer.h"

typedef struct apk_key apk_key_t;

apk_key_t *apk_key_open(const char *dir, char *err, size_t err_cap);
void apk_key_close(apk_key_t *k);
int apk_sign_v2_block(apk_key_t *k, const unsigned char top_digest[32], byte_buf_t *block, char *err, size_t err_cap);

#endif
