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

#include <string.h>
#include <assert.h>

#include "config.h"

#include "socket.h"
#include "log.h"
#include "tunnel_i.h"

#include "message.h"

#ifdef _TRACE_MESSSAG
static const char *message_get_type_name(int type)
{
    const char *name;

    switch (type) {
    case MSG_TUNNEL_HELLO:
        name = "TH";
        break;

    case MSG_TUNNEL_HELLO_ACK:
        name = "THA";
        break;

    case MSG_TUNNEL_NEW_CHANNEL:
        name = "TC";
        break;

    case MSG_TUNNEL_NEW_CHANNEL_ACK:
        name = "TCA";
        break;

    case MSG_CHANNEL_KEEPALIVE:
        name = "CK";
        break;

    case MSG_CHANNEL_DATA:
        name = "CD";
        break;

    case MSG_CHANNEL_DATA_ACK:
        name = "CDA";
        break;

    case MSG_CHANNEL_CLOSE:
        name = "CC";
        break;

    default:
        name = "N/A";
    }

    return name;
}
#endif

int message_send(SOCKET sock, uint8_t type, uint16_t cid, uint16_t sn,
                 void *data, size_t len, 
                 const struct sockaddr *addr, socklen_t addrlen)
{
    int rc;
    int msg_len;
    char msg_buf[sizeof(Message) + TUNNEL_MAX_DATA_LEN - 1];

    assert(len <= TUNNEL_MAX_DATA_LEN);

    msg_len = sizeof(Message) + len - 1;
    Message *msg = (Message *)msg_buf;

    msg->type = type;
    msg->reserved = 0;
    msg->channel_id = htons(cid);
    msg->sn = htons(sn);
    msg->length = htons((short)len);

    if (data && len)
        memcpy(msg->data, data, len);

    rc = sendto(sock, (const char *)msg, msg_len, 0, addr, addrlen);

#ifdef _TRACE_MESSSAG
    if (rc == msg_len) {
        log_debug("=>>U Message[type=%s, cid=%d, sn=%d, payload=%d] to %s.",
                  message_get_type_name(type), cid, sn, len, 
                  socket_addr_name(addr));
    } else {
        log_error("=>>U Message[type=%s, cid=%d, sn=%d, payload=%d] to %s. "
                  "error:%d",
                  message_get_type_name(type), cid, sn, len, 
                  socket_addr_name(addr), socket_errno());
    }
#endif

    return rc;
}

/* return value: 0 on success, -1 on error, 1 on invalid message */
int message_receive(SOCKET sock, Message *msg, 
                    struct sockaddr *from, socklen_t *fromlen)
{
    int rc;
    size_t msg_len = sizeof(Message) + TUNNEL_MAX_DATA_LEN - 1;

    assert(msg->length == TUNNEL_MAX_DATA_LEN);

    rc = recvfrom(sock, (char *)msg, msg_len, 0, from, fromlen);
    if (rc <= 0) /* closed or error */
        return rc;

    msg->length = ntohs(msg->length);
    if ((size_t)rc != sizeof(Message) + msg->length - 1) {
        /* Invalid message, ignore */
#ifdef _TRACE_MESSSAG
        log_warning("<<=T Message invalid, ignore!");
#endif
        rc = 1;
    }

    msg->channel_id = ntohs(msg->channel_id);
    msg->sn = ntohs(msg->sn);

#ifdef _TRACE_MESSSAG
    log_debug("<<=U Message[type=%s, cid=%d, sn=%d, payload=%d] from %s.",
              message_get_type_name(msg->type), msg->channel_id, msg->sn,
              msg->length, socket_addr_name(from));
#endif

    return rc;
}
