#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#define DEFAULT_PORT 42999
#define BUFFER_SIZE 1024
#define BACKLOG 2                                         // <- maximum number of clients to accept

#ifdef _WIN32                                             // <- windows/Linux compatibility
    #include <winsock2.h>
    #pragma comment(lib, "ws2_32.lib")
    typedef SOCKET socket_t;
#else
    #include <unistd.h>
    #include <signal.h>
    #include <arpa/inet.h>
    #include <netinet/in.h>

    typedef int socket_t;
#endif

// Function to gracefully close a socket. Handles differences between Windows and Linux.
void close_socket(socket_t *s) {
    if (*s > 0) {
        shutdown(*s, SHUT_RDWR);                         // <- stop any ongoing transfer on the socket (optional)
        #ifdef _WIN32
            closesocket(*s);
        #else
            close(*s);
        #endif
        *s = -1;                                        // <- explicitly set the socket to negative value, means closed
    }
}

// Function to send the entire buffer over the socket, handling potential partial sends.
int send_full(socket_t sock, const char *buffer, size_t len) {
    ssize_t total_sent = 0;
    ssize_t bytes_sent;

    while (total_sent < len) {
        bytes_sent = send(sock, buffer + total_sent, len - total_sent, 0);
        if (bytes_sent == -1) {
            perror("Error: send failed");
            return -1;                                   // Indicate failure if send encounters an error.
        }
        total_sent += bytes_sent;                       // Increment the total number of bytes sent.
    }
    return 0;                                          // Return 0 to indicate successful transmission of the entire buffer.
}

// Function to receive the entire expected length of data from the socket, handling potential partial receives.
int recv_full(socket_t sock, char *buffer, size_t len) {
    ssize_t total_received = 0;
    ssize_t bytes_received;

    while (total_received < len) {
        bytes_received = recv(sock, buffer + total_received, len - total_received, 0);
        if (bytes_received <= 0) {
            if (bytes_received == -1) {
                perror("Error: recv failed");
            } else {
                printf("Client disconnected.\n");
            }
            return -1;                                   // Indicate failure (error or connection closed).
        }
        total_received += bytes_received;               // Increment the total number of bytes received.
    }
    return 0;                                          // Return 0 to indicate successful reception of the entire expected data.
}

// Function to handle communication with a connected client.
// It receives messages from the client and sends back an acknowledgment.
void handle_client(socket_t client_sock) {                 // <- should call this function for every client
    char buffer[BUFFER_SIZE];
    const char *successMessage = "Connection established successfully.\n";
    const char *acknowledgment = "Message received.\n";

    // Update: Sending the initial success message using send_full
    if (send_full(client_sock, successMessage, strlen(successMessage)) == -1) {
        perror("Error sending success message");
        close_socket(&client_sock);
        return;
    }

    while (1) {
        int bytes_received = recv(client_sock, buffer, BUFFER_SIZE - 1, 0); // <- get client messages

        if (bytes_received > 0) {
            buffer[bytes_received] = '\0';             // <- null terminate
            printf("Client > %s\n", buffer);
            // Update: Sending the acknowledgment using send_full
            if (send_full(client_sock, acknowledgment, strlen(acknowledgment)) == -1) {
                perror("Error sending acknowledgment");
                break;
            }
        } else if (bytes_received == 0) {                 // <- means client disconnected
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

    close_socket(&client_sock);                         // <- make sure socket is closed after client disconnects
}

#ifdef _WIN32
/* Handle termination signals
can be used for a pre-termination cleanup */
BOOL WINAPI signal_handler(DWORD sig) {
    if (sig == CTRL_C_EVENT || sig == CTRL_BREAK_EVENT || sig == CTRL_CLOSE_EVENT ||     // <- all windows termination signals
        sig == CTRL_LOGOFF_EVENT || sig == CTRL_SHUTDOWN_EVENT) {

        printf("\nShutting down server...\n");
        close_socket(&server_sock);
        WSACleanup();                                   // <- winsock requirement
        exit(0);
    }
    return TRUE;                                       // <- Indicates we handled signal, or ignored it intentionally
}
#else
/* Handle termination signals
can be used for a pre-termination cleanup */
void signal_handler(int sig) {
    printf("\nShutting down server...\n");
    exit(0);
}
#endif

int server_sock; // Global variable for the server socket to be accessible in the signal handler (Windows).

int main(int argc, char *argv[]) {

    int client_sock;
    struct sockaddr_in server_address, client_address;
    socklen_t client_addr_len = sizeof(client_address);

    /* Determine port from command-line argument or use default */
    int port = DEFAULT_PORT;
    if (argc < 2) {
        printf("No port specified, using default: %d\n", DEFAULT_PORT);
    } else {
        char *endptr;
        long temp_port = strtol(argv[1], &endptr, 10);     // <- convert string to long (port)
        if (*endptr != '\0' || temp_port <= 0 || temp_port > 65535) {
            fprintf(stderr, "Invalid port number. Using default: %d\n", DEFAULT_PORT);
        } else {
            port = (int)temp_port;
        }
    }

    /* Initialize windows socket (Winsock) if on Windows */
    #ifdef _WIN32
        WSADATA wsa;
        if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
            fprintf(stderr, "Error: Winsock initialization failed\n");
            return 1;
        }
    #endif

    /* Handle termination signals for graceful shutdown */
    #ifdef _WIN32
        SetConsoleCtrlHandler(signal_handler, TRUE);
    #else
        signal(SIGINT, signal_handler);
        signal(SIGTERM, signal_handler);
    #endif

    /* Create the server socket */
    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock == -1) {
        perror("Error: Couldn't initialize socket");

        #ifdef _WIN32
            WSACleanup();                                   // <- winsock requirement, before exiting the program (ONLY ONCE)
        #endif

        exit(1);
    }

    /* Set up the server address structure */
    memset(&server_address, 0, sizeof(server_address));     // <- initialize the structure with zeros
    server_address.sin_family = AF_INET;                     // <- specify IPv4 address family
    server_address.sin_port = htons(port);                     // <- convert port to network byte order
    server_address.sin_addr.s_addr = INADDR_ANY;             // <- listen on all available network interfaces

    /* Set socket options - SO_REUSEADDR allows reusing the port if the server was recently closed */
    int opt = 1;
    if (setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("Error: setsockopt failed");
        close_socket(&server_sock);

        #ifdef _WIN32
            WSACleanup();
        #endif
        exit(1);
    }

    /* Bind the socket to the specified address and port */
    if (bind(server_sock, (struct sockaddr *)&server_address, sizeof(server_address)) == -1) {
        perror("Error: Couldn't bind socket");

        close_socket(&server_sock);

        #ifdef _WIN32
            WSACleanup();
        #endif
        exit(1);
    }

    /* Start listening for incoming client connections */
    if (listen(server_sock, BACKLOG) == -1) {
        perror("Error: Listening failed");

        close_socket(&server_sock);

        #ifdef _WIN32
            WSACleanup();
        #endif
        exit(1);
    }

    printf("Server is running on port %d...\n", port);

    /* Main loop to accept incoming client connections */
    while (1) {

        /* Accept a new client connection */
        client_sock = accept(server_sock, (struct sockaddr *)&client_address, &client_addr_len);
        if (client_sock == -1) {                             // handle potential accept errors
            #ifndef _WIN32
                if (errno == EINTR) continue;               // Interrupted system call, try again
            #endif
            perror("Error: Failed to accept client");
            continue;                                      // Continue listening for other clients
        }

        /* Log the new connection */
        printf("New connection from %s:%d\n",
               inet_ntoa(client_address.sin_addr),           // <- convert client IP to string format
               ntohs(client_address.sin_port));             // <- convert client port to host byte order

        /* Handle the client connection in the handle_client function */
        handle_client(client_sock);
        // Note: For concurrent handling of multiple clients, this handle_client
        // call should ideally be done in a new thread or process.
    }

    /* Close the server socket */
    close_socket(&server_sock);

    /* Clean up Winsock resources if on Windows */
    #ifdef _WIN32
        WSACleanup();
    #endif
    return 0;
}

