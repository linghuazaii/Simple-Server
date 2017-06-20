#include <stdio.h>
#include <string>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <iostream>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <inttypes.h>
#include <time.h>
#include "threadpool.h"
using namespace std;

#define _8K 8 * 1024
#define _16K 16 * 1024
#define _64K 64 * 1024
#define _128K _64K * 2
#define _256K _128K * 2
#define _512K _256K * 2
#define _1M 1024 * 1024

#define PORT 19920
#define THREADPOOL_SIZE 8
#define BACKLOG 1024
#define MAX_EVENTS 3000
#define IPLEN 32

#define err(fmt, ...) do {\
    printf(fmt, ##__VA_ARGS__);\
    if (errno != 0)\
        printf(" (%s)", strerror(errno));\
    printf("\n");\
} while (0)

#define log(fmt, ...) do {\
    printf(fmt, ##__VA_ARGS__);\
    printf("\n");\
} while (0)

#define abort(fmt, ...) do {\
    err(fmt, ##__VA_ARGS__);\
    exit(1);\
} while(0)

#define ss_malloc(size) calloc(1, size)
#define ss_free(ptr) ss_free_imp((void **)&ptr)

#if 0
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
#endif

typedef struct ep_buffer_t {
    uint32_t length;
    char *buffer;
    uint32_t count;
} ep_buffer_t;

typedef enum {
    PACKET_NONE,
    PACKET_START,
    PACKET_CHUNCK,
    PACKET_END
} PACKET_STATE;

typedef struct ep_data_t {
    int epfd;
    int eventfd;
    PACKET_STATE packet_state;
    ep_buffer_t ep_read_buffer;
    ep_buffer_t ep_write_buffer;
    void (*read_callback) (void *);
    void (*write_callback) (void *);
    pthread_mutex_t buffer_mtx;
    void *extra;
} ep_data_t;

typedef struct request_t {
    ep_data_t *ep_data;
    char *buffer;
    uint32_t length;
} request_t;

threadpool_t *read_threadpool;
threadpool_t *write_threadpool;
threadpool_t *listener_threadpool;
threadpool_t *error_threadpool;
threadpool_t *worker_threadpool;

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
        return -1;
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
    if (ret == -1) {
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
    serv_addr.sin_addr.s_addr = INADDR_ANY;

    set_reuseaddr(sock);
    //set_tcp_defer_accept(sock);

    int ret = bind(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
    if (ret == -1)
        abort("bind socket error");

    ret = listen(sock, BACKLOG);
    if (ret == -1)
        abort("listen socket error");

    return sock;
}

int ss_epoll_create() {
    int epoll_fd = epoll_create1(0);
    if (epoll_fd == -1)
        abort("epoll create error");

    return epoll_fd;
}

int ss_epoll_ctl(int epfd, int op, int fd, struct epoll_event *event) {
    int ret = epoll_ctl(epfd, op, fd, event);
    if (ret == -1)
        abort("epoll_ctl error");

    return 0;
}

int ss_epoll_wait(int epfd, struct epoll_event *events, int maxevents) {
    int ret = epoll_wait(epfd, events, maxevents, -1);
    if (ret == -1)
        abort("epoll_wait error");

    return ret;
}

void ss_free_imp(void **ptr) {
    free(*ptr);
    *ptr = NULL;
}

void handle_response(void *data) {
    log("response finished!");
}

void response(request_t *req, char *resp, uint32_t length) {
    ep_data_t *data = req->ep_data;
    data->ep_write_buffer.buffer = (char *)ss_malloc(length);
    memcpy(data->ep_write_buffer.buffer, resp, length);
    data->ep_write_buffer.length = length;
    struct epoll_event event;
    event.data.fd = data->eventfd;
    event.data.ptr = data;
    event.events = EPOLLIN | EPOLLOUT | EPOLLET | EPOLLRDHUP; /* we should always allow read */
    ss_epoll_ctl(data->epfd, EPOLL_CTL_MOD, data->eventfd, &event);
    ss_free(req->buffer);
    ss_free(req);
}

void handle_request(void *data) {
    request_t *req = (request_t *)data;
    char *resp = (char *)ss_malloc(req->length + 1024);
    const char *msg = "Hi there, you just said: ";
    memcpy(resp, msg, strlen(msg));
    memcpy(resp + strlen(msg), req->buffer, req->length);
    response(req, resp, strlen(resp));
    ss_free(resp);
}

void do_accept(void *arg) {
    ep_data_t *data = (ep_data_t *)arg;
    struct sockaddr_in client;
    socklen_t addrlen = sizeof(client);
    int conn = accept4(data->eventfd, (struct sockaddr *)&client, &addrlen, SOCK_NONBLOCK);
    if (conn == -1)
        abort("accpet4 error");
    char ip[IPLEN];
    inet_ntop(AF_INET, &client.sin_addr, ip, IPLEN);
    log("new connection from %s, fd: %d", ip, conn);

    struct epoll_event event;
    event.data.fd = conn;
    //event.events = EPOLLIN | EPOLLOUT | EPOLLRDHUP; //shouldn't include EPOLLOUT '
    event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
    ep_data_t *ep_data = (ep_data_t *)ss_malloc(sizeof(ep_data_t));
    ep_data->epfd = data->epfd;
    ep_data->eventfd = conn;
    ep_data->packet_state = PACKET_START;
    ep_data->read_callback = handle_request; /* callback for request */
    ep_data->write_callback = handle_response; /* callback for response */
    pthread_mutex_init(&ep_data->buffer_mtx, NULL);
    ss_epoll_ctl(data->epfd, EPOLL_CTL_ADD, conn, &event);
}

void do_read(void *arg) {
    ep_data_t *data = (ep_data_t *)arg;
    while (true) {
        if (data->packet_state == PACKET_START) {
            uint32_t length;
            int ret = read(data->eventfd, &length, sizeof(length));
            if (ret == -1) {
                if (errno == EAGAIN) {
                    break; // no more data to read
                }
                err("read error for %d", data->eventfd);
                return;
            }
            pthread_mutex_lock(&data->buffer_mtx);
            data->ep_read_buffer.length = length;
            data->ep_read_buffer.buffer = (char *)ss_malloc(length);
            data->ep_read_buffer.count = 0;
            pthread_mutex_unlock(&data->buffer_mtx);
        }
        pthread_mutex_lock(&data->buffer_mtx);
        int count = read(data->eventfd, data->ep_read_buffer.buffer + data->ep_read_buffer.count, data->ep_read_buffer.length - data->ep_read_buffer.count);
        if (count == -1) {
            if (errno == EAGAIN) {
                pthread_mutex_unlock(&data->buffer_mtx);
                break; /* no more data to read */
            }
            err("read error for %d", data->eventfd);
            pthread_mutex_unlock(&data->buffer_mtx);
            return;
        }
        data->ep_read_buffer.count += count;
        if (data->ep_read_buffer.count < data->ep_read_buffer.length) {
            data->packet_state = PACKET_CHUNCK;
        } else {
            data->packet_state = PACKET_START; /* reset to PACKET_START to recv new packet */
            //data->read_callback(data); /* should not block read thread */
            request_t *req = (request_t *)ss_malloc(sizeof(request_t));
            req->ep_data = data;
            req->buffer = data->ep_read_buffer.buffer;
            req->length = data->ep_read_buffer.length;
            threadpool_add(worker_threadpool, data->read_callback, (void *)req, 0);
        }
        pthread_mutex_unlock(&data->buffer_mtx);
    }
}

void reset_epdata(ep_data_t *data) {
    ss_free(data->ep_write_buffer.buffer);
    ss_free(data->ep_read_buffer.buffer);
    memset(&data->ep_write_buffer, 0, sizeof(data->ep_write_buffer));
    memset(&data->ep_read_buffer, 0, sizeof(data->ep_read_buffer));
    /* remove write event */
    struct epoll_event event;
    event.data.fd = data->eventfd;
    event.data.ptr = (void *)data;
    event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
    ss_epoll_ctl(data->epfd, EPOLL_CTL_MOD, data->eventfd, &event);
}

void do_write(void *arg) {
    ep_data_t *data = (ep_data_t *)arg;
    while (true) {
        int count = write(data->eventfd, data->ep_write_buffer.buffer + data->ep_write_buffer.count, data->ep_write_buffer.length - data->ep_write_buffer.count);
        if (count == -1) {
            if (errno == EAGAIN) {
                break; /* wait for next write */
            }
            err("write error for %d", data->eventfd);
            return;
        }
        data->ep_write_buffer.count += count;
        if (data->ep_write_buffer.count < data->ep_write_buffer.length) {
            /* write not finished, wait for next write */
        } else {
            //data->write_callback(data); /* should not block write */
            threadpool_add(worker_threadpool, data->write_callback, NULL, 0);
            reset_epdata(data);
        }
    }
}

void do_close(void *arg) {
    ep_data_t *data = (ep_data_t *)arg;
    ss_epoll_ctl(data->epfd, EPOLL_CTL_DEL, data->eventfd, NULL);
    close(data->eventfd);
    pthread_mutex_destroy(&data->buffer_mtx);
    if (data != NULL) {
        if (data->ep_read_buffer.buffer != NULL)
            ss_free(data->ep_read_buffer.buffer);
        if (data->ep_write_buffer.buffer != NULL)
            ss_free(data->ep_write_buffer.buffer);
        ss_free(data);
    }
}

void event_loop() {
    int listener = ss_socket();
    int epfd = ss_epoll_create();
    log("epollfd: %d\tlistenfd: %d", epfd, listener);
    ep_data_t *listener_data = (ep_data_t *)ss_malloc(sizeof(ep_data_t));
    listener_data->epfd = epfd;
    listener_data->eventfd = listener;
    struct epoll_event ev_listener;
    ev_listener.data.fd = listener;
    ev_listener.data.ptr = listener_data;
    ev_listener.events = EPOLLIN;
    //ss_epoll_ctl(epfd, EPOLL_CTL_ADD, listener, &ev_listener);
    epoll_ctl(epfd, EPOLL_CTL_ADD, listener, &ev_listener);
    struct epoll_event events[MAX_EVENTS];
    for (;;) {
        int nfds = ss_epoll_wait(epfd, events, MAX_EVENTS);
        log("nfds: %d", nfds);
        for (int i = 0; i < nfds; ++i) {
            log("events fd: %d", events[i].data.fd);
            if (events[i].data.fd == listener) {
                threadpool_add(listener_threadpool, do_accept, events[i].data.ptr, 0);
            } else {
                if (events[i].events & EPOLLIN) {
                    threadpool_add(read_threadpool, do_read, events[i].data.ptr, 0);
                } 
                if (events[i].events & EPOLLOUT) {
                    threadpool_add(write_threadpool, do_write, events[i].data.ptr, 0);
                }
                if (events[i].events & EPOLLRDHUP | events[i].events & EPOLLERR | events[i].events & EPOLLHUP) {
                    threadpool_add(error_threadpool, do_close, events[i].data.ptr, 0);
                }
            }
        }
    }
}

int main(int argc, char **argv) {
    read_threadpool = threadpool_create(8, MAX_EVENTS, 0);
    write_threadpool = threadpool_create(8, MAX_EVENTS, 0);
    listener_threadpool = threadpool_create(1, MAX_EVENTS, 0);
    error_threadpool = threadpool_create(2, MAX_EVENTS, 0);
    worker_threadpool = threadpool_create(8, MAX_EVENTS * 2, 0);
    event_loop();

    return 0;
}
