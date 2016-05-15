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

#ifndef __MESSAGE_H__
#define __MESSAGE_H__

#include <stdint.h>

#include "socket.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Message types, 8 bits */
#define MSG_TUNNEL_HELLO                    0x01
#define MSG_TUNNEL_HELLO_ACK                0x02
#define MSG_TUNNEL_NEW_CHANNEL              0x03
#define MSG_TUNNEL_NEW_CHANNEL_ACK          0x04

#define MSG_CHANNEL_KEEPALIVE               0x05
#define MSG_CHANNEL_DATA                    0x06
#define MSG_CHANNEL_DATA_ACK                0x07
#define MSG_CHANNEL_CLOSE                   0x0F

#ifdef _MSC_VER
#pragma pack(push, 1)
#endif
struct message
{
    uint8_t type;
    uint8_t reserved;
    uint16_t channel_id;
    uint16_t sn;
    uint16_t length;
    char data[1];
} 
#ifdef __GNUC__
__attribute__ ((__packed__))
#endif
;
#ifdef _MSC_VER
#pragma pack(pop, 1)
#endif

typedef struct message Message;

int message_send(SOCKET sock, uint8_t type, uint16_t cid, uint16_t sn,
                 void *data, size_t len, 
                 const struct sockaddr *addr, socklen_t addrlen);

int message_receive(SOCKET sock, Message *msg, 
                    struct sockaddr *from, socklen_t *fromlen);

#ifdef __cplusplus
}
#endif

#endif /* __MESSAGE_H__ */
