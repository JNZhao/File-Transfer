#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <math.h>
#include <unistd.h>
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>


#define MAX_LEN 1000

int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "Usage: deliver <server address> <server port number>\n");
        return 1;
    }

    char cmd[MAX_LEN], file_name[MAX_LEN];
    char buffer[MAX_LEN], message[MAX_LEN + 200];
    int sockSend;
    struct addrinfo tmpAddr, *info;;
    bzero(&tmpAddr, sizeof(tmpAddr));
    tmpAddr.ai_family = AF_INET;
    tmpAddr.ai_socktype = SOCK_DGRAM;
    if (getaddrinfo(argv[1], argv[2], &tmpAddr, &info) != 0) {
        perror("getaddrinfo Error..");
        return 1;
    }

    clock_t time = clock();
    sockSend = socket(info->ai_family, info->ai_socktype, info->ai_protocol);
    if (sockSend == -1) {
        perror("socket Error");
        exit(1);
    }
    freeaddrinfo(info);

    scanf("%s%s", cmd, file_name);
    if (strcmp(cmd, "ftp") != 0) {
        fprintf(stderr, "Usage: ftp <file>\n");
        exit(1);
    }

    if (access(file_name, F_OK) != 0) {
        fprintf(stderr, "Error: %s does not exist\n", file_name);
        exit(1);
    }
    if (sendto(sockSend, "ftp", 3, 0, info->ai_addr, info->ai_addrlen) == -1) {
        perror("sendto Error...");
        exit(0);
    }

    struct sockaddr addr;
    socklen_t len = sizeof(addr);
    if (recvfrom(sockSend, buffer, sizeof(buffer), 0, &addr, &len) < 0) {
        perror("recvfrom Error...");
        exit(1);
    }
    printf("Round trip: %.4f ms\n", ((double) clock() - time) / CLOCKS_PER_SEC * 1000);

    //check receive command
    if (strcmp(buffer, "yes") != 0) {
        exit(1);
    } else {
        printf("A file transfer can start\n");
    }

    //open file
    FILE *fp = fopen(file_name, "r");
    if (fp == NULL) {
        fprintf(stderr, "Error: open %s\n", file_name);
        exit(1);
    }

    fseek(fp, 0L, SEEK_END);
    long int file_size = ftell(fp);
    fseek(fp, 0L, SEEK_SET);

    int total_frag = ceil(((double) file_size) / MAX_LEN);
    int frag_no = 1;
    int total_bytes = 0;

    struct timeval time_out_flag;
    time_out_flag.tv_sec = 0;
    time_out_flag.tv_usec = 500000;
    setsockopt(sockSend, SOL_SOCKET, SO_RCVTIMEO, &time_out_flag, sizeof(time_out_flag));

    while (!feof(fp)) {
        //read data from file
        int bytes_num = 0;
        while (bytes_num < MAX_LEN && !feof(fp)) {
            int b = fread(buffer + bytes_num, sizeof(char), MAX_LEN - bytes_num, fp);
            if (b < -1) {
                fprintf(stderr, "Error: fread %s\n", file_name);
                exit(1);
            }
            bytes_num += b;
        }
        sprintf(message, "%d:%d:%d:%s:%s", total_frag, frag_no, bytes_num, file_name, buffer);
        //printf("msg: %s\n", message);
        if (sendto(sockSend, message, strlen(message), 0, info->ai_addr, info->ai_addrlen) == -1) {
            perror("sendto Error...");
            exit(1);
        }
        total_bytes += bytes_num;

        struct sockaddr new_source;
        socklen_t addrlen = sizeof(new_source);
        if (recvfrom(sockSend, buffer, 5, 0, &new_source, &addrlen) < 0 && errno == EAGAIN) {
            printf("Timeout for ACK\n");
            total_bytes -= bytes_num;
            fseek(fp, -bytes_num, SEEK_CUR);
            continue;
        }
        if (strcmp(buffer, "ACK") == 0) {
            printf("ACK\n");
        }
        frag_no += 1;
    }
    printf("File size is %ld bytes and actually %d bytes are sent.\n", file_size, total_bytes);

    close(sockSend);
    fclose(fp);

    return 0;
}
