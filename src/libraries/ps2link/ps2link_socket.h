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

#ifndef PS2LINK_SOCKET_H
#define PS2LINK_SOCKET_H

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
typedef SOCKET ps2link_socket_t;
#define PS2LINK_INVALID_SOCKET INVALID_SOCKET
#else
typedef int ps2link_socket_t;
#define PS2LINK_INVALID_SOCKET (-1)
#endif

int ps2link_socket_startup(void);
ps2link_socket_t ps2link_socket_listen_udp(int port);
ps2link_socket_t ps2link_socket_connect_tcp(const char *host, int port, int timeout_ms);
ps2link_socket_t ps2link_socket_connect_udp(const char *host, int port);
int ps2link_socket_send_all(ps2link_socket_t sock, const void *data, int size);
int ps2link_socket_receive_all(ps2link_socket_t sock, void *data, int size);
int ps2link_socket_receive_some(ps2link_socket_t sock, void *data, int size);
int ps2link_socket_wait_any(ps2link_socket_t first, ps2link_socket_t second, int timeout_ms, int *first_ready, int *second_ready);
void ps2link_socket_close(ps2link_socket_t sock);

#endif
