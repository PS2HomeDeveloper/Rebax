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

#include "filesystem/rebax_fs.h"
#include "libraries/apk/apk_pack.h"
#include "tool_commands.h"
#include "tool_common.h"

static const char *file_arch(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return "none";
    unsigned char b[64];
    size_t n = fread(b, 1, sizeof(b), f);
    const char *result = "unknown";
    if (n >= 20 && b[0] == 0x7f && b[1] == 'E' && b[2] == 'L' && b[3] == 'F') {
        if (b[19] == 0x00 && b[18] == 0x3e) result = "x86_64";
        else if (b[19] == 0x00 && b[18] == 0x03) result = "x86";
        else if (b[19] == 0x00 && b[18] == 0xb7) result = "arm64";
        else if (b[19] == 0x00 && b[18] == 0x28) result = "arm";
    } else if (n >= 64 && b[0] == 'M' && b[1] == 'Z') {
        long off = (long)b[60] | ((long)b[61] << 8) | ((long)b[62] << 16) | ((long)b[63] << 24);
        unsigned char m[2];
        if (off >= 0 && fseek(f, off + 4, SEEK_SET) == 0 && fread(m, 1, 2, f) == 2) {
            if (m[0] == 0x64 && m[1] == 0x86) result = "x86_64";
            else if (m[0] == 0x4c && m[1] == 0x01) result = "x86";
            else if (m[0] == 0x64 && m[1] == 0xaa) result = "arm64";
            else if (m[0] == 0xc0 && m[1] == 0x01) result = "arm";
            else if (m[0] == 0xc4 && m[1] == 0x01) result = "arm";
        }
    } else if (n >= 8) {
        uint32_t h = ((uint32_t)b[0] << 24) | ((uint32_t)b[1] << 16) | ((uint32_t)b[2] << 8) | b[3];
        if (h == 0xcffaedfe || h == 0xcefaedfe || h == 0xfeedface || h == 0xfeedfacf || h == 0xcafebabe || h == 0xbebafeca) {
            uint32_t cm = ((uint32_t)b[4] << 24) | ((uint32_t)b[5] << 16) | ((uint32_t)b[6] << 8) | b[7];
            if (cm == 0x0c000001 || cm == 0x0100000c) result = "arm64";
            else if (cm == 0x07000001 || cm == 0x01000007) result = "x86_64";
            else if (cm == 0x0c000000 || cm == 0x0000000c) result = "arm";
            else if (cm == 0x07000000 || cm == 0x00000007) result = "x86";
        }
    }
    fclose(f);
    return result;
}

int cmd_android_entry(int argc, char **argv) {
    if (argc != 3) die("usage: android-entry <out.c>");
    ensure_parent_dir(argv[2]);
    if (!write_text_file(argv[2],
            "extern int main(void);\n"
            "int SDL_main(int argc, char **argv) { (void)argc; (void)argv; return main(); }\n"))
        die("cannot write '%s'", argv[2]);
    return 0;
}

int cmd_apk_pack(int argc, char **argv) {
    if (argc != 11) die("usage: apk-pack <stage_dir> <out.apk> <keys_dir> <package> <label> <version_name> <version_code> <min_sdk> <target_sdk>");
    apk_pack_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.stage_dir = argv[2];
    cfg.out_path = argv[3];
    cfg.keys_dir = argv[4];
    cfg.package_name = argv[5];
    cfg.app_label = argv[6];
    cfg.version_name = argv[7];
    cfg.version_code = (uint32_t)strtoul(argv[8], NULL, 10);
    cfg.min_sdk = (uint32_t)strtoul(argv[9], NULL, 10);
    cfg.target_sdk = (uint32_t)strtoul(argv[10], NULL, 10);
    ensure_parent_dir(cfg.out_path);
    char err[512];
    if (!apk_pack(&cfg, err, sizeof(err))) die("%s", err);
    return 0;
}

int cmd_require_lib(int argc, char **argv) {
    if (argc != 5 || argv[4][0] == '\0') {
        fprintf(stderr, "ERROR: no %s library matching the target was found\n", argc > 2 ? argv[2] : "required");
        return 1;
    }
    const char *label = argv[2];
    const char *expect = argv[3];
    const char *path = argv[4];
    if (!rebax_fs_exists(path)) {
        fprintf(stderr, "ERROR: %s (%s) was not found on disk; nothing was packaged\n", label, path);
        return 1;
    }
    const char *arch = file_arch(path);
    if (strcmp(arch, "unknown") != 0 && strcmp(arch, expect) != 0) {
        fprintf(stderr, "ERROR: %s is %s but the target is %s\n", path, arch, expect);
        return 1;
    }
    return 0;
}
