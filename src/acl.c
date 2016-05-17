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

#define  _WINSOCK_DEPRECATED_NO_WARNINGS

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#if !defined(_WIN32) && !defined(_WIN64)
#include <unistd.h>
#endif

#include "config.h"

#include "socket.h"
#include "acl.h"

int acl_init(char *str, AccessControlList *acl)
{
    char *tokc = NULL;
    char *val;
    unsigned long addr; 

    /* Default allow all */
    acl->src = 0;
    acl->dest = 0;
    acl->dest_port = 0;
    acl->deny = 0;

    if (!str || !*str)
        return 0;

    val = strtok_r(str, ",", &tokc);
    if (!val || !*val)
        return -1;
    addr = (unsigned long)inet_addr(val);
    if (addr == INADDR_NONE)
        return -1;
    acl->src = (uint32_t)addr;

    val = strtok_r(NULL, ",", &tokc);
    if (!val || !*val)
        return -1;
    addr = (unsigned long)inet_addr(val);
    if (addr == INADDR_NONE)
        return -1;
    acl->dest = (uint32_t)addr;

    val = strtok_r(NULL, ",", &tokc);
    if (!val || !*val)
        return -1;
    acl->dest_port = (uint16_t)atoi(val);

    val = strtok_r(NULL, ",", &tokc);
    if (!val || !*val)
        return -1;
    if (strcmp("allow", val) == 0)
        acl->deny = 0;
    else if(strcmp("deny", val) == 0)
        acl->deny = 1;
    else
        return -1;

    return 0;
}

int acl_check(AccessControlList *acl, const struct sockaddr *src,
    const char *dest_ip, const char *dest_port)
{
    unsigned long src_addr, dest_addr;
    int nomatch = acl->deny ? 0 : 1;

#ifdef _MSC_VER
    src_addr = ((struct sockaddr_in *)src)->sin_addr.S_un.S_addr;
#else
    src_addr = ((struct sockaddr_in *)src)->sin_addr.s_addr;
#endif

    dest_addr = (unsigned long)inet_addr(dest_ip);
    if (dest_addr == INADDR_NONE)
        return 1;

    if (acl->src && acl->src != (uint32_t)src_addr)
        return nomatch;

    if (acl->dest && acl->dest != (uint32_t)dest_addr)
        return nomatch;

    if (acl->dest_port && acl->dest_port != (uint32_t)atoi(dest_port))
        return nomatch;

    return acl->deny;
}
