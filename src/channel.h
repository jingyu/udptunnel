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

#ifndef __CHANNEL_H__
#define __CHANNEL_H__

#include <stdint.h>

#include "socket.h"
#include "message.h"
#include "tunnel_i.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Channel data/ack timeout */
#define CHANNEL_DATA_TIMEOUT                1 /* seconds */

/* Channel data max resend */
#define CHANNEL_DATA_MAX_RESEND             10

/* Channel keep-alive time */
#define CHANNEL_KEEPALIVE_TIME              60 /* seconds */

/* Channel keep-alive max retry */
#define CHANNEL_KEEPALIVE_RETRY             5

/* Channel keep-alive timeout */
#define CHANNEL_KEEPALIVE_TIMEOUT           (CHANNEL_KEEPALIVE_RETRY * \
                                             CHANNEL_KEEPALIVE_TIME + 1)

/* Channel state */
#define CHANNEL_CONNECTING                  1
#define CHANNEL_CONNECTED                   2
#define CHANNEL_WAIT_DATA                   3
#define CHANNEL_WAIT_DATA_ACK               4
#define CHANNEL_CLOSE                       255

#define CHANNEL_MODE_CLIENT                 0
#define CHANNEL_MODE_SERVER                 1

/* Channel perform flags */
#define CHANNEL_IDLE                        0
#define CHANNEL_MESSAGE                     1
#define CHANNEL_TCP_ACTIVE                  2

typedef struct channel {
    Tunnel *tunnel;

    uint16_t id;
    int state;
    int mode;

    uint16_t sn;

    /* Keep-alive */
    struct timeval keepalive;

    /* UDP side */
    SOCKET udp_sock;
    SOCKET tcp_sock;

    /* Tunnel address */
    struct sockaddr_storage tunnel_addr;
    socklen_t tunnel_addr_Len;

    /* Transmit fields */
    int udp2tcp_state;
    uint16_t udp2tcp_sn;

    int tcp2udp_state;
    uint16_t tcp2udp_sn;
    struct timeval tcp2udp_timeout;
    int tcp2udp_resent;
    int tcp2udp_data_len;
    char tcp2udp_data[TUNNEL_MAX_DATA_LEN];
} Channel;

Channel *channel_create_server(Tunnel *t, uint16_t cid,
                               const char *host, const char *port,
                               const struct sockaddr *tunnelAddr,
                               socklen_t addrlen);
                               
Channel *channel_create_client(Tunnel *t, SOCKET tcp_sock, uint16_t cid,
                               const struct sockaddr *tunnelAddr,
                               socklen_t addrlen);

void channel_mark_to_close(Channel *ch);

void channel_close(Channel *ch);

int channel_connect(Channel *ch);

void channel_opened(Channel *ch, uint16_t new_id);

int channel_tcp2udp_data(Channel *ch);

int channel_handle_message(Channel *ch, Message *msg);

int channel_idle(Channel *ch);

int channel_socket_isset(Channel *ch, fd_set *fds);

#ifdef __cplusplus
}
#endif

#endif /* __CHANNEL_H__ */
