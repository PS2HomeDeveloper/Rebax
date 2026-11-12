#ifndef APK_MANIFEST_H
#define APK_MANIFEST_H

#include "libraries/apk/apk_buffer.h"
#include "libraries/apk/apk_pack.h"

int apk_manifest_build(const apk_pack_config_t *cfg, apk_buf_t *out);

#endif
