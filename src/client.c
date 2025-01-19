#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>


int main(int argc, char *argv[]) {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_port   = htons(9000);

    inet_pton(AF_INET, argv[1], &address.sin_addr);

    connect(sockfd, (struct sockaddr*)&address, sizeof(address));

    char buffer[2048] = {0};
    char message[2048] = {0};
    while (1) {
        printf("Enter your message: ");
        scanf("%s", message);
        send(sockfd, message, strlen(message), 0);
        read(sockfd, buffer, 2048 -1);

        if (!strcmp(message, "exit") || !strcmp(message, "bye")) {
            break;
        }
    }

    close(sockfd);
    return 0;
}