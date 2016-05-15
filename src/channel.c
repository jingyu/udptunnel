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

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <sys/time.h>

#include "config.h"

#include "message.h"
#include "log.h"

#include "channel.h"

Channel *channel_create_server(Tunnel *t, uint16_t cid,
                               const char *host, const char *port,
                               const struct sockaddr *tunnelAddr,
                               socklen_t addrlen)
{
    Channel *ch = (Channel *)calloc(1, sizeof(Channel));
    if (!ch) {
        log_error("New channel(%d), out of memory.", cid);
        return NULL;
    }

    ch->tunnel = t;
    ch->id = cid;
    ch->state = CHANNEL_CONNECTING;
    ch->mode = CHANNEL_MODE_SERVER;

    ch->udp_sock = t->udp_svr_sock;
    ch->tcp_sock = INVALID_SOCKET;

    memcpy(&ch->tunnel_addr, tunnelAddr, addrlen);
    ch->tunnel_addr_Len = addrlen;

    ch->udp2tcp_state = CHANNEL_WAIT_DATA;
    ch->tcp2udp_state = CHANNEL_WAIT_DATA;

    gettimeofday(&ch->keepalive, NULL);
    ch->keepalive.tv_sec += CHANNEL_KEEPALIVE_TIMEOUT;

    /* store remote host & port in tcp2udp_data temporary */
    /* Format: host\0port\0 */
    strcpy(ch->tcp2udp_data, host);
    strcpy(ch->tcp2udp_data + strlen(host) + 1, port);

    return ch;
}

Channel *channel_create_client(Tunnel *t, SOCKET tcp_sock, uint16_t cid,
                               const struct sockaddr *tunnelAddr,
                               socklen_t addrlen)
{
    Channel *ch = (Channel *)calloc(1, sizeof(Channel));
    if (!ch) {
        log_error("New channel(%d), out of memory.", -cid);
        return NULL;
    }

    ch->tunnel = t;
    ch->id = cid;
    ch->state = CHANNEL_CONNECTING;
    ch->mode = CHANNEL_MODE_CLIENT;

    ch->udp_sock = t->udp_svr_sock;
    ch->tcp_sock = tcp_sock;

    memcpy(&ch->tunnel_addr, tunnelAddr, addrlen);
    ch->tunnel_addr_Len = addrlen;

    ch->udp2tcp_state = CHANNEL_WAIT_DATA;
    ch->tcp2udp_state = CHANNEL_WAIT_DATA;

    gettimeofday(&ch->keepalive, NULL);
    ch->keepalive.tv_sec += CHANNEL_KEEPALIVE_TIMEOUT;

    return ch;
}

int channel_connect(Channel *ch)
{
    char *host, *port;

    assert(ch->state == CHANNEL_CONNECTING);

    host = ch->tcp2udp_data;
    port = host + strlen(host) + 1;
    ch->tcp_sock = socket_connect(AF_INET, SOCK_STREAM, host, port);
    if (ch->tcp_sock == INVALID_SOCKET) {
        log_error("New channel(%d), connect to %s:%s error:%d.",
                  ch->id, host, port, socket_errno());
        return -1;
    }

    ch->state = CHANNEL_CONNECTED;

    ch->udp2tcp_state = CHANNEL_WAIT_DATA;
    ch->tcp2udp_state = CHANNEL_WAIT_DATA;

    /* Add peer socket to tunnel fdset */
    tunnel_sockets_set(ch->tunnel, ch->tcp_sock);

    log_info("Channel(%d) connected to %s:%s, opened.",
             ch->id, host, port);

    return 0;
}

/* For client side */
void channel_opened(Channel *ch, uint16_t new_id)
{
    assert(ch->state == CHANNEL_CONNECTING);

    /* Add TCP socket to tunnel fdset */
    tunnel_sockets_set(ch->tunnel, ch->tcp_sock);

    ch->id = new_id;

    ch->state = CHANNEL_CONNECTED;
    ch->tcp2udp_state = CHANNEL_WAIT_DATA;
    ch->udp2tcp_state = CHANNEL_WAIT_DATA;

    gettimeofday(&ch->keepalive, NULL);
    ch->keepalive.tv_sec += CHANNEL_KEEPALIVE_TIME;

    log_info("Channel(%d) for %s opened.", ch->id,
             socket_remote_name(ch->tcp_sock));
}

static inline uint16_t channel_next_sn(Channel *ch)
{
    if (++ch->sn == 0)
        ++ch->sn;

    return ch->sn;
}

int channel_socket_isset(Channel *ch, fd_set *fds)
{
    return FD_ISSET(ch->tcp_sock, fds);
}

void channel_mark_to_close(Channel *ch)
{
     ch->state = CHANNEL_CLOSE;
     log_debug("Channel(%d) requested to close.", ch->id);
}

static inline int channel_is_timeout(Channel *ch)
{
    struct timeval now;

    gettimeofday(&now, NULL);
    if (timercmp(&now, &ch->keepalive, >)) {
        return 1;
    } else
        return 0;
}

static inline int channel_send_message(Channel *ch, uint8_t type, uint16_t sn,
                                       void *data, size_t len)
{
    return message_send(ch->udp_sock, type, ch->id, sn, data, len,
                       (const struct sockaddr *)&ch->tunnel_addr,
                        ch->tunnel_addr_Len);
}

/* For client side: send & update keep-alive */
static int channel_send_keepalive(Channel *ch)
{
    int rc = 0;

    gettimeofday(&ch->keepalive, NULL);
    ch->keepalive.tv_sec += CHANNEL_KEEPALIVE_TIME;

    rc = channel_send_message(ch, MSG_CHANNEL_KEEPALIVE,
                              channel_next_sn(ch), NULL, 0);
    if (rc <= 0) {
        log_warning("Channel(%d) send keep-alive failed, retry later.", ch->id);
        return -1;
    } else {
        log_info("Channel(%d) send keep-alive.", ch->id);
    }

    return 0;
}

/* For server side: update keep-alive when received MSG_CHANNEL_KEEPALIVE. */
static void channel_update_keepalive(Channel *ch)
{
    gettimeofday(&ch->keepalive, NULL);
    ch->keepalive.tv_sec += CHANNEL_KEEPALIVE_TIME;

    log_info("Channel(%d) updated keep-alive.", ch->id);
}

/* Call after channel got UDP->TCP data, send it to TCP socket.
   Returns data length on success;  < 0 if error. */
static int channel_udp2tcp_data(Channel *ch, uint16_t sn,
                                const char *data, size_t len)
{
    int rc;
    int resend = 0;

    assert(ch->udp2tcp_state == CHANNEL_WAIT_DATA);

    if (ch->udp2tcp_state != CHANNEL_WAIT_DATA) {
        /* Ingnore the imcoming data. */
        log_error("Channel(%d) UDP->TCP data, wrong state.", ch->id);
        return -1;
    }

    if (ch->udp2tcp_sn != sn) {
        /* New data */
        ch->udp2tcp_sn = sn;
        log_debug("Channel(%d) UDP->TCP data(%d), %d bytes.", ch->id, sn, len);
    } else {
        /* Resend data, do not copy it again. */
        resend = 1;
        log_debug("Channel(%d) UDP->TCP data(%d, resend), %d bytes.", 
                  ch->id, sn, len);
    }

    /* Send ACK */
    rc = channel_send_message(ch, MSG_CHANNEL_DATA_ACK, sn, NULL, 0);
    if (rc <= 0) {
        log_error("Channel(%d) send UDP->TCP data(%d) ack to %s error:%d.", 
                  ch->id, sn, 
                  socket_addr_name((const struct sockaddr *)(&ch->tunnel_addr)),
                  socket_errno());
        return -1;
    } else {
        log_debug("Channel(%d) sent UDP->TCP data(%d) ack to %s.", ch->id, sn, 
                 socket_addr_name((const struct sockaddr *)(&ch->tunnel_addr)));
    }

    if (!resend) {
        /* Send data to TCP peer */
        int bytes_sent = 0;

        while ((size_t)bytes_sent < len) {
            rc = send(ch->tcp_sock, data + bytes_sent, len - bytes_sent, 0);
            if (rc <= 0) {
                log_error("Channel(%d) UDP->TCP data(%d) send to %s error:%d.", 
                          ch->id, sn, socket_remote_name(ch->tcp_sock),
                          socket_errno());
                return -1;

            }

            log_debug("Channel(%d) UDP->TCP data(%d) sent to %s, %d bytes.", 
                      ch->id, sn, socket_remote_name(ch->tcp_sock), rc);

            bytes_sent += rc;
        }
    }

    return len;
}

/* Call after got ACK for TCP->UDP data.
   Returns 0 is no erro, or <0 on error */
static int channel_tcp2udp_data_ack(Channel *ch, uint16_t sn)
{
    if (ch->tcp2udp_state != CHANNEL_WAIT_DATA_ACK) {
        /* Ignore */
        log_warning("Channel(%d) TCP->UDP data(%d) ack, wrong state. Ignored.",
                    ch->id, sn);
        return 0;
    }

    if (ch->tcp2udp_sn != sn) {
        /* Ignore */
        log_warning("Channel(%d) TCP->UDP data ack, "
                    "wrong sn(%d expected, %d reveived). Ignored.",
                    ch->id, ch->tcp2udp_sn, sn);
        return 0;
    }

    log_debug("Channel(%d) TCP->UDP data(%d) ack.", ch->id, sn);

    /* reset tcp2udp states */
    ch->tcp2udp_state = CHANNEL_WAIT_DATA;

    /* Add TCP socket to tunnel fdset again */
    tunnel_sockets_set(ch->tunnel, ch->tcp_sock);

    return 0;
}

/* Returns the number of bytes sent, 0 if no data to send or waiting for ACK, 
   -1 if error */
static int channel_send_tcp2udp_data(Channel *ch)
{
    int rc;

    if (ch->tcp2udp_data_len == 0)
        return 0;

    if (ch->tcp2udp_resent >= CHANNEL_DATA_MAX_RESEND) {
        log_error("Channel(%d) TCP->UDP data(%d) resend to %s, "
                 "too many retries.",
                 ch->id, ch->tcp2udp_sn, 
                 socket_addr_name((const struct sockaddr *)(&ch->tunnel_addr)));
        return -1;
    }

    rc = channel_send_message(ch, MSG_CHANNEL_DATA, ch->tcp2udp_sn, 
                              ch->tcp2udp_data, ch->tcp2udp_data_len);
    if (rc <= 0) {
        log_error("Channel(%d) TCP->UDP data(%d) %s to %s error:%d.", ch->id, 
                  ch->tcp2udp_sn, ch->tcp2udp_resent ? "resend" : "send",
                  socket_addr_name((const struct sockaddr *)(&ch->tunnel_addr)),
                  socket_errno());
        return -1;
    }

    log_debug("Channel(%d)  TCP->UDP data(%d) %s to %s.",
              ch->id, ch->tcp2udp_sn,
              ch->tcp2udp_resent ? "resent" : "sent",
              socket_addr_name((const struct sockaddr *)(&ch->tunnel_addr)));

    ch->tcp2udp_state = CHANNEL_WAIT_DATA_ACK;

    gettimeofday(&ch->tcp2udp_timeout, NULL);
    ch->tcp2udp_timeout.tv_sec += (ch->tcp2udp_resent+1)*CHANNEL_DATA_TIMEOUT;

    return rc;
}

/* Returns the number of bytes sent, 0 if no data to send or waiting for ACK, 
   -1 if error */
static int channel_check_and_resend_tcp2udp_data(Channel *ch)
{
    int rc = 0;
    struct timeval now;

    gettimeofday(&now, NULL);
    if((ch->tcp2udp_state == CHANNEL_WAIT_DATA_ACK) &&
            timercmp(&now, &ch->tcp2udp_timeout, >)) {
        ch->tcp2udp_resent++;
        rc = channel_send_tcp2udp_data(ch);
    }

    return rc;
}

/* Returns the number of bytes got, 0 if peer socket is closed. <0 if error */
int channel_tcp2udp_data(Channel *ch)
{
    int rc;

    assert(ch->tcp2udp_state == CHANNEL_WAIT_DATA);

    if (ch->tcp2udp_state != CHANNEL_WAIT_DATA) {
        log_error("Channel(%d) TCP->UDP data, wrong state.", ch->id);
        return -1;
    }

    rc = recv(ch->tcp_sock, ch->tcp2udp_data, sizeof(ch->tcp2udp_data), 0);
    if (rc == 0) {
        log_debug("Channel(%d) associated TCP socket closed.", ch->id);
        return 0;
    } else if (rc < 0) {
#if defined(_WIN32) || defined(_WIN64)
        if (WSAGetLastError() == WSAECONNRESET) {
            log_debug("Channel(%d) associated TCP socket reseted.", ch->id);
            return 0;
        }
#endif
        log_error("Channel(%d) associated TCP socket read error:%d.",
                  ch->id, socket_errno());
        return -1;
    }

    ch->tcp2udp_data_len = rc;
    ch->tcp2udp_sn = channel_next_sn(ch);
    ch->tcp2udp_resent = 0;

    log_debug("Channel(%d) TCP->UDP data(%d), %d bytes.", ch->id, 
              ch->tcp2udp_sn, ch->tcp2udp_data_len);

    /* Remove TCP socket from tunnel fdset. */
    tunnel_sockets_clear(ch->tunnel, ch->tcp_sock);

    return channel_send_tcp2udp_data(ch);
}

int channel_handle_message(Channel *ch, Message *msg)
{
    int rc = 0;

    assert(msg->channel_id == ch->id);

    switch (msg->type) {
    case MSG_CHANNEL_KEEPALIVE:
        /* For server side. */
        channel_update_keepalive(ch);
        break;

    case MSG_CHANNEL_DATA:
        rc = channel_udp2tcp_data(ch, msg->sn, msg->data, msg->length);
        break;

    case MSG_CHANNEL_DATA_ACK:
        rc = channel_tcp2udp_data_ack(ch, msg->sn);
        break;

    case MSG_CHANNEL_CLOSE:
        channel_mark_to_close(ch);
        rc = -1; /* Tunnel will close current channel. */
        break;
    }

    return rc;
}

int channel_idle(Channel *ch)
{
    if (channel_is_timeout(ch)) {
        if (ch->mode == CHANNEL_MODE_CLIENT)
            channel_send_keepalive(ch);
        else
            return -1; /* Tunnel will close current channel. */
    }

    return channel_check_and_resend_tcp2udp_data(ch);
}

void channel_close(Channel *ch)
{
    assert(ch);

    /* Request the peer to close. */
    if (ch->state != CHANNEL_CLOSE)
        channel_send_message(ch, MSG_CHANNEL_CLOSE, 
                             channel_next_sn(ch), NULL, 0);

    if (ch->tcp_sock != INVALID_SOCKET) {
        tunnel_sockets_clear(ch->tunnel, ch->tcp_sock);
        socket_close(ch->tcp_sock);
    }

    log_info("Channel(%d) closed.", ch->id);

    free(ch);
}
