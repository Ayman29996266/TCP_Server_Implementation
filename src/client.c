#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DEFAULT_IP "127.0.0.1"
#define DEFAULT_PORT 42999
#define BUFFER_SIZE 1024

#ifdef _WIN32
    #include <winsock2.h>
    #include <windows.h>
    #pragma comment(lib, "ws2_32.lib")
    typedef SOCKET socket_t;
#else
    #include <unistd.h>
    #include <arpa/inet.h>
    typedef int socket_t;
#endif

// Function to gracefully close a socket. Handles differences between Windows and Linux.
void close_socket(socket_t *s) {
    if (*s > 0) {
        shutdown(*s, SHUT_RDWR); // Stop any ongoing transfer on the socket (optional but recommended).
        #ifdef _WIN32
            closesocket(*s); // Windows specific function to close a socket.
        #else
            close(*s);       // Linux/Unix specific function to close a file descriptor (which includes sockets).
        #endif
        *s = -1; // Explicitly set the socket descriptor to an invalid value to indicate it's closed.
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
            return -1; // Indicate failure if send encounters an error.
        }
        total_sent += bytes_sent; // Increment the total number of bytes sent.
    }
    return 0; // Return 0 to indicate successful transmission of the entire buffer.
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
                printf("Server closed the connection.\n");
            }
            return -1; // Indicate failure (error or connection closed).
        }
        total_received += bytes_received; // Increment the total number of bytes received.
    }
    return 0; // Return 0 to indicate successful reception of the entire expected data.
}

int main(int argc, char *argv[]) {
    socket_t sock; // Socket descriptor. In Windows, it's a SOCKET type; in Linux, it's an int.
    struct sockaddr_in server_address; // Structure to hold the server's IP address and port.
    char buffer[BUFFER_SIZE]; // Buffer to hold data being sent or received.

    /* initialize windows socket */
    #ifdef _WIN32
        WSADATA wsa; // Structure to hold Windows Sockets startup information.
        if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) { // Initialize Winsock library (version 2.2).
            printf("Error: Winsock initialization failed\n");
            return 1; // Indicate an error during Winsock initialization.
        }
    #endif

    /* Get IP and port from command-line args (default: 127.0.0.1:42999) */
    const char *server_ip = (argc > 1) ? argv[1] : DEFAULT_IP; // If an IP is provided as a command-line argument, use it; otherwise, use the default IP.

    int port = DEFAULT_PORT; // Initialize port with the default value.
    if (argc > 2) { // Check if a port number is provided as a command-line argument.
        char *endptr;
        long temp = strtol(argv[2], &endptr, 10); // Convert the port argument from string to long.
        if (*endptr == '\0' && temp > 0 && temp <= 65535) { // Check if the conversion was successful and the port is within the valid range (1-65535).
            port = (int)temp; // Assign the converted port number.
        } else {
            fprintf(stderr, "Error: Invalid port number\n");

            #ifdef _WIN32
                WSACleanup(); // Clean up Winsock resources before exiting (Windows specific).
            #endif

            exit(1); // Exit the program due to an invalid port number.
        }
    }

    /* Create socket */
    sock = socket(AF_INET, SOCK_STREAM, 0); // Create a socket: AF_INET for IPv4, SOCK_STREAM for TCP, 0 for default protocol.
    if (sock == -1) {
        perror("Error: Couldn't create socket"); // Print an error message if socket creation fails.

        #ifdef _WIN32
            WSACleanup(); // Clean up Winsock resources on error (Windows specific).
        #endif

        exit(1); // Exit the program if socket creation failed.
    }

    /* Set up server address */
    memset(&server_address, 0, sizeof(server_address)); // Initialize the server address structure with zeros.
    server_address.sin_family = AF_INET; // Set the address family to IPv4.
    server_address.sin_port = htons(port); // Convert the port number to network byte order and set it.

    /* Convert IP address */
    if (inet_pton(AF_INET, server_ip, &server_address.sin_addr) <= 0) { // Convert the server IP address from string to binary form.
        fprintf(stderr, "Error: Failed to convert IP address '%s'.\n", server_ip);

        close_socket(&sock); // Close the socket if IP conversion fails.

        #ifdef _WIN32
            WSACleanup(); // Clean up Winsock resources on error (Windows specific).
        #endif
        exit(1); // Exit the program if IP address conversion failed.
    }

    /* Connect to server */
    #ifdef _WIN32
        sockaddr *addr = (sockaddr *) &server_address; // Cast the server address structure to a generic sockaddr pointer (Windows).
    #else
        struct sockaddr *addr = (struct sockaddr *) &server_address; // Cast the server address structure to a generic sockaddr pointer (Linux).
    #endif

    if (connect(sock, addr, sizeof(server_address)) == -1) { // Attempt to establish a connection with the server.
        perror("Error: Connection failed"); // Print an error message if the connection fails.

        close_socket(&sock); // Close the socket if the connection failed.

        #ifdef _WIN32
            WSACleanup(); // Clean up Winsock resources on error (Windows specific).
        #endif
        exit(1); // Exit the program if the connection failed.
    }

    printf("Connected to server at %s:%d\n", server_ip, port);

    /* Receive initial message from server */
    int bytes_received = recv(sock, buffer, BUFFER_SIZE - 1, 0); // Receive the initial message from the server (using the original recv, assuming it's short).
    if (bytes_received > 0) {
        buffer[bytes_received] = '\0'; // Null-terminate the received data to make it a valid C string.
        printf("Server > %s\n", buffer); // Print the initial message from the server.
    }

    /* Communication loop */
    while (1) {
        printf("You > ");
        if (fgets(buffer, BUFFER_SIZE, stdin) == NULL) { // Read input from the user.
            printf("\nInput error. Closing connection...\n");
            break; // Exit the loop if there's an input error.
        }

        /* Remove trailing newline */
        buffer[strcspn(buffer, "\n")] = 0; // Remove the newline character that fgets might add.

        /* Exit on user command */
        if (strcmp(buffer, "exit") == 0) { // Check if the user entered the "exit" command.
            printf("Closing connection...\n");
            break; // Exit the communication loop.
        }

        /* Send message to server (using the new send_full function to handle longer messages) */
        if (send_full(sock, buffer, strlen(buffer)) == -1) {
            break; // Exit the loop if sending the message fails.
        }

        /* Receive server response (still using the original recv for simplicity in this client example) */
        bytes_received = recv(sock, buffer, BUFFER_SIZE - 1, 0);
        if (bytes_received > 0) {
            buffer[bytes_received] = '\0'; // Null-terminate the received data.
            printf("Server > %s\n", buffer); // Print the server's response.
        } else {
            printf("Server closed the connection.\n");
            break; // Exit the loop if the server closed the connection or an error occurred during reception.
        }
    }

    close_socket(&sock); // Close the socket when the communication is finished.

    #ifdef _WIN32
        WSACleanup(); // Clean up Winsock resources before exiting (Windows specific).
    #endif
    return 0; // Indicate successful program execution.
}
