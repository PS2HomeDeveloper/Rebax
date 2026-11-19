/*
 *  ____  _____ ____    _    __  __
 * |  _ \| ____| __ )  / \   \ \/ /
 * | |_) | _| | _ \ / _ \   \  /
 * |  _ <| |___| |_) / ___ \  /  \\
 * |_| \_\_____|____/_/   \_\/ /_\
 *
 * Rebax Engine
 * Copyright (c) 2026 PS2HomeDeveloper
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "exporters/compiler/ps1/ps1_exe.h"

#define PS1_EXE_HEADER 2048u
#define PS1_EXE_SECTOR 2048u

static uint32_t read32(const unsigned char *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint16_t read16(const unsigned char *p) {
    return (uint16_t)(p[0] | (p[1] << 8));
}

static void write32(unsigned char *p, uint32_t v) {
    p[0] = (unsigned char)v;
    p[1] = (unsigned char)(v >> 8);
    p[2] = (unsigned char)(v >> 16);
    p[3] = (unsigned char)(v >> 24);
}

static int fail(char *error, size_t size, const char *message) {
    if (error != NULL && size > 0) snprintf(error, size, "%s", message);
    return 0;
}

static unsigned char *read_file(const char *path, size_t *size) {
    FILE *f = fopen(path, "rb");
    unsigned char *data;
    long length;
    if (f == NULL) return NULL;
    if (fseek(f, 0, SEEK_END) != 0 || (length = ftell(f)) < 0 || fseek(f, 0, SEEK_SET) != 0) { fclose(f); return NULL; }
    data = (unsigned char *)malloc((size_t)length + 1);
    if (data == NULL) { fclose(f); return NULL; }
    if (fread(data, 1, (size_t)length, f) != (size_t)length) { free(data); fclose(f); return NULL; }
    fclose(f);
    *size = (size_t)length;
    return data;
}

int ps1_exe_convert(const char *elf_path, const char *exe_path, char *error, size_t error_size) {
    size_t elf_size = 0;
    unsigned char *elf = read_file(elf_path, &elf_size);
    unsigned char *image = NULL;
    uint32_t phoff, phnum, phentsize, entry, base = 0xFFFFFFFFu, end = 0, span, padded;
    FILE *out;
    int ok = 0;
    if (elf == NULL) return fail(error, error_size, "cannot read the ELF file");
    if (elf_size < 52 || memcmp(elf, "\177ELF", 4) != 0 || elf[4] != 1 || elf[5] != 1)
        { free(elf); return fail(error, error_size, "the file is not a 32-bit little-endian ELF"); }
    if (read16(elf + 18) != 8) { free(elf); return fail(error, error_size, "the ELF is not built for MIPS"); }
    entry = read32(elf + 24);
    phoff = read32(elf + 28);
    phentsize = read16(elf + 42);
    phnum = read16(elf + 44);
    if (phentsize < 32 || phoff + (size_t)phnum * phentsize > elf_size)
        { free(elf); return fail(error, error_size, "the ELF program headers are invalid"); }
    for (uint32_t i = 0; i < phnum; i++) {
        const unsigned char *ph = elf + phoff + (size_t)i * phentsize;
        uint32_t vaddr = read32(ph + 8), filesz = read32(ph + 16);
        if (read32(ph) != 1 || filesz == 0) continue;
        if (vaddr < base) base = vaddr;
        if (vaddr + filesz > end) end = vaddr + filesz;
    }
    if (base == 0xFFFFFFFFu || end <= base) { free(elf); return fail(error, error_size, "the ELF has no loadable code"); }
    span = end - base;
    if (span > 0x1F0000u) { free(elf); return fail(error, error_size, "the program is larger than the 2 MB PlayStation memory"); }
    padded = (span + PS1_EXE_SECTOR - 1u) / PS1_EXE_SECTOR * PS1_EXE_SECTOR;
    image = (unsigned char *)calloc(1, PS1_EXE_HEADER + padded);
    if (image == NULL) { free(elf); return fail(error, error_size, "out of memory"); }
    for (uint32_t i = 0; i < phnum; i++) {
        const unsigned char *ph = elf + phoff + (size_t)i * phentsize;
        uint32_t offset = read32(ph + 4), vaddr = read32(ph + 8), filesz = read32(ph + 16);
        if (read32(ph) != 1 || filesz == 0) continue;
        if ((size_t)offset + filesz > elf_size) { free(elf); free(image); return fail(error, error_size, "the ELF segment is truncated"); }
        memcpy(image + PS1_EXE_HEADER + (vaddr - base), elf + offset, filesz);
    }
    memcpy(image, "PS-X EXE", 8);
    write32(image + 16, entry);
    write32(image + 20, 0);
    write32(image + 24, base);
    write32(image + 28, padded);
    out = fopen(exe_path, "wb");
    if (out != NULL) {
        ok = fwrite(image, 1, PS1_EXE_HEADER + padded, out) == PS1_EXE_HEADER + padded;
        if (fclose(out) != 0) ok = 0;
    }
    free(elf);
    free(image);
    if (!ok) return fail(error, error_size, "cannot write the PS-EXE file");
    return 1;
}
