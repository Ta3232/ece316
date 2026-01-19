#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#define BUF_SIZE 1024
ssize_t sendto(int sockfd, const void *buf, size_t len, int flags,
const struct sockaddr *dest_addr, socklen_t addrlen);

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <server IP/hostname> <server port>\n", argv[0]);
        return 1;
    }

    char *server_ip = argv[1];
    int port = atoi(argv[2]);
    if (port <= 0 || port > 65535) {
        fprintf(stderr, "Invalid port number\n");
        return 1;
    }

    //INPUT
    char command[16];
    char filename[256];

    printf("ftp <file name>\n");
    if (scanf("%15s %255s", command, filename) != 2) {
        fprintf(stderr, "Invalid input format\n");
        return 1;
    }

    FILE *fp = fopen(filename, "rb");
    if (fp == NULL) {
        //printf("don't exist\n");
        return 1;
    }
    fclose(fp);
    //printf("ftp\n");

    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("socket");
        return 1;
    }

    //get server addr
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons((uint16_t)port);

    //inet_pton(AF_INET, server_ip, &server_addr.sin_addr);
    struct addrinfo hints, *res = NULL;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;

    int err = getaddrinfo(server_ip, argv[2], &hints, &res);
    if (err != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(err));
        close(sockfd);
        return 1;
    }

struct sockaddr_in server_addr;
memset(&server_addr, 0, sizeof(server_addr));
memcpy(&server_addr, res->ai_addr, sizeof(server_addr));
freeaddrinfo(res);

    
    sendto(sockfd, "ftp", 3, 0,
               (struct sockaddr *)&server_addr,
               sizeof(server_addr));
    
    char buffer[BUF_SIZE];
    socklen_t addr_len = sizeof(server_addr);

    ssize_t n = recvfrom(sockfd, buffer, BUF_SIZE - 1, 0,
                         (struct sockaddr *)&server_addr,
                         &addr_len);

    buffer[n] = '\0'; 
    if (strcmp(buffer, "yes") == 0) {
        printf("A file transfer can start.\n");
    }

    close(sockfd);
    return 0;

}
