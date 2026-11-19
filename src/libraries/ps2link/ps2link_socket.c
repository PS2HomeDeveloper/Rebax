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

#include "libraries/ps2link/ps2link_socket.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

#ifndef _WIN32
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <signal.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#endif

static int set_blocking(ps2link_socket_t sock, int blocking) {
#ifdef _WIN32
    u_long mode = blocking ? 0 : 1;
    return ioctlsocket(sock, FIONBIO, &mode) == 0 ? 0 : -1;
#else
    int flags = fcntl(sock, F_GETFL, 0);
    if (flags < 0) return -1;
    if (blocking) flags &= ~O_NONBLOCK;
    else flags |= O_NONBLOCK;
    return fcntl(sock, F_SETFL, flags);
#endif
}

static int resolve(const char *host, int port, struct sockaddr_in *out) {
    struct addrinfo hints, *result = NULL;
    char service[16];
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    snprintf(service, sizeof(service), "%d", port);
    if (getaddrinfo(host, service, &hints, &result) != 0 || result == NULL) return 0;
    memcpy(out, result->ai_addr, sizeof(*out));
    freeaddrinfo(result);
    return 1;
}

int ps2link_socket_startup(void) {
#ifdef _WIN32
    WSADATA data;
    return WSAStartup(MAKEWORD(2, 2), &data) == 0 ? 1 : 0;
#else
    signal(SIGPIPE, SIG_IGN);
    return 1;
#endif
}

ps2link_socket_t ps2link_socket_listen_udp(int port) {
    struct sockaddr_in address;
    ps2link_socket_t sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock == PS2LINK_INVALID_SOCKET) return PS2LINK_INVALID_SOCKET;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons((unsigned short)port);
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(sock, (struct sockaddr *)&address, sizeof(address)) != 0) {
        ps2link_socket_close(sock);
        return PS2LINK_INVALID_SOCKET;
    }
    return sock;
}

ps2link_socket_t ps2link_socket_connect_udp(const char *host, int port) {
    struct sockaddr_in address;
    ps2link_socket_t sock;
    if (!resolve(host, port, &address)) return PS2LINK_INVALID_SOCKET;
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock == PS2LINK_INVALID_SOCKET) return PS2LINK_INVALID_SOCKET;
    if (connect(sock, (struct sockaddr *)&address, sizeof(address)) != 0) {
        ps2link_socket_close(sock);
        return PS2LINK_INVALID_SOCKET;
    }
    return sock;
}

ps2link_socket_t ps2link_socket_connect_tcp(const char *host, int port, int timeout_ms) {
    struct sockaddr_in address;
    ps2link_socket_t sock;
    int nodelay = 1;
    if (!resolve(host, port, &address)) return PS2LINK_INVALID_SOCKET;
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == PS2LINK_INVALID_SOCKET) return PS2LINK_INVALID_SOCKET;
    setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (const char *)&nodelay, sizeof(nodelay));
    if (set_blocking(sock, 0) != 0) {
        ps2link_socket_close(sock);
        return PS2LINK_INVALID_SOCKET;
    }
    if (connect(sock, (struct sockaddr *)&address, sizeof(address)) != 0) {
        fd_set write_set, error_set;
        struct timeval timeout;
        int error = 0;
        socklen_t length = sizeof(error);
#ifdef _WIN32
        int pending = WSAGetLastError() == WSAEWOULDBLOCK;
#else
        int pending = errno == EINPROGRESS;
#endif
        if (!pending) {
            ps2link_socket_close(sock);
            return PS2LINK_INVALID_SOCKET;
        }
        FD_ZERO(&write_set);
        FD_ZERO(&error_set);
        FD_SET(sock, &write_set);
        FD_SET(sock, &error_set);
        timeout.tv_sec = timeout_ms / 1000;
        timeout.tv_usec = (timeout_ms % 1000) * 1000;
        if (select((int)sock + 1, NULL, &write_set, &error_set, &timeout) <= 0 ||
            getsockopt(sock, SOL_SOCKET, SO_ERROR, (char *)&error, &length) != 0 || error != 0) {
            ps2link_socket_close(sock);
            return PS2LINK_INVALID_SOCKET;
        }
    }
    if (set_blocking(sock, 1) != 0) {
        ps2link_socket_close(sock);
        return PS2LINK_INVALID_SOCKET;
    }
    return sock;
}

int ps2link_socket_send_all(ps2link_socket_t sock, const void *data, int size) {
    int total = 0;
    while (total < size) {
        int sent = (int)send(sock, (const char *)data + total, size - total, 0);
        if (sent <= 0) {
#ifndef _WIN32
            if (sent < 0 && errno == EINTR) continue;
#endif
            return 0;
        }
        total += sent;
    }
    return 1;
}

int ps2link_socket_receive_all(ps2link_socket_t sock, void *data, int size) {
    int total = 0;
    while (total < size) {
        int received = (int)recv(sock, (char *)data + total, size - total, 0);
        if (received <= 0) {
#ifndef _WIN32
            if (received < 0 && errno == EINTR) continue;
#endif
            return 0;
        }
        total += received;
    }
    return 1;
}

int ps2link_socket_receive_some(ps2link_socket_t sock, void *data, int size) {
    return (int)recv(sock, (char *)data, size, 0);
}

int ps2link_socket_wait_any(ps2link_socket_t first, ps2link_socket_t second, int timeout_ms, int *first_ready, int *second_ready) {
    fd_set read_set;
    struct timeval timeout;
    int highest = 0, result;
    FD_ZERO(&read_set);
    if (first != PS2LINK_INVALID_SOCKET) { FD_SET(first, &read_set); if ((int)first > highest) highest = (int)first; }
    if (second != PS2LINK_INVALID_SOCKET) { FD_SET(second, &read_set); if ((int)second > highest) highest = (int)second; }
    timeout.tv_sec = timeout_ms / 1000;
    timeout.tv_usec = (timeout_ms % 1000) * 1000;
    result = select(highest + 1, &read_set, NULL, NULL, &timeout);
    *first_ready = result > 0 && first != PS2LINK_INVALID_SOCKET && FD_ISSET(first, &read_set);
    *second_ready = result > 0 && second != PS2LINK_INVALID_SOCKET && FD_ISSET(second, &read_set);
    return result;
}

void ps2link_socket_close(ps2link_socket_t sock) {
    if (sock == PS2LINK_INVALID_SOCKET) return;
#ifdef _WIN32
    closesocket(sock);
#else
    close(sock);
#endif
}
