#include "pop3_commands.h"
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdbool.h>
#include <errno.h>
#include <sys/select.h>

#define PORT 110
#define MAX_CLIENTS 1000
#define BUFFER_SIZE 1024

void reset_client_state(client_state *client) {
    if (client) {
        memset(client->read_buffer, 0, BUFFER_SIZE);
        memset(client->write_buffer, 0, BUFFER_SIZE);
        client->read_buffer_pos = 0;
        client->write_buffer_pos = 0;
        client->authenticated = false;
        memset(client->username, 0, sizeof(client->username));
        client->fd = 0;
    }
}

int main(void) {
    int server_fd, new_socket, max_sd, activity, i, valread;
    struct sockaddr_in address;
    socklen_t addrlen = sizeof(address);
    client_state clients[MAX_CLIENTS] = {0};

    // Initialize server socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // Set server socket to allow multiple connections
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt)) < 0) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    // Bind the server socket to the server address
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    // Listen on the server socket
    if (listen(server_fd, MAX_CLIENTS) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    // Accept incoming connection
    puts("Waiting for connections ...");

    fd_set readfds, writefds;
    while (true) {
        FD_ZERO(&readfds);
        FD_ZERO(&writefds);
        max_sd = server_fd;

        FD_SET(server_fd, &readfds);
        for (i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i].fd > 0) {
                FD_SET(clients[i].fd, &readfds);
                if (clients[i].fd > max_sd) {
                    max_sd = clients[i].fd;
                }
            }
        }

        activity = select(max_sd + 1, &readfds, &writefds, NULL, NULL);

        if ((activity < 0) && (errno != EINTR)) {
            perror("select error");
            exit(EXIT_FAILURE);
        }

        if (FD_ISSET(server_fd, &readfds)) {
            if ((new_socket = accept(server_fd, (struct sockaddr *)&address, &addrlen)) < 0) {
                perror("accept");
                exit(EXIT_FAILURE);
            }

            printf("New connection, socket fd is %d, ip is: %s, port: %d\n", new_socket, inet_ntoa(address.sin_addr), ntohs(address.sin_port));

            for (i = 0; i < MAX_CLIENTS; i++) {
                if (clients[i].fd == 0) {
                    clients[i].fd = new_socket;
                    printf("Adding to list of sockets as %d\n", i);

                    // Send welcome message
                    char *message = "Welcome to the POP3 server. Please log in.\r\n";
                    send(new_socket, message, strlen(message), 0);
                    break;
                }
            }
        }

        for (i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i].fd > 0 && FD_ISSET(clients[i].fd, &readfds)) {
                valread = read(clients[i].fd, clients[i].read_buffer + clients[i].read_buffer_pos, sizeof(clients[i].read_buffer) - clients[i].read_buffer_pos);
                if (valread > 0) {
                    clients[i].read_buffer_pos += valread;
                    // Check if a command was received and handle it
                    if (process_pop3_command(&clients[i]) == -1) {
                        // If process_pop3_command returns -1, it means the client disconnected
                        close(clients[i].fd);
                        reset_client_state(&clients[i]);
                    }
                } else if (valread == 0) {
                    // Client disconnected
                    close(clients[i].fd);
                    reset_client_state(&clients[i]);
                } else {
                    if (errno != EWOULDBLOCK && errno != EAGAIN) {
                        close(clients[i].fd);
                        reset_client_state(&clients[i]);
                    }
                }
            }
        }
    }

    close(server_fd);
    return 0;
}
