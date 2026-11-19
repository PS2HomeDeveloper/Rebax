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
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "libraries/ps2link/ps2link_client.h"

#include <SDL.h>
#include <stdio.h>
#include <string.h>

#include "libraries/ps2link/ps2link_host_files.h"
#include "libraries/ps2link/ps2link_protocol.h"
#include "libraries/ps2link/ps2link_socket.h"

#define LINE_CAP 512
#define LINE_QUEUE 256
#define CONNECT_TIMEOUT_MS 4000
#define HANDSHAKE_TIMEOUT_MS 10000

static SDL_Thread *g_thread;
static SDL_mutex *g_lock;
static volatile int g_stop;
static volatile ps2link_state_t g_state = PS2LINK_STATE_IDLE;
static char g_error[256];
static char g_host[128];
static char g_root[1024];
static char g_elf[256];

static char g_lines[LINE_QUEUE][LINE_CAP];
static int g_line_head, g_line_count;
static char g_line_out[LINE_CAP];

static ps2link_socket_t g_console = PS2LINK_INVALID_SOCKET;
static ps2link_socket_t g_request = PS2LINK_INVALID_SOCKET;
static ps2link_socket_t g_command = PS2LINK_INVALID_SOCKET;

static void put32(unsigned char *p, unsigned int v) {
    p[0] = (unsigned char)(v >> 24);
    p[1] = (unsigned char)(v >> 16);
    p[2] = (unsigned char)(v >> 8);
    p[3] = (unsigned char)v;
}

static unsigned int get32(const unsigned char *p) {
    return ((unsigned int)p[0] << 24) | ((unsigned int)p[1] << 16) | ((unsigned int)p[2] << 8) | (unsigned int)p[3];
}

static void push_line(const char *text) {
    int slot;
    SDL_LockMutex(g_lock);
    if (g_line_count == LINE_QUEUE) {
        g_line_head = (g_line_head + 1) % LINE_QUEUE;
        g_line_count--;
    }
    slot = (g_line_head + g_line_count) % LINE_QUEUE;
    snprintf(g_lines[slot], LINE_CAP, "%s", text);
    g_line_count++;
    SDL_UnlockMutex(g_lock);
}

static void fail(const char *message) {
    snprintf(g_error, sizeof(g_error), "%s", message);
    g_state = PS2LINK_STATE_FAILED;
}

static int send_command(unsigned int number, const unsigned char *body, int body_size) {
    unsigned char packet[PS2LINK_HEADER_SIZE + 4 + PS2LINK_PATH_SIZE];
    int size = PS2LINK_HEADER_SIZE + body_size;
    if (body_size > 4 + PS2LINK_PATH_SIZE) return 0;
    put32(packet, number);
    packet[4] = (unsigned char)(size >> 8);
    packet[5] = (unsigned char)size;
    if (body_size > 0) memcpy(packet + PS2LINK_HEADER_SIZE, body, (size_t)body_size);
    return ps2link_socket_send_all(g_command, packet, size);
}

static int send_ping(void) {
    unsigned char body[4 + PS2LINK_PATH_SIZE];
    memset(body, 0, sizeof(body));
    put32(body, PS2LINK_PING_REGISTERS);
    snprintf((char *)body + 4, PS2LINK_PATH_SIZE, "%s", PS2LINK_PING_PATH);
    return send_command(PS2LINK_COMMAND_DUMPREG, body, (int)sizeof(body));
}

static int send_execee(const char *path) {
    unsigned char body[4 + 256];
    size_t length = strlen(path);
    if (length + 1 > 256) return 0;
    memset(body, 0, sizeof(body));
    put32(body, 1);
    memcpy(body + 4, path, length + 1);
    return send_command(PS2LINK_COMMAND_EXECEE, body, (int)sizeof(body));
}

static int send_reset(void) {
    return send_command(PS2LINK_COMMAND_RESET, NULL, 0);
}

static void console_data(const unsigned char *data, int size, char *partial, int *partial_size) {
    for (int i = 0; i < size; i++) {
        char c = (char)data[i];
        if (c == '\r' || c == '\0') continue;
        if (c == '\n' || *partial_size == LINE_CAP - 1) {
            partial[*partial_size] = '\0';
            if (*partial_size > 0) push_line(partial);
            *partial_size = 0;
            if (c == '\n') continue;
        }
        partial[(*partial_size)++] = c;
    }
}

static int service_request(void) {
    unsigned char header[PS2LINK_HEADER_SIZE];
    unsigned char payload[PS2LINK_MAX_REQUEST_PAYLOAD];
    int length;
    if (!ps2link_socket_receive_all(g_request, header, PS2LINK_HEADER_SIZE)) return 0;
    length = (header[4] << 8) | header[5];
    if (length < PS2LINK_HEADER_SIZE || length - PS2LINK_HEADER_SIZE > PS2LINK_MAX_REQUEST_PAYLOAD) return 0;
    if (length > PS2LINK_HEADER_SIZE && !ps2link_socket_receive_all(g_request, payload, length - PS2LINK_HEADER_SIZE)) return 0;
    return ps2link_host_files_handle(g_request, get32(header), payload, length - PS2LINK_HEADER_SIZE);
}

static int pump(int timeout_ms, char *partial, int *partial_size) {
    static unsigned char buffer[65536];
    int console_ready = 0, request_ready = 0;
    ps2link_socket_wait_any(g_console, g_request, timeout_ms, &console_ready, &request_ready);
    if (console_ready) {
        int size = ps2link_socket_receive_some(g_console, buffer, (int)sizeof(buffer));
        if (size > 0) console_data(buffer, size, partial, partial_size);
    }
    if (request_ready && !service_request()) return 0;
    return 1;
}

static void close_all(void) {
    ps2link_socket_close(g_console);
    ps2link_socket_close(g_request);
    ps2link_socket_close(g_command);
    g_console = g_request = g_command = PS2LINK_INVALID_SOCKET;
    ps2link_host_files_close_all();
}

static int worker(void *unused) {
    char partial[LINE_CAP];
    int partial_size = 0;
    Uint32 start;
    Uint32 next_ping = 0;
    (void)unused;

    if (!ps2link_socket_startup()) { fail("Could not start the network."); return 0; }
    g_console = ps2link_socket_listen_udp(PS2LINK_PORT_CONSOLE);
    if (g_console == PS2LINK_INVALID_SOCKET) { fail("The console port (UDP 0x4712) is busy. Close any other ps2link client."); return 0; }
    g_request = ps2link_socket_connect_tcp(g_host, PS2LINK_PORT_REQUEST, CONNECT_TIMEOUT_MS);
    if (g_request == PS2LINK_INVALID_SOCKET) { close_all(); fail("Could not reach ps2link on the PS2. Check the network cable and the IP address."); return 0; }
    g_command = ps2link_socket_connect_udp(g_host, PS2LINK_PORT_COMMAND);
    if (g_command == PS2LINK_INVALID_SOCKET) { close_all(); fail("Could not open the ps2link command port."); return 0; }

    ps2link_host_files_reset(g_root);
    start = SDL_GetTicks();
    while (!ps2link_host_files_ping_answered()) {
        Uint32 now = SDL_GetTicks();
        if (g_stop) { close_all(); g_state = PS2LINK_STATE_ENDED; return 0; }
        if (now - start > HANDSHAKE_TIMEOUT_MS) { close_all(); fail("ps2link accepted the connection but does not answer."); return 0; }
        if (now >= next_ping) { send_ping(); next_ping = now + 1000; }
        if (!pump(50, partial, &partial_size)) { close_all(); fail("ps2link closed the connection."); return 0; }
    }

    {
        char path[300];
        snprintf(path, sizeof(path), "host:%s", g_elf);
        if (!send_execee(path)) { close_all(); fail("The program name is too long for ps2link."); return 0; }
    }
    g_state = PS2LINK_STATE_RUNNING;

    while (!g_stop) {
        if (!pump(100, partial, &partial_size)) {
            close_all();
            g_state = PS2LINK_STATE_ENDED;
            return 0;
        }
    }
    send_reset();
    close_all();
    g_state = PS2LINK_STATE_ENDED;
    return 0;
}

int ps2link_start(const char *host, const char *root_dir, const char *elf_name) {
    if (g_lock == NULL) g_lock = SDL_CreateMutex();
    if (g_thread != NULL) {
        if (g_state == PS2LINK_STATE_CONNECTING || g_state == PS2LINK_STATE_RUNNING) return 0;
        SDL_WaitThread(g_thread, NULL);
        g_thread = NULL;
    }
    snprintf(g_host, sizeof(g_host), "%s", host);
    snprintf(g_root, sizeof(g_root), "%s", root_dir);
    snprintf(g_elf, sizeof(g_elf), "%s", elf_name);
    g_error[0] = '\0';
    g_stop = 0;
    g_line_head = g_line_count = 0;
    g_state = PS2LINK_STATE_CONNECTING;
    g_thread = SDL_CreateThread(worker, "rebax-ps2link", NULL);
    if (g_thread == NULL) {
        fail("Could not start the connection thread.");
        return 0;
    }
    return 1;
}

void ps2link_stop(void) {
    if (g_thread == NULL) return;
    g_stop = 1;
    SDL_WaitThread(g_thread, NULL);
    g_thread = NULL;
}

ps2link_state_t ps2link_get_state(void) {
    return g_state;
}

const char *ps2link_get_error(void) {
    return g_error;
}

const char *ps2link_poll_line(void) {
    const char *result = NULL;
    if (g_lock == NULL) return NULL;
    SDL_LockMutex(g_lock);
    if (g_line_count > 0) {
        snprintf(g_line_out, sizeof(g_line_out), "%s", g_lines[g_line_head]);
        g_line_head = (g_line_head + 1) % LINE_QUEUE;
        g_line_count--;
        result = g_line_out;
    }
    SDL_UnlockMutex(g_lock);
    return result;
}
