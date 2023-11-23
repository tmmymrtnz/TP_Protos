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
#include "include/logger.h"

bool is_valid_port(int port) {
    return port > 0 && port <= 65535;
}

char* my_strdup(const char* s) {
    char* new_str = malloc(strlen(s) + 1);
    if (new_str == NULL) {
        perror("Failed to allocate memory");
        exit(EXIT_FAILURE);
    }
    strcpy(new_str, s);
    return new_str;
}

void parse_options(int argc, char *argv[], ServerConfig *config) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-p") == 0 && i + 1 < argc) {
            config->ipv4_port = atoi(argv[++i]);
            if (!is_valid_port(config->ipv4_port)) {
                fprintf(stderr, "Invalid IPv4 port number: %d\n", config->ipv4_port);
                exit(EXIT_FAILURE);
            }
        } else if (strcmp(argv[i], "-P") == 0 && i + 1 < argc) {
            config->ipv6_port = atoi(argv[++i]);
            if (!is_valid_port(config->ipv6_port)) {
                fprintf(stderr, "Invalid IPv6 port number: %d\n", config->ipv6_port);
                exit(EXIT_FAILURE);
            }
        } else if (strcmp(argv[i], "-d") == 0 && i + 1 < argc) {
            config->mail_dir = my_strdup(argv[++i]);
        } else if (strcmp(argv[i], "-t") == 0 && i + 1 < argc) {
            config->transform_command = my_strdup(argv[++i]);
        } else {
            fprintf(stderr, "Usage: %s [-p IPv4_port] [-P IPv6_port] [-d mail_directory] [-t transform_command]\n", argv[0]);
            exit(EXIT_FAILURE);
        }
    }
}

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

int main(int argc, char *argv[]) {
    ServerStatus * server_status = get_server_status();
    ServerConfig * server_config = get_server_config();
    int server_fd_ipv4, server_fd_ipv6, new_socket, max_sd, activity, i, valread;
    struct sockaddr_storage address;
    socklen_t addrlen = sizeof(address);
    char addr_str[INET6_ADDRSTRLEN];
    client_state clients[MAX_CLIENTS] = {0};
    

    parse_options(argc, argv, server_config);

    // Initialize server socket
    if ((server_fd_ipv6 = socket(AF_INET6, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    if ((server_fd_ipv4 = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // Set server socket to allow multiple connections
    int opt = 1;
    if (setsockopt(server_fd_ipv6, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt)) < 0
        || setsockopt(server_fd_ipv4, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt)) < 0) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    // Bind the server socket to the server address
    struct sockaddr_in6 *address6 = (struct sockaddr_in6 *)&address;
    address6->sin6_family = AF_INET6;
    address6->sin6_addr = in6addr_any;
    address6->sin6_port = htons(server_config->ipv6_port);

    if (bind(server_fd_ipv6, (struct sockaddr *)address6, sizeof(*address6)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    // Listen on the server socket
    if (listen(server_fd_ipv6, MAX_CLIENTS) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in *address4 = (struct sockaddr_in *)&address;
    address4->sin_family = AF_INET;
    address4->sin_addr.s_addr = INADDR_ANY;
    address4->sin_port = htons(server_config->ipv4_port);

    if (bind(server_fd_ipv4, (struct sockaddr *)address4, sizeof(*address4)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    // Listen on the server socket
    if (listen(server_fd_ipv4, MAX_CLIENTS) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    initialize_users();

    //print all the config of the server in the log
    log_info("Server configuration:");
    log_info("IPv4 port: %d", server_config->ipv4_port);
    log_info("IPv6 port: %d", server_config->ipv6_port);
    log_info("Mail directory: %s", server_config->mail_dir);
    log_info("Transform command: %s", server_config->transform_command);

    // Accept incoming connections
    puts("Waiting for connections ...");

    fd_set readfds, writefds;
    while (true) {
        FD_ZERO(&readfds);
        FD_ZERO(&writefds);
        max_sd = server_fd_ipv6 > server_fd_ipv4 ? server_fd_ipv6 : server_fd_ipv4;

        FD_SET(server_fd_ipv6, &readfds);
        FD_SET(server_fd_ipv4, &readfds);
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

        if (FD_ISSET(server_fd_ipv4, &readfds)) {
            if ((new_socket = accept(server_fd_ipv4, (struct sockaddr *)&address, &addrlen)) < 0) {
                perror("accept");
                exit(EXIT_FAILURE);
            }

            if (server_status->current_connections >= MAX_CLIENTS) {
                char *message = "-ERR Server is full, try again later.\r\n";
                send_response(new_socket, message);
                close(new_socket);
                log_info("Rejected connection from %s:%d because the server is full", addr_str, ntohs(((struct sockaddr_in *)&address)->sin_port));
            } else {
                server_status->total_connections++;
                server_status->current_connections++;

                inet_ntop(address.ss_family, &((struct sockaddr_in *)&address)->sin_addr, addr_str, sizeof(addr_str));
                log_new_connection(new_socket, addr_str, ntohs(((struct sockaddr_in *)&address)->sin_port));

                printf("New connection, socket fd is %d, ip is: %s, port: %d\n", new_socket, addr_str, ntohs(((struct sockaddr_in6 *)&address)->sin6_port));

                for (i = 0; i < MAX_CLIENTS; i++) {
                    if (clients[i].fd == 0) {
                        clients[i].fd = new_socket;
                        printf("Adding to list of sockets as %d\n", i);

                        // Send welcome message
                        char *message = "+OK POP3 mail.itba.net v4.20 server ready\r\n";
                        send_response(clients[i].fd, message);
                        break;
                    }
                }
            }
        }

        if (FD_ISSET(server_fd_ipv6, &readfds)) {
            if ((new_socket = accept(server_fd_ipv6, (struct sockaddr *)&address, &addrlen)) < 0) {
                perror("accept");
                exit(EXIT_FAILURE);
            }

            if (server_status->current_connections >= MAX_CLIENTS) {
                char *message = "-ERR Server is full, try again later.\r\n";
                send_response(new_socket, message);
                close(new_socket);
                log_info("Rejected connection from %s:%d because the server is full", addr_str, ntohs(((struct sockaddr_in6 *)&address)->sin6_port));
            } else {
                server_status->total_connections++;
                server_status->current_connections++;

                inet_ntop(address.ss_family, &((struct sockaddr_in6 *)&address)->sin6_addr, addr_str, sizeof(addr_str));
                log_new_connection(new_socket, addr_str, ntohs(((struct sockaddr_in6 *)&address)->sin6_port));

                printf("New connection, socket fd is %d, ip is: %s, port: %d\n", new_socket, addr_str, ntohs(((struct sockaddr_in6 *)&address)->sin6_port));

                for (i = 0; i < MAX_CLIENTS; i++) {
                    if (clients[i].fd == 0) {
                        clients[i].fd = new_socket;
                        printf("Adding to list of sockets as %d\n", i);

                        // Send welcome message
                        char *message = "+OK POP3 mail.itba.net v4.20 server ready\r\n";
                        send_response(clients[i].fd, message);
                        break;
                    }
                }
            }
        }

        for (i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i].fd > 0 && FD_ISSET(clients[i].fd, &readfds)) {
                valread = read(clients[i].fd, clients[i].read_buffer + clients[i].read_buffer_pos, sizeof(clients[i].read_buffer) - clients[i].read_buffer_pos);
                if (valread > 0) {
                    server_status->bytes_transmitted += valread;
                    clients[i].read_buffer_pos += valread;
                    // Check if a command was received and handle it
                    if (process_pop3_command(&clients[i]) == -1) {
                        // If process_pop3_command returns -1, it means the client disconnected
                        log_disconnection(&clients[i]);
                        close(clients[i].fd);
                        reset_client_state(&clients[i]);
                        server_status->current_connections--;
                    }
                } else if (valread == 0) {
                    // Client disconnected
                    log_disconnection(&clients[i]);
                    close(clients[i].fd);
                    reset_client_state(&clients[i]);
                    server_status->current_connections--;
                } else {
                    if (errno != EWOULDBLOCK && errno != EAGAIN) {
                        log_disconnection(&clients[i]);
                        close(clients[i].fd);
                        reset_client_state(&clients[i]);
                        server_status->current_connections--;
                    }
                }
            }
        }
    }

    close(server_fd_ipv4);
    close(server_fd_ipv6);
    return 0;
}
