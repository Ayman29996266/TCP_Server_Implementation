#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#define DEFAULT_PORT 42999
#define BUFFER_SIZE 1024
#define BACKLOG 2                                                                       // <- maximum number of clients to accept

#ifdef _WIN32                                                                           // <- windows/Linux compatibility
    #include <winsock2.h>
    #pragma  comment(lib, "ws2_32.lib")
    typedef SOCKET socket_t;
#else
    #include <unistd.h>
    #include <signal.h> 
    #include <arpa/inet.h>
    #include <netinet/in.h>

    typedef int socket_t;
#endif


void close_socket(socket_t *s) {                                                        // <- the function to close sockets is different in windows than in Linux
    if (*s > 0) {                                                                       // <- check if socket is actually open
        shutdown(*s, SHUT_RDWR);                                                        // <- stop any ongoing transfer on the socket (optional)
        #ifdef _WIN32
            closesocket(*s);
        #else
            close(*s);
        #endif
        *s = -1;                                                                        // <- explicitly set the socket to negative value, means closed
    }
}



void handle_client(socket_t client_sock) {                                              // <- should call this function for every client
    char buffer[BUFFER_SIZE];
    const char *successMessage = "Connection established successfully.\n";
    const char *acknoledgment  = "Message received.\n";
    
    send(client_sock, successMessage, strlen(successMessage), 0);                       // <- send success message, and make sure it's sent correctly

    while (1) {
        int bytes_received = recv(client_sock, buffer, BUFFER_SIZE - 1, 0);             // <- get client messages
        
        if (bytes_received > 0) {
            buffer[bytes_received] = '\0';                                              // <- null terminate
            printf("Client > %s\n", buffer);
            send(client_sock, acknoledgment, strlen(acknoledgment), 0);
        } else if (bytes_received == 0) {                                               // <- means client disconnected
            printf("Client disconnected.\n");
            break;
        } else {
            #ifdef _WIN32
                fprintf(stderr, "Error: recv() failed with code %d\n", WSAGetLastError());
            #else
                perror("Error: recv() failed");
            #endif
            break;
        }
    }

    close_socket(& client_sock);                                                        // <- make sure socket is closed after client disconnects
}



#ifdef _WIN32
/* Handle termination signals
can be used for a pre-termination cleanup */
BOOL WINAPI signal_handler(DWORD sig) {
    if (sig == CTRL_C_EVENT || sig == CTRL_BREAK_EVENT || sig == CTRL_CLOSE_EVENT ||    // <- all windows termination signals
        sig == CTRL_LOGOFF_EVENT || sig == CTRL_SHUTDOWN_EVENT) {

        printf("\nShutting down server...\n");
        close_socket(server_sock);
        WSACleanup();                                                                   // <- winsock requiarement
        exit(0);
    }
    return TRUE;                                                                        // <- Indicates we handled signal, or ignored it intentionally
}
#else
/* Handle termination signals
can be used for a pre-termination cleanup */
void signal_handler(int sig) {
    printf("\nShutting down server...\n");
    exit(0);
}
#endif


int main(int argc, char *argv[]) {

    int server_sock, client_sock;
    struct sockaddr_in server_address, client_address;
    socklen_t client_addr_len = sizeof(client_address);


    /* Determine port*/
    int port = DEFAULT_PORT;
    if (argc < 2) {
        printf("No port specified, using default: %d\n", DEFAULT_PORT);
    } else {
        char *endptr;
        long temp_port = strtol(argv[1], &endptr, 10);                                  // <- convert string to long (port)
        if (*endptr != '\0' || temp_port <= 0 || temp_port > 65535) {
            fprintf(stderr, "Invalid port number. Using default: %d\n", DEFAULT_PORT);
        } else {
            port = (int)temp_port;
        }
    }



    /* Initialize windows socket */                                                     // <- winsock requiarement
    #ifdef _WIN32
        WSADATA wsa;
        if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
            fprintf(stderr, "Error: Winsock initialization failed\n");
            return 1;
        }
    #endif

    
    /* Handle termination signals */
    #ifdef _WIN32
        SetConsoleCtrlHandler(signal_handler, TRUE);
    #else
        signal(SIGINT, signal_handler);
        signal(SIGTERM, signal_handler);
    #endif

    
    /* Create socket */
    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock == -1) {
        perror("Error: Couldn't initialize socket");

        #ifdef _WIN32
            WSACleanup();                                                               // <- winsock requiarement, before exiting the program (ONLY ONCE)
        #endif

        exit(1);
    }

    
    /* Set up server address */
    memset(&server_address, 0, sizeof(server_address));                                 // <- empty the buffer  (optional)
    server_address.sin_family = AF_INET;                                                // <- for IPv4 protocol
    server_address.sin_port = htons(port);                                              // <- set the port
    server_address.sin_addr.s_addr = INADDR_ANY;                                        // <- set to all available interfaces 

    
    /* Set socket options */
    int opt = 1;                                                                        // <- opt 1 allows for port reuse for recent socket reconnect
    if (setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("Error: setsockopt failed");
        close_socket(&server_sock);

        #ifdef _WIN32
            WSACleanup();
        #endif
        exit(1);
    }
    

    /* Bind socket */
    if (bind(server_sock, (struct sockaddr *)&server_address, sizeof(server_address)) == -1) {
        perror("Error: Couldn't bind socket");
        
        close_socket(& server_sock);
        
        #ifdef _WIN32
            WSACleanup();
        #endif
        exit(1);
    }

    
    /* Start listening */
    if (listen(server_sock, BACKLOG) == -1) {
        perror("Error: Listening failed");
        
        close_socket(& server_sock);
        
        #ifdef _WIN32
            WSACleanup();
        #endif
        exit(1);
    }

    printf("Server is running on port %d...\n", port);

    
    while (1) {
        
        /* Accept client connection */
        client_sock = accept(server_sock, (struct sockaddr *)&client_address, &client_addr_len);
        if (client_sock == -1) {                                                        // reconnect to socket it it fails

            #ifndef _WIN32
                if (errno == EINTR) continue;               
            #endif

            perror("Error: Failed to accept client");
            continue;
        }

        printf("New connection from %s:%d\n",
               inet_ntoa(client_address.sin_addr),                                      // <- client address
               ntohs(client_address.sin_port));                                         // <- client port

        
        /* Handle client (should be in a new thread) */
        handle_client(client_sock);
    }

    
    close_socket(& server_sock);
    
    #ifdef _WIN32
        WSACleanup();
    #endif
    return 0;
}

