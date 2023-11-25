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
#include "include/admin_commands.h"

#define PORT 331
#define MAX_CLIENTS 1000
#define BUFFER_SIZE 1024


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

void add_user_from_string(const char* user_pass) {
    char* user = my_strdup(user_pass);
    char* pass = strchr(user, ':');
    if (pass == NULL) {
        fprintf(stderr, "Invalid user:pass format: %s\n", user_pass);
        exit(EXIT_FAILURE);
    }
    *pass = '\0';
    pass++;
    add_user(user, pass);
    free(user);
}

ssize_t read_line(int fd, char *buffer, size_t max_length) {
    ssize_t total_read = 0;
    char ch;
    ssize_t read_count;

    while (total_read < (ssize_t)(max_length - 1)) {
        read_count = read(fd, &ch, 1);

        if (read_count == -1) {
            // Handle the read error
            if (errno == EINTR) {
                // Interrupted, try again
                continue;
            } else {
                // Other errors
                return -1;
            }
        } else if (read_count == 0) {
            // No more data
            if (total_read == 0) {
                return 0; // No data read
            } else {
                break; // Some data was read
            }
        } else {
            // Append the character if it's not a newline
            if (ch == '\n') {
                break; // End of line
            }
            buffer[total_read++] = ch;
        }
    }

    buffer[total_read] = '\0'; // Null-terminate the string
    return total_read;
}

void parse_options(int argc, char *argv[], ServerConfig *config) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-p") == 0 && i + 1 < argc) {
            config->ipv4_port = atoi(argv[++i]);
            if (!is_valid_port(config->ipv4_port)) {
                fprintf(stderr, "Invalid IPv4 & IPv6 port number: %d\n", config->ipv4_port);
                exit(EXIT_FAILURE);
            }
        } else if (strcmp(argv[i], "-P") == 0 && i + 1 < argc) {
            config->client_port = atoi(argv[++i]);
            if (!is_valid_port(config->client_port)) {
                fprintf(stderr, "Invalid client admin port number: %d\n", config->client_port);
                exit(EXIT_FAILURE);
            }
        } else if (strcmp(argv[i], "-d") == 0 && i + 1 < argc) {
            free(config->mail_dir); // Free existing mail_dir
            config->mail_dir = my_strdup(argv[++i]);
        } else if (strcmp(argv[i], "-u") == 0 && i + 1 < argc) {
            add_user_from_string(argv[++i]);
        } else {
            fprintf(stderr, "Usage: %s [-p IPv4_port] [-P client_port] [-d mail_directory] [-t transform_command] [-u user:pass]\n", argv[0]);
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
        memset(client->deleted_files, 0, sizeof(client->deleted_files));
        client->delete_size = 0;
        client->fd = 0;
    }
}

int main(int argc, char *argv[]) {
    int server_fd, client_fd, new_socket, max_sd, activity, i, valread;
    struct sockaddr_in6 server_addr, client_addr;
    socklen_t addrlen = sizeof(server_addr);
    char addr_str[INET6_ADDRSTRLEN];
    client_state clients[MAX_CLIENTS] = {0};

    ServerConfig *server_config = get_server_config();
    server_config->mail_dir = my_strdup("src/maildir/");
    ServerStatus *server_status = get_server_status();

    initialize_users();
    parse_options(argc, argv, server_config);

    // Initialize server socket
    if ((server_fd = socket(AF_INET6, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt)) < 0) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin6_family = AF_INET6;
    server_addr.sin6_addr = in6addr_any;
    server_addr.sin6_port = htons(server_config->ipv4_port);

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, MAX_CLIENTS) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    // Initialize client socket
    if ((client_fd = socket(AF_INET6, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    if (setsockopt(client_fd, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt)) < 0) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    memset(&client_addr, 0, sizeof(client_addr));
    client_addr.sin6_family = AF_INET6;
    client_addr.sin6_addr = in6addr_any;
    client_addr.sin6_port = htons(server_config->client_port);

    if (bind(client_fd, (struct sockaddr *)&client_addr, sizeof(client_addr)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(client_fd, MAX_CLIENTS) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    printf("Server started\n");
    log_info("IPv4 & IPv6  port: %d", server_config->ipv4_port);
    log_info("Client admin port: %d", server_config->client_port);
    log_info("Mail directory: %s", server_config->mail_dir);

    puts("Waiting for connections ...");

    fd_set readfds, writefds;
    while (true) {
        FD_ZERO(&readfds);
        FD_ZERO(&writefds);
        max_sd = (server_fd > client_fd) ? server_fd : client_fd;

        FD_SET(server_fd, &readfds);
        FD_SET(client_fd, &readfds);

        // Add client sockets to set
        for (i = 0; i < MAX_CLIENTS; i++) {
            int client_sock = clients[i].fd;
            if (client_sock > 0) {
                FD_SET(client_sock, &readfds);
                FD_SET(client_sock, &writefds);
            }
            if (client_sock > max_sd) {
                max_sd = client_sock;
            }
        }

        activity = select(max_sd + 1, &readfds, &writefds, NULL, NULL);

        if ((activity < 0) && (errno != EINTR)) {
            perror("select error");
            exit(EXIT_FAILURE);
        }

        if (FD_ISSET(server_fd, &readfds)) {
            if ((new_socket = accept(server_fd, (struct sockaddr *)&server_addr, &addrlen)) < 0) {
                perror("accept");
                exit(EXIT_FAILURE);
            }

            inet_ntop(AF_INET6, &server_addr.sin6_addr, addr_str, sizeof(addr_str));
            printf("New connection, socket fd is %d, ip is: %s, port: %d\n", new_socket, addr_str, ntohs(server_addr.sin6_port));

            server_status->total_connections++;
            server_status->current_connections++;

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

        // Handle new admin connections
        if (FD_ISSET(client_fd, &readfds)) {
            if ((new_socket = accept(client_fd, (struct sockaddr *)&client_addr, &addrlen)) < 0) {
                perror("accept");
                exit(EXIT_FAILURE);
            }

            inet_ntop(AF_INET6, &client_addr.sin6_addr, addr_str, sizeof(addr_str));
            printf("New admin connection, socket fd is %d, ip is: %s, port: %d\n", new_socket, addr_str, ntohs(client_addr.sin6_port));

            server_status->total_connections++;
            server_status->current_connections++;

            // Authenticate the admin user
            char buffer[BUFFER_SIZE];
            memset(buffer, 0, BUFFER_SIZE);
            int bytes_read = read(new_socket, buffer, BUFFER_SIZE); // Reading password

            if (bytes_read <= 0) {
                // Connection closed or error
                close(new_socket);
                server_status->current_connections--;
                continue;
            }

            // TODO: Implement the function to authenticate
            int auth_result = is_admin(buffer); // Replace with actual function
            send_response(new_socket, auth_result == 0 ? "+OK\r\n" : "-ERR\r\n");

            if (auth_result == 0) {
                // Enter a loop to handle continuous commands after successful authentication
                while (true) {
                    memset(buffer, 0, BUFFER_SIZE);
                    bytes_read = read_line(new_socket, buffer, BUFFER_SIZE); // Reading command

                    if (bytes_read <= 0) {
                        // Connection closed or error
                        break;
                    }

                    // Process the command
                    int process_result = process_admin_command(buffer, new_socket); // Replace with actual function

                    if (process_result != 0) {
                        // Error or termination signal
                        break;
                    }
                }
            }

            close(new_socket);
            server_status->current_connections--;
        }

        // Handle client communications
        for (i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i].fd > 0 && FD_ISSET(clients[i].fd, &readfds)) {
                valread = read(clients[i].fd, &clients[i].read_buffer[clients[i].read_buffer_pos], 1);
                if (valread > 0) {
                    server_status->bytes_transmitted += valread;
                    clients[i].read_buffer_pos += valread;
                    // Check for newline to process the command
                    if (clients[i].read_buffer[clients[i].read_buffer_pos - 1] == '\n') {
                        // Process the command when a newline is received
                        clients[i].read_buffer[clients[i].read_buffer_pos] = '\0'; // Null-terminate the command
                        int process_result = process_pop3_command(&clients[i]);
                        if (process_result == -1) {
                            // Disconnect if process_pop3_command returns -1
                            log_disconnection(&clients[i]);
                            close(clients[i].fd);
                            reset_client_state(&clients[i]);
                            server_status->current_connections--;
                            continue; // Move to the next client
                        }
                        memset(clients[i].read_buffer, 0, BUFFER_SIZE);
                        clients[i].read_buffer_pos = 0;
                    }
                } else if (valread == 0 || errno == ECONNRESET) {
                    // Client disconnected or connection reset by peer
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

    // Clean up
    free(server_config->mail_dir);
    close(server_fd);
    close(client_fd);
    return 0;
}
