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

#ifndef __TUNNEL_H__
#define __TUNNEL_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Not include null terminal */
#define TUNNEL_MAX_PROFILE_LEN          63
#define TUNNEL_MAX_HOST_LEN             127
#define TUNNEL_MAX_PORT_LEN             63

struct tunnel;
typedef struct tunnel Tunnel;

Tunnel *tunnel_create_server(const char *host, const char *port);

Tunnel *tunnel_create_client(const char *host, const char *port,
                             const char *tunnel_host, char *tunnel_port,
                             const char *remote_host, char *remote_port);

void tunnel_close(Tunnel *t);

int tunnel_run(Tunnel *t);

void tunnel_stop(Tunnel *t);

#ifdef __cplusplus
}
#endif

#endif /* __TUNNEL_H__ */