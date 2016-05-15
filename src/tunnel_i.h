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

#ifndef __TUNNEL_I_H__
#define __TUNNEL_I_H__

#include <stdint.h>

#if !defined(_WIN32) && !defined(_WIN64)
#include <inttypes.h>
#include <sys/time.h>
#endif /* !_WIN32 && !_WIN64 */

#include "socket.h"
#include "hashtable.h"

#include "tunnel.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Max tunnel message payload length */
#define TUNNEL_MAX_DATA_LEN                 1024

/* TCP server backlog on tunnel client side */
#define TUNNEL_SERVER_BACKLOG               16

#define TUNNEL_HELLO_MEX_RETRY              5

#define TUNNEL_MODE_CLIENT                  0
#define TUNNEL_MODE_SERVER                  1

typedef struct tunnel {
    int mode;

    SOCKET udp_svr_sock;
    SOCKET tcp_svr_sock;                        /* For client side only */

    fd_set fds;
    int nfds;

    uint16_t sn;
    uint16_t cid;

    int stop;

    Hashtable *channels;

    char remote_host[TUNNEL_MAX_HOST_LEN+1];    /* For client side only */
    char remote_port[TUNNEL_MAX_PORT_LEN+1];    /* For client side only */

    struct sockaddr_storage tunnel_addr;        /* For client side only */
    socklen_t tunnel_addr_len;                  /* For client side only */
} Tunnel;

void tunnel_sockets_set(Tunnel *t, SOCKET sock);

void tunnel_sockets_clear(Tunnel *t, SOCKET sock);

#ifdef __cplusplus
}
#endif

#endif /* __TUNNEL_I_H__ */
