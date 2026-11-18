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

#include "libraries/ps2link/ps2link_host_files.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#include "libraries/ps2link/ps2link_protocol.h"

#define MAX_HANDLES 64
#define KIND_FREE 0
#define KIND_FILE 1
#define KIND_PING 2

typedef struct {
    int kind;
    FILE *file;
} handle_t;

static handle_t g_handles[MAX_HANDLES];
static char g_root[1024];
static int g_ping_answered;

static unsigned int get32(const unsigned char *p) {
    return ((unsigned int)p[0] << 24) | ((unsigned int)p[1] << 16) | ((unsigned int)p[2] << 8) | (unsigned int)p[3];
}

static void put32(unsigned char *p, unsigned int v) {
    p[0] = (unsigned char)(v >> 24);
    p[1] = (unsigned char)(v >> 16);
    p[2] = (unsigned char)(v >> 8);
    p[3] = (unsigned char)v;
}

static int send_response(ps2link_socket_t sock, unsigned int number, const unsigned char *body, int body_size) {
    unsigned char packet[PS2LINK_HEADER_SIZE + 320];
    int size = PS2LINK_HEADER_SIZE + body_size;
    if (body_size > 320) return 0;
    put32(packet, number);
    packet[4] = (unsigned char)(size >> 8);
    packet[5] = (unsigned char)size;
    memcpy(packet + PS2LINK_HEADER_SIZE, body, (size_t)body_size);
    return ps2link_socket_send_all(sock, packet, size);
}

static int respond_result(ps2link_socket_t sock, unsigned int number, int result) {
    unsigned char body[4];
    put32(body, (unsigned int)result);
    return send_response(sock, number, body, 4);
}

void ps2link_host_files_reset(const char *root_dir) {
    ps2link_host_files_close_all();
    snprintf(g_root, sizeof(g_root), "%s", root_dir ? root_dir : "");
    g_ping_answered = 0;
}

void ps2link_host_files_close_all(void) {
    for (int i = 0; i < MAX_HANDLES; i++) {
        if (g_handles[i].file != NULL) fclose(g_handles[i].file);
        g_handles[i].file = NULL;
        g_handles[i].kind = KIND_FREE;
    }
}

int ps2link_host_files_ping_answered(void) {
    return g_ping_answered;
}

static int resolve_path(const unsigned char *raw, char *out, size_t cap) {
    char relative[PS2LINK_PATH_SIZE + 1];
    size_t length = 0;
    const char *start;
    memcpy(relative, raw, PS2LINK_PATH_SIZE);
    relative[PS2LINK_PATH_SIZE] = '\0';
    for (char *c = relative; *c; c++) if (*c == '\\') *c = '/';
    start = relative;
    if (strncmp(start, "host:", 5) == 0) start += 5;
    while (*start == '/') start++;
    if (g_root[0] == '\0') return 0;
    if (strstr(start, "..") != NULL) return 0;
    if (start[0] != '\0' && start[1] == ':') return 0;
    length = strlen(g_root);
    if (length + 1 + strlen(start) + 1 > cap) return 0;
    snprintf(out, cap, "%s%s%s", g_root, (length > 0 && g_root[length - 1] != '/') ? "/" : "", start);
    return 1;
}

static int allocate_handle(void) {
    for (int i = 0; i < MAX_HANDLES; i++) if (g_handles[i].kind == KIND_FREE) return i;
    return -1;
}

static handle_t *lookup(unsigned int fd) {
    if (fd == 0 || fd > MAX_HANDLES) return NULL;
    if (g_handles[fd - 1].kind == KIND_FREE) return NULL;
    return &g_handles[fd - 1];
}

static const char *open_mode(unsigned int flags, int exists) {
    int access = (int)(flags & 3u);
    if (flags & PS2LINK_OPEN_APPEND) return "ab";
    if (access == PS2LINK_OPEN_READ) return "rb";
    if (access == PS2LINK_OPEN_WRITE) return "wb";
    if (flags & PS2LINK_OPEN_TRUNCATE) return "w+b";
    return exists ? "r+b" : "w+b";
}

static int do_open(ps2link_socket_t sock, const unsigned char *payload, int size) {
    char path[1400];
    unsigned int flags;
    int slot, exists;
    struct stat info;
    if (size < 4 + PS2LINK_PATH_SIZE) return respond_result(sock, PS2LINK_RESPONSE_OPEN, -1);
    flags = get32(payload);
    if (strncmp((const char *)payload + 4, PS2LINK_PING_PATH, sizeof(PS2LINK_PING_PATH)) == 0) {
        slot = allocate_handle();
        if (slot < 0) return respond_result(sock, PS2LINK_RESPONSE_OPEN, -1);
        g_handles[slot].kind = KIND_PING;
        g_handles[slot].file = NULL;
        return respond_result(sock, PS2LINK_RESPONSE_OPEN, slot + 1);
    }
    if (!resolve_path(payload + 4, path, sizeof(path))) return respond_result(sock, PS2LINK_RESPONSE_OPEN, -1);
    exists = stat(path, &info) == 0 && !S_ISDIR(info.st_mode);
    if (!exists && !(flags & PS2LINK_OPEN_CREATE)) return respond_result(sock, PS2LINK_RESPONSE_OPEN, -1);
    slot = allocate_handle();
    if (slot < 0) return respond_result(sock, PS2LINK_RESPONSE_OPEN, -1);
    g_handles[slot].file = fopen(path, open_mode(flags, exists));
    if (g_handles[slot].file == NULL) return respond_result(sock, PS2LINK_RESPONSE_OPEN, -1);
    g_handles[slot].kind = KIND_FILE;
    return respond_result(sock, PS2LINK_RESPONSE_OPEN, slot + 1);
}

static int do_close(ps2link_socket_t sock, const unsigned char *payload, int size) {
    handle_t *handle = size >= 4 ? lookup(get32(payload)) : NULL;
    int result = -1;
    if (handle != NULL) {
        if (handle->kind == KIND_PING) g_ping_answered = 1;
        if (handle->file != NULL) fclose(handle->file);
        handle->file = NULL;
        handle->kind = KIND_FREE;
        result = 0;
    }
    return respond_result(sock, PS2LINK_RESPONSE_CLOSE, result);
}

static int do_read(ps2link_socket_t sock, const unsigned char *payload, int size) {
    handle_t *handle = size >= 8 ? lookup(get32(payload)) : NULL;
    unsigned char header[8];
    unsigned char *buffer;
    unsigned int wanted;
    size_t got;
    if (handle == NULL || handle->kind != KIND_FILE) {
        put32(header, (unsigned int)-1);
        put32(header + 4, (unsigned int)-1);
        return send_response(sock, PS2LINK_RESPONSE_READ, header, 8);
    }
    wanted = get32(payload + 4);
    if (wanted > (1u << 24)) wanted = 1u << 24;
    buffer = (unsigned char *)malloc(wanted ? wanted : 1);
    if (buffer == NULL) {
        put32(header, (unsigned int)-1);
        put32(header + 4, (unsigned int)-1);
        return send_response(sock, PS2LINK_RESPONSE_READ, header, 8);
    }
    got = fread(buffer, 1, wanted, handle->file);
    put32(header, (unsigned int)got);
    put32(header + 4, (unsigned int)got);
    if (!send_response(sock, PS2LINK_RESPONSE_READ, header, 8) ||
        (got > 0 && !ps2link_socket_send_all(sock, buffer, (int)got))) {
        free(buffer);
        return 0;
    }
    free(buffer);
    return 1;
}

static int do_write(ps2link_socket_t sock, const unsigned char *payload, int size) {
    handle_t *handle = size >= 8 ? lookup(get32(payload)) : NULL;
    unsigned int count = size >= 8 ? get32(payload + 4) : 0;
    unsigned char *buffer;
    int result = -1;
    if (count > (1u << 24)) return 0;
    buffer = (unsigned char *)malloc(count ? count : 1);
    if (buffer == NULL) return 0;
    if (count > 0 && !ps2link_socket_receive_all(sock, buffer, (int)count)) {
        free(buffer);
        return 0;
    }
    if (handle != NULL && handle->kind == KIND_PING) result = (int)count;
    else if (handle != NULL && handle->kind == KIND_FILE) result = (int)fwrite(buffer, 1, count, handle->file);
    free(buffer);
    return respond_result(sock, PS2LINK_RESPONSE_WRITE, result);
}

static int do_lseek(ps2link_socket_t sock, const unsigned char *payload, int size) {
    handle_t *handle = size >= 12 ? lookup(get32(payload)) : NULL;
    int result = -1;
    if (handle != NULL && handle->kind == KIND_FILE) {
        int whence = (int)get32(payload + 8);
        int origin = whence == 1 ? SEEK_CUR : (whence == 2 ? SEEK_END : SEEK_SET);
        if (fseek(handle->file, (long)(int)get32(payload + 4), origin) == 0) result = (int)ftell(handle->file);
    }
    return respond_result(sock, PS2LINK_RESPONSE_LSEEK, result);
}

static void fill_time(unsigned char *out, time_t value) {
    struct tm *local = localtime(&value);
    memset(out, 0, 8);
    if (local == NULL) return;
    out[6] = (unsigned char)local->tm_year;
    out[5] = (unsigned char)(local->tm_mon + 1);
    out[4] = (unsigned char)local->tm_mday;
    out[3] = (unsigned char)local->tm_hour;
    out[2] = (unsigned char)local->tm_min;
    out[1] = (unsigned char)local->tm_sec;
}

static int do_getstat(ps2link_socket_t sock, const unsigned char *payload, int size) {
    unsigned char body[4 + 4 + 4 + 4 + 24 + 4];
    char path[1400];
    struct stat info;
    unsigned int mode = 0;
    int result = -1;
    memset(body, 0, sizeof(body));
    if (size >= PS2LINK_PATH_SIZE && resolve_path(payload, path, sizeof(path)) && stat(path, &info) == 0) {
        result = 0;
        mode = (unsigned int)(info.st_mode & 0x07);
        if (S_ISDIR(info.st_mode)) mode |= PS2LINK_MODE_DIRECTORY;
        if (S_ISREG(info.st_mode)) mode |= PS2LINK_MODE_FILE;
        put32(body + 12, (unsigned int)info.st_size);
        fill_time(body + 16, info.st_ctime);
        fill_time(body + 24, info.st_atime);
        fill_time(body + 32, info.st_mtime);
    }
    put32(body, (unsigned int)result);
    put32(body + 4, mode);
    return send_response(sock, PS2LINK_RESPONSE_GETSTAT, body, (int)sizeof(body));
}

static int do_unsupported(ps2link_socket_t sock, unsigned int response) {
    if (response == PS2LINK_RESPONSE_READDIR) {
        unsigned char body[4 + 4 + 4 + 4 + 24 + 4 + PS2LINK_PATH_SIZE];
        memset(body, 0, sizeof(body));
        put32(body, (unsigned int)-1);
        return send_response(sock, response, body, (int)sizeof(body));
    }
    return respond_result(sock, response, -1);
}

int ps2link_host_files_handle(ps2link_socket_t sock, unsigned int number, const unsigned char *payload, int payload_size) {
    switch (number) {
    case PS2LINK_REQUEST_OPEN: return do_open(sock, payload, payload_size);
    case PS2LINK_REQUEST_CLOSE: return do_close(sock, payload, payload_size);
    case PS2LINK_REQUEST_READ: return do_read(sock, payload, payload_size);
    case PS2LINK_REQUEST_WRITE: return do_write(sock, payload, payload_size);
    case PS2LINK_REQUEST_LSEEK: return do_lseek(sock, payload, payload_size);
    case PS2LINK_REQUEST_GETSTAT: return do_getstat(sock, payload, payload_size);
    case PS2LINK_REQUEST_OPENDIR: return do_unsupported(sock, PS2LINK_RESPONSE_OPENDIR);
    case PS2LINK_REQUEST_CLOSEDIR: return do_unsupported(sock, PS2LINK_RESPONSE_CLOSEDIR);
    case PS2LINK_REQUEST_READDIR: return do_unsupported(sock, PS2LINK_RESPONSE_READDIR);
    case PS2LINK_REQUEST_REMOVE: return do_unsupported(sock, PS2LINK_RESPONSE_REMOVE);
    case PS2LINK_REQUEST_MKDIR: return do_unsupported(sock, PS2LINK_RESPONSE_MKDIR);
    case PS2LINK_REQUEST_RMDIR: return do_unsupported(sock, PS2LINK_RESPONSE_RMDIR);
    default: return 1;
    }
}
