#ifndef APK_AXML_H
#define APK_AXML_H

#include "libraries/apk/apk_buffer.h"

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
int apk_axml_finish(apk_axml_t *a, apk_buf_t *out);

#endif
