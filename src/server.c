#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h> 
/* Libraries needed for the server. */



int main(int argc, char *argv[]) {

    /* Create a socket */
    int server_sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sockfd == -1) {                                  // error handling
        perror("Error... Couldn't initialize a socket.\n");
        exit(1);
    }



    /* A structure to hold the server address and configuration */
    struct sockaddr_in server_address;
    server_address.sin_family       = AF_INET;                   // Set the protocol     (IP)
    server_address.sin_port         = htons(9000);               // Set the port         (9000)
    server_address.sin_addr.s_addr  = htonl(INADDR_ANY);         // Set the IP address   (Local host)
    socklen_t server_socklen        = sizeof(server_address);



    /* Bind the socket to the address*/
    int binding_result = bind(server_sockfd, (struct sockaddr*)&server_address, server_socklen);
    if (binding_result == -1) {                                 // error handling
        perror("Error... Couldn't bind the socket.\n");
        close(server_sockfd);
        exit(1);
    }



    /* Set the limits of the server */
    int listening_result = listen(server_sockfd, 1);
    if (listening_result == -1) {                               // error handling
        perror("Error... An issue occured while listening to the port.\n");
        close(server_sockfd);
        exit(1);
    }



    /* Accept the client connection request and send a greeting message */
    struct sockaddr_in client_address;
    socklen_t client_socklen = sizeof(client_address);
    int  client = accept(server_sockfd, (struct sockaddr*)&client_address, &client_socklen);

    if (client == -1) {                                         // error handling
        perror("Error... Couldn't connect to client.\n");
        close(server_sockfd);
        exit(1);
    }

    char succcessMessage[]  = "connection established successfully.\n";
    send(client, succcessMessage, strlen(succcessMessage), 0);



    /* Initiate the communication cycle */
    char buffer[2048] = {0};                                    // buffer to store incoming messages
    while (1) {
        int bytes_received = recv(client, buffer, 2048 - 1, 0);
        if (bytes_received > 0) {                               // ignore empty messages
            buffer[bytes_received] = '\0';                      // NULL terminate to prevent buffer overflow
            printf("Client > %s\n", buffer);

        } else if (bytes_received == 0) {                       // When client terminates the connection    
            break;
        } else {                                                // error handling
            perror("Error in the 'recv' function.\n");
            close(client);
            close(server_sockfd);
            exit(1);
        }
    }


    /* Terminate the connection */
    close(client);
    close(server_sockfd);

    return 0;
}