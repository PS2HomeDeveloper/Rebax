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

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "filesystem/path_utils.h"
#include "filesystem/rebax_fs.h"
#include "libraries/zip/zip_writer.h"
#include "tool_commands.h"
#include "tool_common.h"

int cmd_ios_plist(int argc, char **argv) {
    if (argc != 6) die("usage: ios-plist <out.plist> <version> <executable> <iPhoneOS|iPhoneSimulator>");
    ensure_parent_dir(argv[2]);
    const char *version = argv[3];
    if (*version == 'v' || *version == 'V') version++;
    int device = strcmp(argv[5], "iPhoneSimulator") != 0;
    char text[4096];
    snprintf(text, sizeof(text),
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"
        "<plist version=\"1.0\"><dict>"
        "<key>CFBundleDevelopmentRegion</key><string>en</string>"
        "<key>CFBundleExecutable</key><string>%s</string>"
        "<key>CFBundleIdentifier</key><string>org.rebax.engine</string>"
        "<key>CFBundleInfoDictionaryVersion</key><string>6.0</string>"
        "<key>CFBundleName</key><string>Rebax</string>"
        "<key>CFBundleDisplayName</key><string>Rebax</string>"
        "<key>CFBundlePackageType</key><string>APPL</string>"
        "<key>CFBundleShortVersionString</key><string>%s</string>"
        "<key>CFBundleVersion</key><string>%s</string>"
        "<key>CFBundleSupportedPlatforms</key><array><string>%s</string></array>"
        "<key>MinimumOSVersion</key><string>13.0</string>"
        "<key>LSRequiresIPhoneOS</key><true/>"
        "<key>UIDeviceFamily</key><array><integer>1</integer><integer>2</integer></array>"
        "%s"
        "<key>UIRequiresFullScreen</key><true/>"
        "<key>UIStatusBarHidden</key><true/>"
        "<key>UIFileSharingEnabled</key><true/>"
        "<key>LSSupportsOpeningDocumentsInPlace</key><true/>"
        "<key>UISupportedInterfaceOrientations</key><array>"
        "<string>UIInterfaceOrientationLandscapeLeft</string><string>UIInterfaceOrientationLandscapeRight</string>"
        "</array>"
        "</dict></plist>\n",
        argv[4], version, version, argv[5],
        device ? "<key>UIRequiredDeviceCapabilities</key><array><string>arm64</string></array>" : "");
    if (!write_text_file(argv[2], text)) die("cannot write '%s'", argv[2]);
    return 0;
}

typedef struct {
    char *rel;
    char *path;
} zip_item_t;

typedef struct {
    const char *root;
    zip_item_t *items;
    size_t count;
    size_t cap;
} zip_collect_t;

static void zip_collect_cb(const char *path, void *user) {
    zip_collect_t *z = (zip_collect_t *)user;
    const char *rel = path + strlen(z->root);
    while (is_sep(*rel)) rel++;
    if (z->count == z->cap) {
        z->cap = z->cap ? z->cap * 2 : 64;
        z->items = (zip_item_t *)xrealloc(z->items, z->cap * sizeof(zip_item_t));
    }
    z->items[z->count].rel = xstrdup(rel);
    for (char *q = z->items[z->count].rel; *q; q++) if (*q == '\\') *q = '/';
    z->items[z->count].path = xstrdup(path);
    z->count++;
}

static int zip_item_cmp(const void *a, const void *b) {
    return strcmp(((const zip_item_t *)a)->rel, ((const zip_item_t *)b)->rel);
}

int cmd_zip_dir(int argc, char **argv) {
    if (argc < 5) die("usage: zip-dir <out.zip> <base_dir> <entry> [--exec <suffix>]...");
    const char *out = argv[2];
    const char *base = argv[3];
    const char *entry = argv[4];
    const char *exec_suffix[16];
    int exec_count = 0;
    for (int i = 5; i + 1 < argc; i += 2) {
        if (strcmp(argv[i], "--exec") != 0 || exec_count >= 16) die("usage: zip-dir <out.zip> <base_dir> <entry> [--exec <suffix>]...");
        exec_suffix[exec_count++] = argv[i + 1];
    }
    char root[1700];
    snprintf(root, sizeof(root), "%s/%s", base, entry);
    if (!rebax_fs_is_dir(root)) die("'%s' is not a directory", root);

    zip_collect_t z;
    memset(&z, 0, sizeof(z));
    z.root = base;
    rebax_fs_walk_files(root, zip_collect_cb, &z);
    if (z.count == 0) die("'%s' contains no files", root);
    qsort(z.items, z.count, sizeof(zip_item_t), zip_item_cmp);

    rebax_fs_remove_recursive(out);
    ensure_parent_dir(out);
    zip_writer_t *zip = zip_writer_create(out);
    if (!zip) die("cannot write '%s'", out);
    for (size_t i = 0; i < z.count; i++) {
        int is_exec = 0;
        for (int k = 0; k < exec_count; k++) if (path_utils_has_suffix(z.items[i].rel, exec_suffix[k])) is_exec = 1;
        if (!zip_writer_add_file(zip, z.items[i].rel, z.items[i].path, 1, is_exec ? 0x81EDu : 0x81A4u)) die("%s", zip_writer_error(zip));
    }
    if (!zip_writer_finish(zip)) die("%s", zip_writer_error(zip));
    zip_writer_free(zip);

    for (size_t i = 0; i < z.count; i++) {
        free(z.items[i].rel);
        free(z.items[i].path);
    }
    free(z.items);
    return 0;
}
