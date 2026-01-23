#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#define BUF_SIZE 1024

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <server IP/hostname> <server port>\n", argv[0]);
        return 1;
    }

    // Parse port safely
    char *end = NULL;
    long port_long = strtol(argv[2], &end, 10);
    if (!end || *end != '\0' || port_long <= 0 || port_long > 65535) {
        fprintf(stderr, "Invalid port number\n");
        return 1;
    }

    // INPUT: expecting "ftp <filename>"
    char command[16];
    char filename[256];
    printf("ftp <file name>\n");
    if (scanf("%15s %255s", command, filename) != 2) {
        fprintf(stderr, "Invalid input format\n");
        return 1;
    }
    if (strcmp(command, "ftp") != 0) {
        fprintf(stderr, "Expected command: ftp <file name>\n");
        return 1;
    }

    // Check file exists
    FILE *fp = fopen(filename, "rb");
    if (fp == NULL) {
        // File doesn't exist => exit (per lab)
        return 1;
    }
    fclose(fp);

    // Resolve server address (hostname OR IP), UDP
    struct addrinfo hints, *servinfo = NULL, *p = NULL;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;      // IMPORTANT: allow IPv4 or IPv6 (hostnames may return either)
    hints.ai_socktype = SOCK_DGRAM;

    int rv = getaddrinfo(argv[1], argv[2], &hints, &servinfo);
    if (rv != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // Create a socket using the first usable result
    int sockfd = -1;
    for (p = servinfo; p != NULL; p = p->ai_next) {
        sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (sockfd >= 0) break;
    }
    if (sockfd < 0) {
        perror("socket");
        freeaddrinfo(servinfo);
        return 1;
    }

    // Send "ftp" to server (this is the step your code was missing)
    const char *msg = "ftp";
    ssize_t sent = sendto(sockfd, msg, strlen(msg), 0, p->ai_addr, p->ai_addrlen);
    if (sent < 0) {
        perror("sendto");
        close(sockfd);
        freeaddrinfo(servinfo);
        return 1;
    }

    // Receive reply ("yes" / "no")
    char buffer[BUF_SIZE];
    struct sockaddr_storage sender_addr;
    socklen_t addr_len = sizeof sender_addr;

    ssize_t n = recvfrom(sockfd, buffer, BUF_SIZE - 1, 0,
                         (struct sockaddr *)&sender_addr, &addr_len);
    if (n < 0) {
        perror("recvfrom");
        close(sockfd);
        freeaddrinfo(servinfo);
        return 1;
    }
    buffer[n] = '\0';

    if (strcmp(buffer, "yes") == 0) {
        printf("A file transfer can start.\n");
    } else {
        // "no" or anything else => exit silently (per lab)
        close(sockfd);
        freeaddrinfo(servinfo);
        return 1;
    }

    close(sockfd);
    freeaddrinfo(servinfo);
    return 0;
}
