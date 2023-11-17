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
#include "include/user.h"

#define PORT 110
#define MAX_CLIENTS 1000
#define BUFFER_SIZE 1024

void reset_client_state(client_state *client) {
    if (client) {
        if (client->authenticated) {
            reset_user_connection(client->username);
        }
        memset(client->read_buffer, 0, BUFFER_SIZE);
        memset(client->write_buffer, 0, BUFFER_SIZE);
        client->read_buffer_pos = 0;
        client->write_buffer_pos = 0;
        client->authenticated = false;
        memset(client->username, 0, sizeof(client->username));
        memset(client->deleted_messages, 0, sizeof(client->deleted_messages));
        client->fd = 0;
    }
}

int main(void) {
    int server_fd, new_socket, max_sd, activity, i, valread;
    struct sockaddr_storage address;
    socklen_t addrlen = sizeof(address);
    char addr_str[INET6_ADDRSTRLEN];
    client_state clients[MAX_CLIENTS] = {0};

    // Initialize server socket
    if ((server_fd = socket(AF_INET6, SOCK_STREAM, 0)) == 0) {
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
    struct sockaddr_in6 *address6 = (struct sockaddr_in6 *)&address;
    address6->sin6_family = AF_INET6;
    address6->sin6_addr = in6addr_any;
    address6->sin6_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)address6, sizeof(*address6)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    // Listen on the server socket
    if (listen(server_fd, MAX_CLIENTS) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    initializeUsers();

    // Accept incoming connections
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

            inet_ntop(address.ss_family, &((struct sockaddr_in6 *)&address)->sin6_addr, addr_str, sizeof(addr_str));
            printf("New connection, socket fd is %d, ip is: %s, port: %d\n", new_socket, addr_str, ntohs(((struct sockaddr_in6 *)&address)->sin6_port));

            for (i = 0; i < MAX_CLIENTS; i++) {
                if (clients[i].fd == 0) {
                    clients[i].fd = new_socket;
                    printf("Adding to list of sockets as %d\n", i);

                    // Send welcome message
                    char *message = "+OK POP3 mail.itba.net v4.20 server ready\r\n";
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
