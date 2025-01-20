#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/socket.h>
/* Libraries needed for the client. */



int main(int argc, char *argv[]) {
    
    /* Create a socket */
    int client_sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (client_sockfd == -1) {                                  // error handling, in case the socket creation failed
        perror("Error... Couldn't initialize a socket.\n");
        exit(1);
    }



    /* A structure to hold the server address and configuration */
    char *IP_address = argv[1]? argv[1] : "127.0.0.1";          // IP address can be passed as program argument or defaults to local host
    struct sockaddr_in server_address;
    server_address.sin_family       = AF_INET;                  // Set the protocol     (IP)
    server_address.sin_port         = htons(9000);              // Set the port         (9000)
    server_address.sin_addr.s_addr  = inet_addr(IP_address);    // Set the IP address   (Local host)



    /* Connect the socket to the address */
    int connection_result = connect(client_sockfd, (struct sockaddr*)&server_address, sizeof(server_address));
    
    if (connection_result == -1) {                              // error handling, in case connection failed
        perror("Error... Couldn't connect to the server.\n");
        close(client_sockfd);
        exit(1);
    }



    /* Initiate the communication cycle */
    char buffer[2048]   = {0};                                  // buffer to store incoming messages
    char message[2048]  = {0};
    int  message_length = strlen(message);
    while (1) {
        printf("Enter your message: ");
        scanf("%s", message);
        send(client_sockfd, message, message_length, 0);

        int bytes_received      = recv(client_sockfd, buffer, 2048 -1, MSG_DONTWAIT);
        if (bytes_received < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {                
            } else {
                perror("Error... something wrong happened with 'recv'.");
                close(client_sockfd);
                exit(1);
            }
        } else if (bytes_received > 0) {
            buffer[bytes_received]  = '\0';
            printf("\nServer > %s\n", buffer);
            // buffer[0] = '\0';
        } else {
            break;
        }

        // handle session closing
        if (!strcmp(message, "exit") || !strcmp(message, "bye") || !strcmp(message, "close")) {
            break;
        }
    }


    /* Terminate the connection */
    close(client_sockfd);
    return 0;
}