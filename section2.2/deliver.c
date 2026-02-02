#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>
#include <time.h>

#define DATA_SIZE 1000
#define BUF_SIZE 4096

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Usage: %s <server address> <server port>\n", argv[0]);
        return 1;
    }

    char cmd[16], filename[256];
    printf("ftp <file name>\n");
    if (scanf("%15s %255s", cmd, filename) != 2) return 1;
    if (strcmp(cmd, "ftp") != 0) return 1;

    FILE *fp = fopen(filename, "rb");
    if (!fp) return 1;

    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    getaddrinfo(argv[1], argv[2], &hints, &res);

    int sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);

    fseek(fp, 0, SEEK_END); //move fp to the end of file
    long filesize = ftell(fp); //size of file
    rewind(fp);//move fp to start of file

    //calculate total_frag need to transfer this file
    unsigned int total_frag = (filesize + DATA_SIZE - 1) / DATA_SIZE;

    char packet[BUF_SIZE];
    char ctrlbuf[128]; //ACK or NACK
    unsigned char data[DATA_SIZE];
    struct timespec sendtime, recvtime; //time logic
    double expected_rtt = 0.5;

    for (unsigned int frag_no = 1; frag_no <= total_frag; ) {
        size_t size = fread(data, 1, DATA_SIZE, fp); //read up to 1000Bytes 

        int header_len = snprintf(packet, sizeof(packet),
                                  "%u:%u:%zu:%s:", total_frag, frag_no, size, filename);
        memcpy(packet + header_len, data, size);
        clock_gettime (CLOCK_MONOTONIC, &sendtime); //
        sendto(sockfd, packet, header_len + size, 0, res->ai_addr, res->ai_addrlen);

        while (1) {
            struct sockaddr_storage server;
            socklen_t server_len = sizeof(server);

            ssize_t rn = recvfrom(sockfd, ctrlbuf, sizeof(ctrlbuf)-1, 0, (struct sockaddr *)&server, &server_len);
            ctrlbuf[rn] = '\0';
            clock_gettime(CLOCK_MONOTONIC, &recvtime);
            double rtt = (recvtime.tv_sec - sendtime.tv_sec) + (recvtime.tv_nsec - sendtime.tv_nsec)/1e9;
            printf("fragment %d sent RTT:%f s\n", frag_no, rtt);

            char type[8];
            unsigned int fno;

            if (sscanf(ctrlbuf, "%7[^:]:%u", type, &fno) != 2) continue;
            if (fno != frag_no) continue;

            if (strcmp(type, "ACK") == 0 && rtt <= expected_rtt) {
                frag_no++;   // move to next fragment
                break;
            }
                    
            if (strcmp(type, "NACK") == 0 || (strcmp(type, "ACK") == 0 && rtt > expected_rtt)) {
                // resend same fragment
                sendto(sockfd, packet, header_len + size, 0, res->ai_addr, res->ai_addrlen);
            }
        }
    }

    printf("File transfer completed.\n");
    fclose(fp);
    close(sockfd);
    freeaddrinfo(res);
    return 0;
}
