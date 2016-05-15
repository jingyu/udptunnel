/*
 * udptunnel : Lightweight TCP over UDP Tunneling
 *
 * Copyright (C) 2014 Jingyu jingyu.niu@gmail.com
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *       http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdio.h>
#include <string.h>
#if !defined(_WIN32) && !defined(_WIN64)
#include <unistd.h>
#include <errno.h>
#endif

#include "config.h"

#include "socket.h"

SOCKET socket_create(int family, int type, const char *host, const char *port)
{
    SOCKET sock;
    struct addrinfo hints;
    struct addrinfo *ai;
    struct addrinfo *p;
    int rc;

    if (!host && !port) {
        sock = socket(family, type, 0);
        return sock;
    }

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = family;
    hints.ai_socktype = type;
    if (type == SOCK_STREAM)
        hints.ai_protocol = IPPROTO_TCP;
    else if (type == SOCK_DGRAM)
        hints.ai_protocol = IPPROTO_UDP;
    hints.ai_flags = AI_PASSIVE;

    rc = getaddrinfo(host, port, &hints, &ai);
    if (rc != 0)
        return INVALID_SOCKET;

    for (p = ai; p; p = p->ai_next) {
        sock = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (sock == INVALID_SOCKET)
            continue;

        if (bind(sock, p->ai_addr, p->ai_addrlen) != 0) {
            socket_close(sock);
            sock = INVALID_SOCKET;
            continue;
        }

        break;
    }

    freeaddrinfo(ai);
    return sock;
}

SOCKET socket_connect(int family, int type, const char *host, const char *port)
{
    SOCKET sock;
    struct addrinfo hints;
    struct addrinfo *ai;
    struct addrinfo *p;
    int rc;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = family;
    hints.ai_socktype = type;
    if (type == SOCK_STREAM)
        hints.ai_protocol = IPPROTO_TCP;
    else if (type == SOCK_DGRAM)
        hints.ai_protocol = IPPROTO_UDP;

    rc = getaddrinfo(host, port, &hints, &ai);
    if (rc != 0)
        return INVALID_SOCKET;

    for (p = ai; p; p = p->ai_next) {
        sock = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (sock == INVALID_SOCKET) {
            continue;
        }

        if (p->ai_socktype == SOCK_STREAM) {
            if (connect(sock, p->ai_addr, p->ai_addrlen) != 0) {
                socket_close(sock);
                sock = INVALID_SOCKET;
                continue;
            }
        }

        break;
    }

    freeaddrinfo(ai);
    return sock;
}

int socket_close(SOCKET s)
{
#if !defined(_WIN32) && !defined(_WIN64)
    return close(s);
#else
    return closesocket(s);
#endif
}

/* NOTICE: Not reentrant function. */
const char *socket_addr_name(const struct sockaddr *addr)
{
    static char result[128];
    int port;
    int len;

    if (!addr)
        return NULL;

    inet_ntop(addr->sa_family, &((struct sockaddr_in *)addr)->sin_addr,
              result, sizeof(result));
    len = strlen(result);

    port = ntohs(((struct sockaddr_in *)addr)->sin_port);
    sprintf(result + len, ":%d", port);

    return result;
}

const char *socket_local_name(SOCKET sock)
{
    struct sockaddr_storage addr;
    socklen_t addrlen = sizeof(addr);

    getsockname(sock, (struct sockaddr *)&addr, &addrlen);
    return socket_addr_name((const struct sockaddr *)&addr);
}

const char *socket_remote_name(SOCKET sock)
{
    struct sockaddr_storage addr;
    socklen_t addrlen = sizeof(addr);

    getpeername(sock, (struct sockaddr *)&addr, &addrlen);
    return socket_addr_name((const struct sockaddr *)&addr);
}

int socket_errno()
{
#if defined(_WIN32) || defined(_WIN64)
    return WSAGetLastError();
#else
    return errno;
#endif
}
