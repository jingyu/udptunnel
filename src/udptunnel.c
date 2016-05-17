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
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <assert.h>
#include <getopt.h>

#include "config.h"

#if defined(_WIN32) || defined(_WIN64)
#include "socket.h"
#endif

#include "log.h"
#include "tunnel.h"

static int mode;

static char *host;          /* local udp or tcp server address */
static char *port;
static char *tunnel_host;   /* tunnel server address */
static char *tunnel_port;
static char *remote_host;   /* remote address */
static char *remote_port;

static char *acl;

#ifdef _DEBUG
static int log_level = UDPTUNNEL_LOG_DEBUG;
#else
static int log_level = UDPTUNNEL_LOG_INFO;
#endif

static Tunnel *t;

static void usage()
{
    printf("usage: udptunnel -s [host:]port [-a acl]|[-d acl] ...\n");
    printf("   or: udptunnel -c [host:]port -p host:port -t host:port\n");
    printf("\n"
           "  Server options:\n"
           "  -s    server mode. server host and port\n"
           "  -a    allowed source and dest\n"
           "        src_ip,dest_ip,dest_port,allow|deny\n"
           "        any ip is 0.0.0.0, any port is 0"
           "\n"
           "  Client options:\n"
           "  -c    client mode. local TCP server host and port\n"
           "  -t    tunnel server host and port\n"
           "  -r    remote host and port\n"
           "\n"
           "  Common options:\n"
           "  -v    verbose level, 0-3, default is 1\n"
           "        0 - Error, 1 - Warning, 2 - Info, 3 - Debug\n"
           "  -h    show this help and exit\n"
           "\n");
}

static void parse_addr(char *addr, char **host, char **port)
{
    char *p;
    assert(addr && host && port);

    p = strchr(addr, ':');
    if (p) {
        *p++ = 0;
        *host = *addr ? addr : NULL;
        *port = *p ? p : NULL;
    } else {
        *host = NULL;
        *port = addr;
    }
}

static void parse_args(int argc, char *argv[])
{
    int opt;

    static struct option long_options[] = {
        {"server",      required_argument, 0, 's'},
        {"acl",         required_argument, 0, 'a'},
        {"client",      required_argument, 0, 'c'},
        {"tunnel",      required_argument, 0, 't'},
        {"remote",      required_argument, 0, 'r'},
        {"verbose",     required_argument, 0, 'v'},
        {"help",        no_argument,       0, 'h'},
    };


    while ((opt = getopt_long(argc, argv, "s:a:c:t:r:v:h", long_options, NULL)) 
            != -1) {
        switch (opt) {
        case 's':
            mode = 's';
            parse_addr(optarg, &host, &port);
            break;

        case 'a':
            acl = optarg;
            break;

        case 'c':
            mode = 'c';
            parse_addr(optarg, &host, &port);
            break;

        case 't':
            parse_addr(optarg, &tunnel_host, &tunnel_port);
            break;

        case 'r':
            parse_addr(optarg, &remote_host, &remote_port);
            break;

        case 'v':
            log_level = atoi(optarg);
            break;

        case 'h':
            usage();
            exit(0);
            break;

        case '?':
            usage();
            exit(1);
            break;

        case ':':
            usage();
            exit(1);
            break;
        }
    }

    if (mode == 0) {
        printf("Tunnel mode option must be provided by -s or -c.\n\n");
        usage();
        exit(1);
    }

    if (mode == 's' && !port) {
        printf("Tunnel server option missing argument.\n\n");
        usage();
        exit(1);
    }

    if (mode == 'c') {
        if (!port) {
            printf("Tunnel client option missing argument.\n\n");
            usage();
            exit(1);
        }

        if (!tunnel_host || !tunnel_port) {
            printf("Missing tunnel server address option for client mode.\n\n");
            usage();
            exit(1);
        }

        if (!remote_host || !remote_port) {
            printf("Missing remote address option for client mode.\n\n");
            usage();
            exit(1);
        }
    }
}

static void stop(int UNUSED(sig) )
{   
    tunnel_stop(t);
}

int main(int argc, char *argv[])
{
#if defined(_WIN32) || defined(_WIN64)
    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(2,0), &wsa_data) != 0)
        return -1;
#endif

    parse_args(argc, argv);
    utlog_set_level(log_level);

    signal(SIGINT, stop);
    signal(SIGTERM, stop);

    if (mode == 's')
        t = tunnel_create_server(host, port, acl);
    else
        t = tunnel_create_client(host, port, tunnel_host, tunnel_port,
                                 remote_host, remote_port);

    if (!t)
        return -1;

    tunnel_run(t);

    tunnel_close(t);

#if defined(_WIN32) || defined(_WIN64)
    WSACleanup();
#endif
}
