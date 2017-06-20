/* This file is auto-generated.Edit it at your own peril.*/
#include <iostream>
#include <string>
#include <stdio.h>
#include <cstring>
#include <unistd.h>
#include <cstdlib>
#include <vector>
#include <set>
#include <map>
#include <termios.h>
#include <inttypes.h>
#include <errno.h>
#include <algorithm>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/uio.h>
#include <sys/mman.h>
#include <sched.h>
#include <malloc.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include "threadpool.h"
using namespace std;

#define PORT 19920
#define LINE 256

typedef struct data_t {
    uint32_t length;
    char buffer[256];
} data_t;

void routine(void *arg) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_port = htons(PORT);
    inet_aton("172.31.43.244", &server.sin_addr);
    FILE *poem = fopen("poem.txt", "r");
    connect(sock, (struct sockaddr *)&server, sizeof(server));
    char buffer[LINE];
    data_t data;
    while (true) {
        char *end = fgets(buffer, LINE, poem);
        if (end == NULL)
            break;
        data.length = strlen(buffer);
        strcpy(data.buffer, buffer);
        write(sock, &data, data.length + 4);
        cout<<"data send!"<<endl;
        int count = read(sock, buffer, LINE);
        buffer[count] = 0;
        cout<<buffer<<endl;
    //    sleep(1);
    }
    fclose(poem);
}

int main(int argc, char **argv) {
    threadpool_t *pool = threadpool_create(8, 3000, 0);
    for (int i = 0; i < 100; ++i) {
        threadpool_add(pool, routine, NULL, 0);
    }
    sleep(86400);

    return 0;
}

