#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>


int main(int argc, char *argv[]) {

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in address;
    address.sin_family      = AF_INET;
    address.sin_port        = htons(9000);
    address.sin_addr.s_addr = INADDR_ANY;

    socklen_t socketLength = (socklen_t)sizeof(address);

    bind(sockfd, (struct sockaddr*)&address, socketLength);
    listen(sockfd, 1);

    int client = accept(sockfd, (struct sockaddr*)&address, &socketLength);

    char succcessMessage[] = "connection established successfully.";
    send(client, succcessMessage, strlen(succcessMessage), 0);

    char buffer[2048] = {0};
    char message[] = "message was recieved.";

    while (1) {
        read(client, buffer, 2048 - 1);
        buffer[strlen(buffer)] = '\0';

        send(client, message, strlen(message), 0);

        if (!strcmp(buffer, "exit") || !strcmp(buffer, "bye")) {
            break;
        }
    }

    close(sockfd);
    close(client);
    return 0;
}