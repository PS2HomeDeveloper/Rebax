#include "libraries/apk/apk_manifest.h"

#include "libraries/apk/apk_axml.h"

#define STYLE_THEME_NOTITLEBAR_FULLSCREEN 0x01030007u

static void permission(apk_axml_t *a, const char *name) {
    apk_axml_start(a, "uses-permission");
    apk_axml_attr_string(a, "name", name);
    apk_axml_end(a);
}

int apk_manifest_build(const apk_pack_config_t *cfg, apk_buf_t *out) {
    apk_axml_t *a = apk_axml_new();
    if (!a) return 0;
    apk_axml_start(a, "manifest");
    apk_axml_attr_string(a, "package", cfg->package_name);
    apk_axml_attr_int(a, "versionCode", cfg->version_code);
    apk_axml_attr_string(a, "versionName", cfg->version_name);

    apk_axml_start(a, "uses-sdk");
    apk_axml_attr_int(a, "minSdkVersion", cfg->min_sdk);
    apk_axml_attr_int(a, "targetSdkVersion", cfg->target_sdk);
    apk_axml_end(a);

    permission(a, "android.permission.READ_EXTERNAL_STORAGE");
    permission(a, "android.permission.WRITE_EXTERNAL_STORAGE");
    permission(a, "android.permission.MANAGE_EXTERNAL_STORAGE");

    apk_axml_start(a, "uses-feature");
    apk_axml_attr_hex(a, "glEsVersion", 0x00020000u);
    apk_axml_attr_bool(a, "required", 1);
    apk_axml_end(a);

    apk_axml_start(a, "application");
    apk_axml_attr_string(a, "label", cfg->app_label);
    apk_axml_attr_bool(a, "allowBackup", 1);
    apk_axml_attr_bool(a, "requestLegacyExternalStorage", 1);
    apk_axml_attr_bool(a, "extractNativeLibs", 1);
    apk_axml_attr_bool(a, "hasCode", 1);

    apk_axml_start(a, "activity");
    apk_axml_attr_string(a, "name", "org.libsdl.app.SDLActivity");
    apk_axml_attr_bool(a, "exported", 1);
    apk_axml_attr_hex(a, "launchMode", 1);
    apk_axml_attr_hex(a, "screenOrientation", 0);
    apk_axml_attr_hex(a, "configChanges", 0x7b0u);
    apk_axml_attr_ref(a, "theme", STYLE_THEME_NOTITLEBAR_FULLSCREEN);

    apk_axml_start(a, "intent-filter");
    apk_axml_start(a, "action");
    apk_axml_attr_string(a, "name", "android.intent.action.MAIN");
    apk_axml_end(a);
    apk_axml_start(a, "category");
    apk_axml_attr_string(a, "name", "android.intent.category.LAUNCHER");
    apk_axml_end(a);
    apk_axml_end(a);

    apk_axml_start(a, "meta-data");
    apk_axml_attr_string(a, "name", "android.app.lib_name");
    apk_axml_attr_string(a, "value", "main");
    apk_axml_end(a);

    apk_axml_end(a);
    apk_axml_end(a);
    apk_axml_end(a);
    int ok = apk_axml_finish(a, out);
    apk_axml_free(a);
    return ok;
}
