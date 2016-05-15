#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if !defined(_WIN32) && !defined(_WIN64)
#include <unistd.h>
#endif

#include "config.h"

#include "socket.h"

static unsigned long total_bytes;

static int tcp_server(const char *host, const char *port,
                      int size, int count, int interval)
{
    int rc;
    SOCKET s;
    SOCKET c;
    
    int i = 0;
    char buf[8192];

    s = socket_create(AF_INET, SOCK_STREAM, host, port);
    if (s == INVALID_SOCKET) {
        printf("Create and bind socket error(%d).\n", socket_errno());
        return -1;
    }

    rc = listen(s, 10);
    if (rc < 0) {
        printf("Listen on socket error(%d).\n", socket_errno());
        socket_close(s);
        return rc;
    }

    c = accept(s, NULL, NULL);
    if (c == INVALID_SOCKET) {
        printf("Accept on socket error(%d).\n", socket_errno());
        socket_close(s);
        return -1;
    }

    size = size <= 8192 ? size : 8192;

    memset(buf, '*', size);

    while (1) {
        rc = send(c, buf, size, 0);
        if (rc == 0) {
            printf("Socket closed.\n");
            break;
        } else if (rc < 0) {
            printf("Socket error: %d.\n", socket_errno());
            break;
        }

        printf("%8d: Sent %d bytes\n", i, rc);
        fflush(stdout);
        total_bytes += rc;

        i++;

        if (count && i >= count)
            break;

        if (interval)
            usleep(interval * 1000);
    }

    socket_close(c);
    socket_close(s);

    printf("Total sent %ld bytes.\n", total_bytes);
    return rc;
}

static int tcp_client(const char *server, const char *port,
                      int size, int count, int interval)
{
    int rc;
    SOCKET s;
    int i = 0;
    char buf[8192];

    s = socket_connect(AF_INET, SOCK_STREAM, server, port);
    if (s == INVALID_SOCKET) {
        printf("Connect to server error(%d).\n", socket_errno());
        return -1;
    }

    size = size <= 8192 ? size : 8192;

    while (1) {
        rc = recv(s, buf, size, 0);
        if (rc == 0) {
            printf("Socket closed.\n");
            break;
        } else if (rc < 0) {
            printf("Socket error: %d.\n", socket_errno());
            break;
        }

        printf("%8d: received %d bytes\n", i, rc);
        fflush(stdout);

        total_bytes += rc;
        i++;

        if (count && i >= count)
            break;

        if (interval)
            usleep(interval * 1000);
    }

    socket_close(s);

    printf("Total reveived %ld bytes.\n", total_bytes);
    return rc;
}

static void usage()
{
    printf("Usage: tcptest -s|-c host port [size] [count] [interval]\n"
            "         size        Packet size to send or receive.\n"
            "         count       Total packet count to send or receive.\n"
            "                     0 for infinity.\n"
            "         interval    Send or receive interval, in milliseconds.\n"
            "\n");
    exit(-1);
}

int main(int argc, char *argv[])
{
    int mode;
    char *host;
    char *port;
    int size = 1024;
    int count = 0;
    int interval = 0;

#if defined(_WIN32) || defined(_WIN64)
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        printf("WSAStartup failed.");
        return -1;
    }
#endif

    if (argc < 4)
        usage();

    if (strcmp(argv[1], "-s") == 0)
        mode = 's';
    else if (strcmp(argv[1], "-c") == 0)
        mode = 'c';
    else
        usage();

    host = argv[2];
    port = argv[3];

    if (argc >=5)
        size = atoi(argv[4]);

    if (argc >=6)
        count = atoi(argv[5]);

    if (argc >=7)
        interval = atoi(argv[6]);

    if (mode == 's')
        tcp_server(host, port, size, count, interval);
    else
        tcp_client(host, port, size, count, interval);

#if defined(_WIN32) || defined(_WIN64)
    WSACleanup();
#endif

    return 0;
}