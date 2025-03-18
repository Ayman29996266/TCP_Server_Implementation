#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DEFAULT_IP "127.0.0.1"
#define DEFAULT_PORT 42999
#define BUFFER_SIZE 1024

#ifdef _WIN32
    #include <winsock2.h>
    #include <windows.h>
    #pragma  comment(lib, "ws2_32.lib")
    typedef SOCKET socket_t;
#else
    #include <unistd.h>
    #include <arpa/inet.h>
    typedef int socket_t;
#endif


void close_socket(socket_t *s) {
    if (*s > 0) {
        shutdown(*s, SHUT_RDWR);
        #ifdef _WIN32
            closesocket(*s);
        #else
            close(*s);
        #endif
        *s = -1;
    }
}


int main(int argc, char *argv[]) {
    socket_t sock;
    struct sockaddr_in server_address;
    char buffer[BUFFER_SIZE];


    // initialize windows socket
    #ifdef _WIN32
        WSADATA wsa;
        if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
            printf("Error: Winsock initialization failed\n");
            return 1;
        }
    #endif


    /* Get IP and port from command-line args (default: 127.0.0.1:42999) */
    const char *server_ip = (argc > 1) ? argv[1] : DEFAULT_IP;

    int port = DEFAULT_PORT;
    if (argc > 2) {
        char *endptr;
        long temp = strtol(argv[2], &endptr, 10);
        if (*endptr == '\0' && temp > 0 && temp <= 65535) {
            port = (int)temp;
        } else {
            fprintf(stderr, "Error: Invalid port number\n");
            
            #ifdef _WIN32
                WSACleanup();           // requiarement for windows sockets
            #endif

            exit(1);
        }
    }

    /* Create socket */
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        perror("Error: Couldn't create socket");

        #ifdef _WIN32
            WSACleanup();
        #endif

        exit(1);
    }

   
    /* Set up server address */
    memset(&server_address, 0, sizeof(server_address));     // empty buffer
    server_address.sin_family = AF_INET;                    // for IPv4
    server_address.sin_port = htons(port);                  // set port

   
    /* Convert IP address */
    if (inet_pton(AF_INET, server_ip, &server_address.sin_addr) <= 0) {
        fprintf(stderr, "Error: Failed to convert IP address '%s'.\n", server_ip);
        
        close_socket(& sock);
        
        #ifdef _WIN32               // windows compatibility 
            WSACleanup();
        #endif
        exit(1);
    }

    
    /* Connect to server */
    #ifdef _WIN32
        sockaddr *addr = (sockaddr *) &server_address;
    #else
        struct sockaddr *addr = (struct sockaddr *) &server_address;
    #endif

    if (connect(sock, addr, sizeof(server_address)) == -1) {
        perror("Error: Connection failed");
        
        close_socket(& sock);
        
        #ifdef _WIN32               // windows compatibility 
            WSACleanup();
        #endif
        exit(1);
    }

    printf("Connected to server at %s:%d\n", server_ip, port);

    
    /* Receive initial message from server */
    int bytes_received = recv(sock, buffer, BUFFER_SIZE - 1, 0);
    if (bytes_received > 0) {
        buffer[bytes_received] = '\0';
        printf("Server > %s\n", buffer);
    }

    
    /* Communication loop */
    while (1) {
        printf("You > ");
        if (fgets(buffer, BUFFER_SIZE, stdin) == NULL) {
            printf("\nInput error. Closing connection...\n");
            break;
        }
        
        
        
        /* Remove trailing newline */
        buffer[strcspn(buffer, "\n")] = 0;

        
        /* Exit on user command */
        if (strcmp(buffer, "exit") == 0) {
            printf("Closing connection...\n");
            break;
        }

        
        /* Send message to server */
        send(sock, buffer, strlen(buffer), 0);

        
        /* Receive server response */
        bytes_received = recv(sock, buffer, BUFFER_SIZE - 1, 0);
        if (bytes_received > 0) {
            buffer[bytes_received] = '\0';
            printf("Server > %s\n", buffer);
        } else {
            printf("Server closed the connection.\n");
            break;
        }
    }
    
    close_socket(& sock);

    #ifdef _WIN32
        WSACleanup();
    #endif
    return 0;
}
