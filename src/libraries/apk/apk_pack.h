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

#ifndef APK_PACK_H
#define APK_PACK_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    const char *stage_dir;
    const char *out_path;
    const char *keys_dir;
    const char *package_name;
    const char *app_label;
    const char *version_name;
    uint32_t version_code;
    uint32_t min_sdk;
    uint32_t target_sdk;
    uint32_t screen_orientation;
    const char *activity_name;
} apk_pack_config_t;

int apk_pack(const apk_pack_config_t *cfg, char *err, size_t err_cap);

#endif
