/*
 *  ____  _____ ____    _    __  __
 * |  _ \| ____| __ )  / \   \ \/ /
 * | |_) | _| | _ \ / _ \   \  /
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
#include "tool_identity.h"
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static uint16_t u16le(const unsigned char *p) { return (uint16_t)p[0] | (uint16_t)p[1] << 8; }
static uint32_t u32le(const unsigned char *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
static const char *machine_name(uint32_t m) {
    if (m == 3 || m == 0x14c || m == 7) return "x86";
    if (m == 62 || m == 0x8664 || m == 0x01000007) return "x86_64";
    if (m == 40 || m == 0x1c0 || m == 12) return "arm";
    if (m == 183 || m == 0xaa64 || m == 0x0100000c) return "arm64";
    return NULL;
}
static int identify_bytes(const unsigned char *b, size_t n, const char **format, const char **arch) {
    if (n >= 8 && memcmp(b, "!<arch>\n", 8) == 0) {
        if (n < 68) return -1;
        size_t member = 60;
        while (member + 8 <= n && (b[member] == '/' || b[member] == ' ')) {
            size_t size = 0; char field[12];
            memcpy(field, b + member + 48, 10); field[10] = '\0';
            if (sscanf(field, "%zu", &size) != 1 || member + 60 + size > n) return -1;
            if (size > 0 && identify_bytes(b + member + 60, size, format, arch) == 0) return 0;
            member += 60 + size + (size & 1u);
            if (member >= n) break;
        }
        return -1;
    }
    if (n >= 20 && b[0] == 0x7f && b[1] == 'E' && b[2] == 'L' && b[3] == 'F') {
        const char *a = machine_name(u16le(b + 18)); if (!a) return -1;
        *format = "ELF"; *arch = a; return 0;
    }
    if (n >= 64 && b[0] == 'M' && b[1] == 'Z') {
        uint32_t pe = u32le(b + 0x3c);
        if (pe + 6 <= n && b[pe] == 'P' && b[pe + 1] == 'E' && b[pe + 2] == 0 && b[pe + 3] == 0) {
            const char *a = machine_name(u16le(b + pe + 4)); if (!a) return -1;
            *format = "PE"; *arch = a; return 0;
        }
    }
    if (n >= 8) {
        uint32_t m = u32le(b);
        if (m == 0xfeedface || m == 0xcefaedfe || m == 0xfeedfacf || m == 0xcffaedfe) {
            const char *a = machine_name(u32le(b + 4)); if (!a) return -1;
            *format = "Mach-O"; *arch = a; return 0;
        }
    }
    return -1;
}
int rebax_library_identity(const char *path, const char *architecture, char *description, int cap) {
    FILE *f = fopen(path, "rb"); unsigned char *buffer; size_t n;
    const char *format = NULL, *arch = NULL;
    if (!f) return -1;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return -1; }
    long length = ftell(f);
    if (length <= 0 || length > 64L * 1024L * 1024L) { fclose(f); return -1; }
    rewind(f); buffer = (unsigned char *)malloc((size_t)length);
    if (!buffer) { fclose(f); return -1; }
    n = fread(buffer, 1, (size_t)length, f); fclose(f);
    if (identify_bytes(buffer, n, &format, &arch) != 0) { free(buffer); return -1; }
    if (description && cap > 0) snprintf(description, (size_t)cap, "%s %s", format, arch);
    int result = architecture == NULL || strcmp(architecture, arch) == 0 ? 1 : 0;
    free(buffer); return result;
}
int cmd_identify_lib(int argc, char **argv) {
    char description[64]; int ok;
    if (argc != 3) { fprintf(stderr, "usage: identify-lib <path>\n"); return 2; }
    ok = rebax_library_identity(argv[2], NULL, description, sizeof(description));
    if (ok < 0) { printf("unknown\n"); return 0; }
    printf("%s\n", description); return 0;
}
