#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>

#define MAX_NAME 50
#define MAX_DATA 1024

typedef struct {
    unsigned int type;
    unsigned int size;
    char source[MAX_NAME];
    char data[MAX_DATA];
} message;

enum {
    LOGIN = 1,
    LO_ACK,
    LO_NAK,
    EXIT,
    JOIN,
    JN_ACK,
    JN_NAK,
    LEAVE_SESS,
    NEW_SESS,
    NS_ACK,
    MESSAGE,
    QUERY,
    QU_ACK
};

int sockfd = -1;
char client_id[MAX_NAME];
pthread_t recv_thread;

void *receiver(void *arg) {

    message msg;

    while (1) {

        int n = recv(sockfd, &msg, sizeof(msg), 0);

        if (n <= 0) {
            printf("Disconnected from server\n");
            exit(0);
        }

        if (msg.type == MESSAGE) {
            printf("%s: %s\n", msg.source, msg.data);
        } else {
            printf("%s\n", msg.data);
        }
    }
}

void send_msg(int type, char *data) {

    message msg;

    memset(&msg, 0, sizeof(msg));

    msg.type = type;

    strcpy(msg.source, client_id);

    if (data)
        strcpy(msg.data, data);

    send(sockfd, &msg, sizeof(msg), 0);
}

int main() {

    char input[1024];

    while (1) {

        fgets(input, sizeof(input), stdin);
        input[strcspn(input, "\n")] = 0;

        if (strncmp(input, "/login", 6) == 0) {

            char id[50], pass[50], ip[50];
            int port;

            sscanf(input, "/login %s %s %s %d", id, pass, ip, &port);

            sockfd = socket(AF_INET, SOCK_STREAM, 0);

            struct sockaddr_in server;
            server.sin_family = AF_INET;
            server.sin_port = htons(port);

            inet_pton(AF_INET, ip, &server.sin_addr);

            connect(sockfd, (struct sockaddr*)&server, sizeof(server));

            strcpy(client_id, id);

            message msg;
            memset(&msg,0,sizeof(msg));

            msg.type = LOGIN;
            strcpy(msg.source, id);
            strcpy(msg.data, pass);

            send(sockfd, &msg, sizeof(msg), 0);

            recv(sockfd, &msg, sizeof(msg), 0);

            if (msg.type == LO_ACK) {
                printf("Login successful\n");
                pthread_create(&recv_thread, NULL, receiver, NULL);
            } else {
                printf("Login failed: %s\n", msg.data);
            }

        }

        else if (strcmp(input, "/logout") == 0) {

            send_msg(EXIT, "");
            close(sockfd);

        }

        else if (strncmp(input, "/joinsession", 12) == 0) {

            char session[50];
            sscanf(input, "/joinsession %s", session);

            send_msg(JOIN, session);
        }

        else if (strcmp(input, "/leavesession") == 0) {

            send_msg(LEAVE_SESS, "");

        }

        else if (strncmp(input, "/createsession", 14) == 0) {

            char session[50];
            sscanf(input, "/createsession %s", session);

            send_msg(NEW_SESS, session);

        }

        else if (strcmp(input, "/list") == 0) {

            send_msg(QUERY, "");

        }

        else if (strcmp(input, "/quit") == 0) {

            send_msg(EXIT, "");
            close(sockfd);
            exit(0);

        }

        else {

            send_msg(MESSAGE, input);

        }
    }

    return 0;
}
