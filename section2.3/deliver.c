#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>
#include <time.h>
#include <errno.h>
#include <math.h>

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
    // RTT/timeout estimator 
    double EstimatedRTT = 0.5; 
    double DevRTT = 0.25;       
    const double alpha = 0.125;
    const double beta  = 0.25;

    for (unsigned int frag_no = 1; frag_no <= total_frag; ) {

        // read current fragment (only advance when ACKed)
        size_t size = fread(data, 1, DATA_SIZE, fp); //read up to 1000Bytes 

        int header_len = snprintf(packet, sizeof(packet),
                                  "%u:%u:%zu:%s:", total_frag, frag_no, size, filename);
        memcpy(packet + header_len, data, size);

        // send fragment once initially
        struct timespec sendtime, recvtime;
        clock_gettime(CLOCK_MONOTONIC, &sendtime);
        if (sendto(sockfd, packet, header_len + size, 0, res->ai_addr, res->ai_addrlen) < 0) {
            perror("sendto");
            break;
        }

        while (1) {
            double t1 = EstimatedRTT + 4.0 * DevRTT;

            // set recv timeout directly here (NO helper function)
            struct timeval tv;
            tv.tv_sec = (int)t1;
            tv.tv_usec = (int)((t1 - tv.tv_sec) * 1e6);
            setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

            struct sockaddr_storage server;
            socklen_t server_len = sizeof(server);

            ssize_t rn = recvfrom(sockfd, ctrlbuf, sizeof(ctrlbuf) - 1, 0,
                                  (struct sockaddr *)&server, &server_len);

            if (rn < 0) {
                // timeout -> retransmit
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    printf("[TIMEOUT] frag %u (t1=%.3fs) -> retransmitting\n", frag_no, t1);

                    clock_gettime(CLOCK_MONOTONIC, &sendtime); // reset timer for this resend
                    if (sendto(sockfd, packet, header_len + size, 0, res->ai_addr, res->ai_addrlen) < 0) {
                        perror("sendto (retransmit)");
                        goto done;
                    }
                    continue;
                }
                if (errno == EINTR) continue;

                perror("recvfrom");
                goto done;
            }

            ctrlbuf[rn] = '\0';
            clock_gettime(CLOCK_MONOTONIC, &recvtime);

            char type[8];
            unsigned int fno;
            if (sscanf(ctrlbuf, "%7[^:]:%u", type, &fno) != 2) continue;
            if (fno != frag_no) continue;
            

            // SampleRTT
            double rtt = (recvtime.tv_sec - sendtime.tv_sec) +
                            (recvtime.tv_nsec - sendtime.tv_nsec) / 1e9;

            printf("frag %u got %s, SampleRTT=%.6f s\n", frag_no, type, rtt);

            if (strcmp(type, "ACK") == 0) {
                // update estimator
                double err = fabs(rtt - EstimatedRTT);
                EstimatedRTT = (1.0 - alpha) * EstimatedRTT + alpha * rtt;
                DevRTT       = (1.0 - beta)  * DevRTT       + beta  * err;

                frag_no++; // next fragment
                break;
            }

            if (strcmp(type, "NACK") == 0) {
                printf("[NACK] frag %u -> retransmitting\n", frag_no);

                clock_gettime(CLOCK_MONOTONIC, &sendtime);
                if (sendto(sockfd, packet, header_len + size, 0, res->ai_addr, res->ai_addrlen) < 0) {
                    perror("sendto (NACK retransmit)");
                    goto done;
                }
            }
        }
    }

    printf("File transfer completed.\n");

done:
    fclose(fp);
    close(sockfd);
    freeaddrinfo(res);
    return 0;
}
