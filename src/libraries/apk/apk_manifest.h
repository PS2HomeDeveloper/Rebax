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

#ifndef APK_MANIFEST_H
#define APK_MANIFEST_H

#include "libraries/bytes/byte_buffer.h"
#include "libraries/apk/apk_pack.h"

int apk_manifest_build(const apk_pack_config_t *cfg, byte_buf_t *out);

#endif
