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
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/time.h>

#include "config.h"

#include "channel.h"
#include "message.h"
#include "log.h"

#include "tunnel_i.h"
#include "tunnel.h"

#define TUNNEL_DEFAULT_PROFILE          "UDPTunnel/1.2"

static int tunnel_say_hello(Tunnel *t, const char *host, const char *port)
{
    int rc;
    int retry;

    struct sockaddr_storage fromaddr;
    socklen_t addrlen;

    uint16_t sn;

    struct addrinfo hints;
    struct addrinfo *ai;
    struct addrinfo *p = NULL;

    struct timeval timeout;

    fd_set fds;

    char msg_buf[sizeof(Message) + TUNNEL_MAX_DATA_LEN - 1];
    Message *msg = (Message *)msg_buf;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP;

    rc = getaddrinfo(host, port,  &hints, &ai);
    if (rc != 0) {
        return -1;
    }

    for (retry = 0; retry < TUNNEL_HELLO_MEX_RETRY && !p; retry++) {
        for (p = ai; p; p = p->ai_next) {
            log_info("Hello to %s.", socket_addr_name(p->ai_addr));

            sn = t->sn++;

            rc = message_send(t->udp_svr_sock, MSG_TUNNEL_HELLO, 0, sn, 
                              TUNNEL_DEFAULT_PROFILE, 
                              strlen(TUNNEL_DEFAULT_PROFILE) + 1, 
                              p->ai_addr, p->ai_addrlen);
            if (rc <= 0) {
                log_warning("Send hello to %s error:%d.", 
                            socket_addr_name(p->ai_addr), socket_errno());
                continue;
            }

            timerclear(&timeout);
            timeout.tv_sec = 1;
            FD_ZERO(&fds);
            FD_SET(t->udp_svr_sock, &fds);

            rc = select(FD_SETSIZE, &fds, NULL, NULL, &timeout);
            if (rc <= 0) { // Timeout or error
                if (errno != EINTR)
                    log_warning("Receive hello ack from %s timeout.",
                                socket_addr_name(p->ai_addr));
                continue;
            }

            addrlen = sizeof(fromaddr);
            msg->length = TUNNEL_MAX_DATA_LEN;
            rc = message_receive(t->udp_svr_sock, msg, 
                                 (struct sockaddr *)&fromaddr, &addrlen);
            if (rc <= 1) {
                log_warning("Receive hello ack from %s error:%d.", 
                           socket_addr_name((const struct sockaddr *)&fromaddr),
                           socket_errno());
                continue;
            }

            if (msg->type != MSG_TUNNEL_HELLO_ACK ||
                msg->sn != sn || addrlen != p->ai_addrlen ||
                memcmp(p->ai_addr, &fromaddr, addrlen) != 0) {
                log_warning("Receive unexpected message from %s, ignore.", 
                          socket_addr_name((const struct sockaddr *)&fromaddr));
                continue;
            }

            memcpy(&t->tunnel_addr, p->ai_addr, p->ai_addrlen);
            t->tunnel_addr_len = p->ai_addrlen;
            goto done;
        }
    }

done:
    freeaddrinfo(ai);

    return p == NULL ? -1 : 0;
}

static int tunnel_hello_ack(Tunnel *t, uint16_t sn, void *data, size_t datalen,
                            const struct sockaddr *from, socklen_t fromlen)
{
    int rc;
    char *profile = (char *)data;
    size_t len;

    /* Data format is: profile\0 */

    if ((len = strnlen(profile, datalen)) == datalen) {
        /* no null terminal */
        log_warning("Hello from %s denied, invalid request.",
                    socket_addr_name(from));
        return -1;
    }

    if (len > TUNNEL_MAX_PROFILE_LEN) {
        log_warning("Hello from %s denied, invalid profile.",
                    socket_addr_name(from));
        return -1;
    }

    if (strcmp(profile, TUNNEL_DEFAULT_PROFILE) != 0) {
        log_warning("Hello from %s denied, unknown profile.",
                    socket_addr_name(from));
        return -1;
    }

    rc = message_send(t->udp_svr_sock, MSG_TUNNEL_HELLO_ACK, 0, sn, 
                      NULL, 0, from, fromlen);
    if (rc <= 0) {
        log_error("Hello from %s, send ack error:%d.",
                  socket_addr_name(from), socket_errno());
        return -1;
    }

    log_info("Hello from %s, allowed.", socket_addr_name(from));

    return 0;
}

Tunnel *tunnel_create_server(const char *host, const char *port)
{
    if (port == NULL || *port == 0)
        return NULL;

    Tunnel *t = (Tunnel *)calloc(1, sizeof(Tunnel));
    if (!t) {
        log_error("Create tunnel, out of memory.");
        return NULL;
    }
    
    t->udp_svr_sock = socket_create(AF_INET, SOCK_DGRAM, host, port);
    if (t->udp_svr_sock == INVALID_SOCKET) {
        log_error("Create socket and bind to %s:%s error:%d.",
                  host ? host : "0.0.0.0", port, socket_errno());
        free(t);
        return NULL;
    }

    t->channels = hashtable_create(512, 0.8f);
    if (!t->channels) {
        log_error("Create tunnel, out of memory.");
        socket_close(t->udp_svr_sock);
        free(t);
        return NULL;
    }

    t->mode = TUNNEL_MODE_SERVER;
    t->tcp_svr_sock = INVALID_SOCKET;

    FD_ZERO(&t->fds);
    FD_SET(t->udp_svr_sock, &t->fds);

    log_info("Tunnel server start on %s.", socket_local_name(t->udp_svr_sock));

    return t;
}

Tunnel *tunnel_create_client(const char *host, const char *port,
                             const char *tunnel_host, char *tunnel_port,
                             const char *remote_host, char *remote_port)
{
    if (port == NULL || *port == 0 ||
        tunnel_host == NULL || *tunnel_host == 0 ||
        tunnel_port == NULL || *tunnel_port == 0 ||
        remote_host == NULL || *remote_host == 0 ||
        remote_port == NULL || *remote_port == 0)
        return NULL;

    if (strlen(remote_host) > TUNNEL_MAX_HOST_LEN || 
        strlen(remote_port) > TUNNEL_MAX_PORT_LEN)
        return NULL;

    Tunnel *t = (Tunnel *)calloc(1, sizeof(Tunnel));
    if (!t) {
        log_error("Create tunnel, out of memory.");
        return NULL;
    }

    t->tcp_svr_sock = socket_create(AF_INET, SOCK_STREAM, host, port);
    if (t->tcp_svr_sock == INVALID_SOCKET) {
        log_error("Create socket and bind to %s:%s error:%d.",
                  host ? host : "0.0.0.0", port, socket_errno());
        free(t);
        return NULL;
    }

    t->udp_svr_sock = socket_create(AF_INET, SOCK_DGRAM, NULL, NULL);
    if (t->udp_svr_sock == INVALID_SOCKET) {
        log_error("Create UDP socket error:%d.", socket_errno());
        socket_close(t->tcp_svr_sock);
        free(t);
        return NULL;
    }

    t->channels = hashtable_create(512, 0.8f);
    if (!t->channels) {
        log_error("Create tunnel, out of memory.");
        socket_close(t->udp_svr_sock);
        socket_close(t->tcp_svr_sock);
        free(t);
        return NULL;
    }

    t->mode = TUNNEL_MODE_CLIENT;

    if (tunnel_say_hello(t, tunnel_host, tunnel_port) < 0) {
        socket_close(t->udp_svr_sock);
        socket_close(t->tcp_svr_sock);
        hashtable_free(t->channels, NULL);
        free(t);
        return NULL;
    }

    strcpy(t->remote_host, remote_host);
    strcpy(t->remote_port, remote_port);

    FD_ZERO(&t->fds);
    FD_SET(t->tcp_svr_sock, &t->fds);
    FD_SET(t->udp_svr_sock, &t->fds);

    log_info("Tunnel client start on %s.", socket_local_name(t->tcp_svr_sock));

    return t;
}

static inline uint16_t tunnel_next_cid(Tunnel *t)
{
    /* Channel 0 is reserved for tunnel */
    if (++t->cid == 0)
        ++t->cid;

    return t->cid;
}

static inline int tunnel_add_channel(Tunnel *t, Channel *ch, int opening)
{
    int32_t cid = opening ? -ch->id : ch->id;

    return hashtable_put(t->channels, cid, ch);
}

static inline Channel *tunnel_get_channel(Tunnel *t, uint16_t cid)
{
    return (Channel *)hashtable_get(t->channels, cid);
}

static inline Channel *tunnel_get_opening_channel(Tunnel *t, uint16_t cid)
{
    int32_t id = -cid;

    Channel *ch = (Channel *)hashtable_get(t->channels, id);
    if (ch)
        hashtable_remove(t->channels, id, NULL);

    return ch;
}

static inline int tunnel_delete_channel(Tunnel *t, Channel *ch)
{
    return hashtable_remove(t->channels, ch->id, 
                            (hashtable_entry_free)channel_close);
}

static int tunnel_server_new_channel(Tunnel *t, uint16_t sn, 
                                     void *data, size_t datalen, 
                                     const struct sockaddr *from,
                                     socklen_t fromlen)
{
    int rc;
    char *profile, *host, *port;
    char *tokc = NULL;
    uint16_t cid;
    Channel *ch;

    /* Data format is: profile:host:port\0 */

    if (strnlen((char *)data, datalen) == datalen) {
        /* no null terminal */
        log_warning("New channel request from %s denied, invalid request.",
                    socket_addr_name(from));
        return -1;
    }

    profile = strtok_r((char *)data, ":", &tokc);
    host = strtok_r(NULL, ":", &tokc);
    port = strtok_r(NULL, ":", &tokc);

    if (!profile || !host || !port ||
        strlen(profile) > TUNNEL_MAX_PROFILE_LEN ||
        strlen(host) > TUNNEL_MAX_HOST_LEN ||
        strlen(port) > TUNNEL_MAX_PORT_LEN) {
        log_warning("New channel request from %s denied, invalid request.",
                    socket_addr_name(from));
        return -1;
    }

    cid = tunnel_next_cid(t);
    
    log_debug("New channel(%d) request from %s to %s:%s.", cid,
              socket_addr_name(from), host, port);

    ch = channel_create_server(t, cid, host, port, from, fromlen);
    if (!ch) {
        channel_close(ch);
        return -1;
    }
    
    rc = message_send(t->udp_svr_sock, MSG_TUNNEL_NEW_CHANNEL_ACK, cid, sn,
                      NULL, 0, from, fromlen);
    if (rc <= 0) {
        channel_close(ch);
        log_error("New channel(%d) request from %s to %s:%d failed, "
                  "send ack error:%d.",
                  cid, socket_addr_name(from), host, port, socket_errno());
        return -1;
    }

    rc = tunnel_add_channel(t, ch, 0);
    if (!rc) {
        channel_close(ch);
        log_error("New channel(%d) from %s to %s:%d failed, out of memory.",
                  cid, socket_addr_name(from), host, port);
        return -1;
    }

    log_debug("Channel(%d) is opening, waiting for handshake.", cid);
    return 0;
}

static int tunnel_client_new_channel(Tunnel *t, SOCKET s)
{
    int rc;
    uint16_t sn;
    Channel *ch;
    char data[TUNNEL_MAX_DATA_LEN];
    size_t len;

    sn = t->sn++;

    log_debug("Connecting from %s, send new channel(%d) request.", 
               socket_remote_name(s), -sn);

    /* Initial channel ID is sn */
    ch = channel_create_client(t, s, sn,
                               (const struct sockaddr *)&t->tunnel_addr,
                               t->tunnel_addr_len);
    if (!ch)
        return -1;

    len = sprintf(data, "%s:%s:%s", TUNNEL_DEFAULT_PROFILE, 
                  t->remote_host, t->remote_port);
    rc = message_send(t->udp_svr_sock, MSG_TUNNEL_NEW_CHANNEL,
                      0, sn, data, len+1,
                      (const struct sockaddr *)&t->tunnel_addr,
                      t->tunnel_addr_len);
    if (rc <= 0) {
        log_error("Send new channel(%d) request error:%d.",
                  -sn, socket_errno());
        channel_mark_to_close(ch);
        channel_close(ch);
        return -1;
    }

    rc = tunnel_add_channel(t, ch, 1);
    if (!rc) {
        channel_mark_to_close(ch);
        channel_close(ch);
        log_error("New channel(%d) failed, out of memory.", -sn);
        return -1;
    }

    log_debug("Channel(%d) for %s is opening, waiting for handshake.", 
              -sn, socket_remote_name(s));

    return 0;
}

static int tunnel_client_new_channel_ack(Tunnel *t, Channel *ch,
                                         uint16_t new_cid, uint16_t sn)
{
    int rc;
    int old_cid = ch->id;

    rc = message_send(t->udp_svr_sock, MSG_TUNNEL_NEW_CHANNEL_ACK, new_cid, sn,
                      NULL, 0, (const struct sockaddr *)&t->tunnel_addr, 
                      t->tunnel_addr_len);
    if (rc <= 0) {
        log_error("New channel(%d) for %s, handshake error:%d.", 
                  -old_cid, socket_remote_name(ch->tcp_sock), socket_errno());
        channel_close(ch);
        return -1;
    }

    channel_opened(ch, new_cid);

    rc = tunnel_add_channel(t, ch, 0);
    if (!rc) {
        channel_close(ch);
        log_error("New channel(%d) for %s failed, out of memory."
                  -old_cid, socket_remote_name(ch->tcp_sock));
        return -1;
    }

    return 0;
}

static int tunnel_handle_message(Tunnel *t, Message *msg,  
                                 const struct sockaddr *from, socklen_t fromlen)
{
    int rc = 0;
    Channel *ch;

    switch (msg->type) {
    case MSG_TUNNEL_HELLO:
        /* for server side */
        rc = tunnel_hello_ack(t, msg->sn, msg->data, msg->length, 
                              from, fromlen);
        break;

    case MSG_TUNNEL_NEW_CHANNEL:
        /* for server side */
        rc = tunnel_server_new_channel(t, msg->sn, msg->data, msg->length, 
                                       from, fromlen);
        break;

    case MSG_TUNNEL_NEW_CHANNEL_ACK:
        if (t->mode == TUNNEL_MODE_SERVER) {
            ch = tunnel_get_channel(t, msg->channel_id);
            if (!ch) {
                log_warning("Unknown channal from %s, ignored.",
                             socket_addr_name(from));
                return -1;
            }

            rc = channel_connect(ch);
            if (rc < 0)
                tunnel_delete_channel(t, ch);
        } else if (t->mode == TUNNEL_MODE_CLIENT) {
            ch = tunnel_get_opening_channel(t, msg->sn);
            if (!ch) {
                log_warning("Unknown channal from %s, ignored.",
                             socket_addr_name(from));
                return -1;
            }

            tunnel_client_new_channel_ack(t, ch, msg->channel_id, msg->sn);
            /* on error, the channel will be closed in 
               tunnel_client_new_channel_ack*/
        }
        break;

    case MSG_CHANNEL_KEEPALIVE:
    case MSG_CHANNEL_DATA:
    case MSG_CHANNEL_DATA_ACK:
    case MSG_CHANNEL_CLOSE:
        ch = tunnel_get_channel(t, msg->channel_id);
        if (!ch) {
            log_warning("Unknown channal(%d) from %s, ignored.",
                         msg->channel_id, socket_addr_name(from));

            return -1;
        }
        rc = channel_handle_message(ch, msg);
        if (rc < 0)
            tunnel_delete_channel(t, ch);
        break;

    default:
        log_warning("Unknown message from %s, ignored.",
                    socket_addr_name(from));
        return -1;
    }

    return rc;
}

int tunnel_run(Tunnel *t)
{
    fd_set fds;
    int nfds;
    int rc;

    Channel *ch;
    uint32_t cid;

    /* receive message data */
    char msg_buf[sizeof(Message) + TUNNEL_MAX_DATA_LEN - 1];
    Message *msg;

    struct sockaddr_storage from;
    socklen_t fromlen;

    struct timeval timeout;
    struct timeval now;
    struct timeval check_time;
    struct timeval check_interval;

    timerclear(&timeout);

    gettimeofday(&now, NULL);
    check_interval.tv_sec = 0;
    check_interval.tv_usec = 500000;
    timeradd(&now, &check_interval, &check_time);

    msg = (Message *)msg_buf;

    if (t->mode == TUNNEL_MODE_CLIENT) {
        rc = listen(t->tcp_svr_sock, TUNNEL_SERVER_BACKLOG);
        if (rc != 0) {
            log_error("Listen on %s error:%d.", 
                      socket_local_name(t->tcp_svr_sock), socket_errno());
            return rc;
        }
    }

    while (!t->stop) {
        if(!timerisset(&timeout))
            timeout.tv_usec = 50000;

        fds = t->fds;
        nfds = select(FD_SETSIZE, &fds, NULL, NULL, &timeout);
        if (nfds < 0) {
            if (errno != EINTR)
                log_error("Tunnel select error:%d.", socket_errno());
            break;
        }

        /* Go through all the channels. */
        gettimeofday(&now, NULL);
        if (timercmp(&now, &check_time, >)) {
            rc = hashtable_first(t->channels, &cid, (void **)&ch);
            while (rc) {
                rc = channel_idle(ch);
                if (rc < 0)
                    tunnel_delete_channel(t, ch);

                rc = hashtable_next(t->channels, &cid, (void **)&ch);
            }

            /* Next check time */
            timeradd(&now, &check_interval, &check_time);
        }

        if (nfds > 0 && FD_ISSET(t->udp_svr_sock, &fds)) {
            msg->type = 0;
            msg->reserved = 0;
            msg->channel_id = 0;
            msg->sn = 0;
            msg->length = TUNNEL_MAX_DATA_LEN;
            fromlen = sizeof(from);

            rc = message_receive(t->udp_svr_sock, msg, 
                                 (struct sockaddr *)&from, &fromlen);
            if (rc == 0) {
                log_info("Tunnel closed.");
                break;
            } else if (rc < 0) {
                log_error("Tunnel recevie message error:%d.", socket_errno());
                break;
            } 

            if (rc != 1) {
                tunnel_handle_message(t, msg, (const struct sockaddr *)&from,
                                      fromlen);
            } else {
                log_warning("Tunnel recevied an invalid message, ingnore.");
            }

            nfds--;
        }

        if (t->mode == TUNNEL_MODE_CLIENT) {
            if (nfds > 0 && FD_ISSET(t->tcp_svr_sock, &fds)) {
                SOCKET s;

                s = accept(t->tcp_svr_sock, (struct sockaddr *)&from, &fromlen);
                if (s != INVALID_SOCKET) {
                    tunnel_client_new_channel(t, s);
                } else {
                    log_error("Accept client connection error:%d.",
                              socket_errno());
                }
                nfds--;
            }
        }

        /* Go through all connected channels*/
        if (nfds > 0) {
            rc = hashtable_first(t->channels, &cid, (void **)&ch);
            while (rc && nfds > 0) {
                if (channel_socket_isset(ch, &fds)) {
                    if (channel_tcp2udp_data(ch) <= 0) {
                        tunnel_delete_channel(t, ch);
                    }

                    nfds--;
                }
                rc = hashtable_next(t->channels, &cid, (void **)&ch);
            }
        }
    }

    log_info("Tunnel shutdown.");

    return rc;
}

void tunnel_close(Tunnel *t)
{
    if (t->udp_svr_sock != INVALID_SOCKET)
        socket_close(t->udp_svr_sock);

    if (t->mode == TUNNEL_MODE_CLIENT && t->tcp_svr_sock != INVALID_SOCKET)
        socket_close(t->tcp_svr_sock);

    if (t->channels)
        hashtable_free(t->channels, (hashtable_entry_free)channel_close);

    free(t);
}

void tunnel_stop(Tunnel *t)
{
    t->stop = 1;
}

void tunnel_sockets_set(Tunnel *t, SOCKET sock)
{
    if (!FD_ISSET(sock, &t->fds)) {
        FD_SET(sock, &t->fds);
        t->nfds++;
    }
}

void tunnel_sockets_clear(Tunnel *t, SOCKET sock)
{
    if (FD_ISSET(sock, &t->fds)) {
        FD_CLR(sock, &t->fds);
        t->nfds--;
    }
}
