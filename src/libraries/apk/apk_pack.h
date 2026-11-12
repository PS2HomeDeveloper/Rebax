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
} apk_pack_config_t;

int apk_pack(const apk_pack_config_t *cfg, char *err, size_t err_cap);

#endif
