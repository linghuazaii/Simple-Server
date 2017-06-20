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
using namespace std;

#define PORT 19920
#define LINE 256

int main(int argc, char **argv) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_port = htons(PORT);
    inet_aton("172.31.43.244", &server.sin_addr);
    FILE *poem = fopen("poem.txt", "r");
    connect(sock, (struct sockaddr *)&server, sizeof(server));
    char buffer[LINE];
    while (true) {
        char *end = fgets(buffer, LINE, poem);
        if (end == NULL)
            break;
        write(sock, buffer, strlen(buffer));
        int count = read(sock, buffer, LINE);
        buffer[count] = 0;
        cout<<buffer<<endl;
        sleep(1);
    }

    return 0;
}

