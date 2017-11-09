#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>


#define MAX_LEN 1100

void save_file(int socket);

typedef  struct Packet {
    unsigned int total_frag;
    unsigned int frag_no;
    unsigned int size;
    char filename[1024];
    char filedata[1000];
}Packet;

int main(int argc, char **argv) {
    int sock;
    struct addrinfo info;
    struct addrinfo *servinfo;
    char buff_s[2000];
    if (argc != 2) {
        fprintf(stderr, "Usage: server <UDP listen port>\n");
        return 1;
    }
    bzero(&info, sizeof(info));
    info.ai_family = AF_INET;
    info.ai_socktype = SOCK_DGRAM;
    info.ai_flags = AI_PASSIVE;

    int ret = getaddrinfo(NULL, argv[1], &info, &servinfo);
    if (ret != 0) {
        fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(ret));
        return 1;
    }
    sock = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);
    if (sock == -1) {
        perror("Socket error");
    }
    if (bind(sock, servinfo->ai_addr, servinfo->ai_addrlen) == -1) {
        close(sock);
        perror("Bind error");
    }

    freeaddrinfo(servinfo);
    while (1) {
        struct sockaddr addr;
        socklen_t len = sizeof(addr);
        bzero(&addr, sizeof(addr));
        //printf("Start...\n");
        int bytesNum = recvfrom(sock, buff_s, sizeof(buff_s) - 1, 0, &addr, &len);
        if (bytesNum == -1) {
            perror("Recvfrom error.");
            break;
        }
        buff_s[bytesNum] = '\0';

        char ans[5] = "no";
        if (strcmp(buff_s, "ftp") == 0) {
            strcpy(ans, "yes");
        }
        sendto(sock, ans, strlen(ans) + 1, 0, &addr, len);
        save_file(sock);
    }
    close(sock);
    return 0;
}

//parse
void parse(char* buffer, Packet* packet){
    char tmp[1024];
    char* beg = buffer;
    char* end = strstr(beg, ":");
    strncpy(tmp, beg, end - beg);
    tmp[end - beg] = '\0';
    packet->total_frag = atoi(tmp);
    beg = end + 1;

    end = strstr(beg, ":");
    strncpy(tmp, beg, end - beg);
    tmp[end - beg] = '\0';
    packet->frag_no = atoi(tmp);
    beg = end + 1;

    end = strstr(beg, ":");
    strncpy(tmp, beg, end - beg);
    tmp[end - beg] = '\0';
    packet->size = atoi(tmp);
    beg = end + 1;

    end = strstr(beg, ":");
    strncpy(packet->filename, beg, end - beg);
    packet->filename[end - beg] = '\0';
    beg = end + 1;

    strcpy(packet->filedata, beg);
}

void save_file(int socket) {
    char buffer[MAX_LEN];
    char filename[200];
    bzero(filename, 200);
    int* is_arrived, total = 0, num = 0;
    FILE* pf = NULL;
    Packet packet;
    int i;

    while(pf == NULL || num < total){
        struct sockaddr new_source;
        socklen_t addrlen = sizeof(new_source);
        int bytesNum = recvfrom(socket, buffer, MAX_LEN, 0, &new_source, &addrlen);
        if (bytesNum == -1) {
            perror("Recvfrom error");
            break;
        }
        buffer[bytesNum] = '\0';
        parse(buffer, &packet);
        printf("Receive: total_frag=%d, frag_no=%d\n", packet.total_frag, packet.frag_no);
        //printf("Msg: %s\n", packet.filedata);

        if (pf == NULL){
            pf = fopen(packet.filename, "w");
            total = packet.total_frag;
            is_arrived = (int*)malloc(sizeof(int) * (total + 1));
            for (i = 0; i < total + 1; ++i) {
                is_arrived[i] = 0;
            }
        }
        sendto(socket, "ACK", sizeof(char) * 3, 0, &new_source, addrlen);
        if (is_arrived[packet.frag_no]){
            continue;
        }
        is_arrived[packet.frag_no] = 1;
        num ++;

        long int offset = (packet.frag_no - 1) * 1000 * sizeof(char);
        fseek(pf, offset, SEEK_SET);
        fwrite(packet.filedata, sizeof(char), packet.size, pf);
    }
    fclose(pf);
}

