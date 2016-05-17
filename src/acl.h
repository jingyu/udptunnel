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

#ifndef __ACL_H__
#define __ACL_H__

#include <stdint.h>

#include "socket.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct AccessControlList {
    uint32_t src;
    uint32_t dest;
    uint16_t dest_port;
    uint16_t deny;
    uint32_t reserved;
} AccessControlList;

int acl_init(char *str, AccessControlList *acl);

int acl_check(AccessControlList *acl, const struct sockaddr *src,
              const char *dest_ip, const char *dest_port);

#ifdef __cplusplus
}
#endif

#endif /* __ACL_H__ */
