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
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef APK_AXML_H
#define APK_AXML_H

#include "libraries/bytes/byte_buffer.h"

typedef struct apk_axml apk_axml_t;

apk_axml_t *apk_axml_new(void);
void apk_axml_free(apk_axml_t *a);
int apk_axml_start(apk_axml_t *a, const char *tag);
int apk_axml_end(apk_axml_t *a);
int apk_axml_attr_string(apk_axml_t *a, const char *name, const char *value);
int apk_axml_attr_int(apk_axml_t *a, const char *name, uint32_t value);
int apk_axml_attr_hex(apk_axml_t *a, const char *name, uint32_t value);
int apk_axml_attr_bool(apk_axml_t *a, const char *name, int value);
int apk_axml_attr_ref(apk_axml_t *a, const char *name, uint32_t resource_id);
int apk_axml_finish(apk_axml_t *a, byte_buf_t *out);

#endif
