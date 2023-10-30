#include "pop3_commands.h"
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>

#define PORT 110
#define MAX_CLIENTS 500

typedef struct {
    int historical_connections;
    int concurrent_connections;
    int transferred_bytes;
} Metrics;

Metrics server_metrics = {0, 0, 0};

void* handle_client(void* arg) {
    int client_socket = *(int*)arg;
    char buffer[4096] = {0};
    char expected_username[128] = {0};  // To store the username from the USER command

    char *welcome_msg = "+OK POP3 server ready\r\n";
    send(client_socket, welcome_msg, strlen(welcome_msg), 0);
    
    while (1) {
        memset(buffer, 0, sizeof(buffer));  // Clear the buffer
        int valread = read(client_socket, buffer, sizeof(buffer) - 1);
        if (valread <= 0) {
            close(client_socket);
            break;
        }

        // Incrementar métricas
        server_metrics.transferred_bytes += valread;

        char* cmd = strtok(buffer, "\r\n");
        while (cmd != NULL) {
            if (strncmp(cmd, "USER", 4) == 0) {
                char* received_username = handle_user_command(client_socket, cmd + 5);
                strncpy(expected_username, received_username, sizeof(expected_username) - 1);
            } else if (strncmp(cmd, "PASS", 4) == 0) {
                if (handle_pass_command(client_socket, expected_username, cmd + 5)) {
                    // Authentication successful, you can set a flag or do further processing
                    printf("User %s authenticated\n", expected_username);
                } else {
                    // Authentication failed. Do not process the next commands in this iteration.
                    break;
                }
            } else if (strncmp(cmd, "LIST", 4) == 0) {
                handle_list_command(client_socket, expected_username);
            } else if (strncmp(cmd, "RETR", 4) == 0) {
                int mail_number = atoi(cmd + 5);
                handle_retr_command(client_socket, expected_username, mail_number);
            } else if (strncmp(cmd, "DELE", 4) == 0) {
                int mail_number = atoi(cmd + 5);
                handle_dele_command(client_socket, expected_username, mail_number);
            } else if (strncmp(cmd, "QUIT", 4) == 0) {
                send(client_socket, "+OK Bye\r\n", 9, 0);
                close(client_socket);
                break;
            } else {
                send(client_socket, "-ERR Command not implemented\r\n", 30, 0);
            }

            cmd = strtok(NULL, "\r\n");
        }
    }

    // Decrementar métricas
    server_metrics.concurrent_connections--;
    free(arg);
    return NULL;
}

int main(void) {
    int server_fd;
    struct sockaddr_in address;
    int addrlen = sizeof(address);

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Failed to create socket");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, MAX_CLIENTS) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    while (1) {
        int* new_socket_ptr = malloc(sizeof(int));
        if ((*new_socket_ptr = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
            perror("Accept failed");
            exit(EXIT_FAILURE);
        }

        server_metrics.historical_connections++;
        server_metrics.concurrent_connections++;

        pthread_t client_thread;
        pthread_create(&client_thread, NULL, handle_client, new_socket_ptr);
        pthread_detach(client_thread); 
    }

    close(server_fd);
    return 0;
}
