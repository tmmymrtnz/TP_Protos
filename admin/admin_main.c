#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define MAX_COMMAND_SIZE 1024
#define SERVER_IP "127.0.0.1" // Replace with your server's IP
#define SERVER_PORT 9090     // Replace with your server's port

// Function prototypes
int send_command_to_server(const char * password, const char *command);
int handle_commands(int argc, char *argv[], int sock);
void print_server_response(int sock);
// void receive_response_from_server();

// Command mapping
typedef struct {
    char flag;
    const char* command;
    bool requiresParameter; // Indicates if the command requires an additional parameter
} CommandMapping;

const CommandMapping commandMappings[] = {
    {'h', "HELP", false},
    {'A', "ADD_USER", true}, // Requires username and password
    {'D', "DELETE_USER", true}, // Requires username
    {'R', "RESET_USER_PASSWORD", true}, // Requires username
    {'C', "CHANGE_PASSWORD", true}, // Requires old and new passwords
    {'m', "MAX_USERS", true}, // Requires max users count or none
    {'d', "GET_MAILDIR", false},
    {'M', "SET_MAILDIR", true}, // Requires maildir path
    {'t', "TRANSFORM", true}, // Requires transform command
    {'a', "ALL_CONNEC", false},
    {'c', "CURR_CONNEC", false},
    {'b', "BYTES_TRANS", false},
    {'s', "STATUS", false},
    {'u', "USERS", false},
    // Add other mappings as needed
};

void admin_command_handler(const char *password, int argc, char *argv[]) {
    int sock;
    struct sockaddr_in6 server;

    // Create socket
    sock = socket(AF_INET6, SOCK_STREAM, 0);
    if (sock == -1) {
        perror("Could not create socket");
        exit(EXIT_FAILURE);
    }

    server.sin6_family = AF_INET6;
    server.sin6_addr = in6addr_loopback;
    server.sin6_port = htons(SERVER_PORT);

    // Connect to the server
    if (connect(sock, (struct sockaddr *)&server, sizeof(server)) < 0) {
        perror("Connect failed. Error");
        close(sock);
        exit(EXIT_FAILURE);
    }

    printf("Connected to server at %s:%d\n", SERVER_IP, SERVER_PORT);
    printf("Sending password: %s...\n", password);

 // Create a buffer for the complete command string
    char password_buffer[MAX_COMMAND_SIZE];
    snprintf(password_buffer, MAX_COMMAND_SIZE, "%s\n", password);
    if (send(sock, password_buffer, strlen(password_buffer), 0) < 0) {
        perror("Send failed");
        close(sock);
        exit(EXIT_FAILURE);
    }

    // Wait for password authorization response
    print_server_response(sock);

    // If password is accepted, handle the commands
    if (handle_commands(argc, argv, sock) < 0) {
        perror("Error handling commands");
    }

    close(sock);
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        printf("Usage: ./admin <password> -<command> [<param>] ...\n");
        exit(1);
    }
    admin_command_handler(argv[1], argc, argv);
    return 0;
}

int handle_commands(int argc, char *argv[], int sock) {
    char server_reply[2000];

    for (int i = 2; i < argc; i++) {
        if (argv[i][0] == '-') {
            char commandFlag = argv[i][1];
            bool commandFound = false;

            for (int j = 0; j < (int)(sizeof(commandMappings) / sizeof(CommandMapping)); j++) {
                if (commandMappings[j].flag == commandFlag) {
                    char command[MAX_COMMAND_SIZE];

                    // Construct the command
                    if (commandMappings[j].requiresParameter) {
                        if (i + 1 < argc) {
                            snprintf(command, MAX_COMMAND_SIZE, "%s %s\n", commandMappings[j].command, argv[i + 1]);
                            i++;
                        } else {
                            printf("Missing parameter for -%c command\n", commandFlag);
                            return -1;
                        }
                    } else {
                        snprintf(command, MAX_COMMAND_SIZE, "%s\n", commandMappings[j].command);
                    }

                    // Send the command
                    if (send(sock, command, strlen(command), 0) < 0) {
                        perror("Send failed");
                        return -1;
                    }

                    // Wait for the server's response
                    if (recv(sock, server_reply, 2000, 0) < 0) {
                        perror("recv failed");
                        return -1;
                    } else {
                        printf("Server reply: %s\n", server_reply);
                        //clear the buffer
                        memset(server_reply, 0, sizeof(server_reply));
                    }

                    commandFound = true;
                    break;
                }
            }

            if (!commandFound) {
                printf("Unknown command: -%c\n", commandFlag);
                return -1;
            }
        }
    }

    return 0;
}

void print_server_response(int sock) {
    char server_reply[2000];
    if (recv(sock, server_reply, 2000, 0) < 0) {
        perror("recv failed");
    } else {
        printf("Server reply: %s\n", server_reply);
    }
}