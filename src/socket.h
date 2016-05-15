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

#ifndef __SOCKET_H__
#define __SOCKET_H__

#if !defined(_WIN32) && !defined(_WIN64)
#include <inttypes.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#else
#include <winsock2.h>
#include <ws2tcpip.h>
#endif /* WIN32 */

#ifdef __cplusplus
extern "C" {
#endif

/* for Winsock compatible */
#if !defined(_WIN32) && !defined(_WIN64)
#define SOCKET              int
#define INVALID_SOCKET      -1
#endif

SOCKET socket_create(int family, int type, const char *host, const char *port);

SOCKET socket_connect(int family, int type, const char *host, const char *port);

int socket_close(SOCKET s);

const char *socket_addr_name(const struct sockaddr *addr);

const char *socket_local_name(SOCKET sock);

const char *socket_remote_name(SOCKET sock);

int socket_errno();

#ifdef __cplusplus
}
#endif

#endif /* __SOCKET_H__ */
