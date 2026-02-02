#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

#define BUF_SIZE 1024

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <UDP listen port>\n", argv[0]);
        return 1;
    }

    int port = atoi(argv[1]);  
    if (port <= 0 || port > 65535) {
        fprintf(stderr, "Invalid port\n");
        return 1;
    }

    // Create UDP socket
    int sfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sfd < 0) {
        perror("socket");
        return 1;
    }

    // Bind to <port> on all local interfaces
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons((uint16_t)port);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(sfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        close(sfd);
        return 1;
    }

    printf("Server listening on UDP port %d...\n", port);

    // Receive a message
    char buf[BUF_SIZE];
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);

    ssize_t n = recvfrom(sfd, buf, BUF_SIZE - 1, 0,
                         (struct sockaddr *)&client_addr, &client_len);
    if (n < 0) {
        perror("recvfrom");
        close(sfd);
        return 1;
    }
    buf[n] = '\0'; 

    // Reply yes/no
    const char *reply = (strcmp(buf, "ftp") == 0) ? "yes" : "no";

    if (sendto(sfd, reply, strlen(reply), 0,
               (struct sockaddr *)&client_addr, client_len) < 0) {
        perror("sendto");
        close(sfd);
        return 1;
    }

    printf("process to file transfer");


    return 0;
}

