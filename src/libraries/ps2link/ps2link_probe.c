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
 * Licensed under the MIT License.
 * See the LICENSE file for details.
 */

#include <stdio.h>
#include <string.h>
#include <SDL.h>
#include "libraries/ps2link/ps2link_probe.h"
#include "libraries/ps2link/ps2link_protocol.h"
#include "libraries/ps2link/ps2link_socket.h"

static SDL_Thread *g_thread = NULL;
static volatile int g_state = PS2LINK_PROBE_IDLE;
static char g_host[128];

static int probe_thread(void *unused) {
    (void)unused;
    int ok = 0;
    if (ps2link_socket_startup()) {
        ps2link_socket_t s = ps2link_socket_connect_tcp(g_host, PS2LINK_PORT_REQUEST, 2500);
        if (s != PS2LINK_INVALID_SOCKET) { ok = 1; ps2link_socket_close(s); }
    }
    g_state = ok ? PS2LINK_PROBE_REACHABLE : PS2LINK_PROBE_UNREACHABLE;
    return 0;
}

void ps2link_probe_reset(void) {
    if (g_state == PS2LINK_PROBE_CHECKING) return;
    if (g_thread) { SDL_WaitThread(g_thread, NULL); g_thread = NULL; }
    g_state = PS2LINK_PROBE_IDLE;
}

int ps2link_probe_start(const char *host) {
    if (g_state == PS2LINK_PROBE_CHECKING) return 0;
    if (g_thread) { SDL_WaitThread(g_thread, NULL); g_thread = NULL; }
    snprintf(g_host, sizeof(g_host), "%s", host);
    g_state = PS2LINK_PROBE_CHECKING;
    g_thread = SDL_CreateThread(probe_thread, "ps2link_probe", NULL);
    if (!g_thread) { g_state = PS2LINK_PROBE_UNREACHABLE; return 0; }
    return 1;
}

ps2link_probe_state_t ps2link_probe_state(void) { return (ps2link_probe_state_t)g_state; }
