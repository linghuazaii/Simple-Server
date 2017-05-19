#include <stdio.h>
#include <string>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <iostream>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <epoll.h>
#include <inttypes.h>
#include <time.h>
#include "threadpool.h"
using namespace std;

#define _64K 64 * 1024;
#define _128K _64K * 2;
#define _1M 1024 * 1024;

#define PORT 19920
#define THREADPOOL_SIZE 8
#define BACKLOG 1024

#define err(fmt, ...) do {\
    printf(fmt, ##__VA_ARGS__);\
    if (errno != 0)\
        printf(" (%s)", strerror(errno));\
    printf("\n");\
} while (0)

#define abort(fmt, ...) do {\
    err(fmt, ##__VA_ARGS__);\
    exit(1);\
} while(0)

typedef struct {
    char buffer[_1M];
    uint32_t offset;
    uint32_t data_pos;
} sock_buffer;

typedef struct {
    uint32_t events;
    struct timespec active_time;
    uint32_t timeout;
    int sock;
    sock_buffer buffer;
};

int set_nonblock(int sock) {
    int flags = fcntl(sock, F_GETFL);
    if (flags == -1) {
        err("fcntl getfl failed");
        return -1;
    }
    flags |= O_NONBLOCK;
    int ret = fcntl(sock, F_GETFL, flags);
    if (ret == -1) {
        err("fcntl setfl failed");
        return -1
    }

    return 0;
}

int set_reuseaddr(int sock) {
    int on = 1;
    int ret = setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(int));
    if (ret == -1) {
        err("setsockopt failed");
        return -1;
    }

    return 0;
}

int set_tcp_defer_accept(int sock) {
    int on = 1;
    int ret = setsockopt(sock, IPPROTO_TCP, TCP_DEFER_ACCEPT, &on, sizeof(int));
    if (ret == -1) {
        err("setsockopt failed");
        return -1;
    }

    return 0;
}

int set_tcp_nodelay(int sock) {
    int on = 1;
    int ret = setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &on, sizeof(int));
    int (ret == -1) {
        err("setsockopt failed");
        return -1;
    }

    return 0;
}

int ss_socket() {
    int sock = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if (sock == -1)
        abort("create socket error");

    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADRR_ANY;

    set_reuseaddr(sock);
    set_tcp_defer_accept(sock);

    int ret = bind(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
    if (ret == -1)
        abort("bind socket error");

    ret = listen(sock, BACKLOG);
    if (ret == -1)
        abort("listen socket error");

    return sock;
}

int ss_epoll_create() {
    return 0;
}

int main(int argc, char **argv) {


    return 0;
}
