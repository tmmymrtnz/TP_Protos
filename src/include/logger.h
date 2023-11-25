#ifndef LOGGER_H
#define LOGGER_H

#include <stdio.h>
#include <time.h>
#include <sys/types.h>
#include <stdbool.h>
#include "pop3_commands.h"

#define DEFAULT_IPV4_PORT 1110
#define DEFAULT_CLIENT_PORT 9090
#define MAX_CLIENTS 1000
#define BUFFER_SIZE 1024

// Log levels   
typedef enum {
    LOG_DEBUG,
    LOG_INFO,
    LOG_WARN,
    LOG_ERROR
} LogLevel;

typedef struct {
    size_t total_connections;
    size_t current_connections;
    ssize_t bytes_transmitted;
} ServerStatus;

typedef struct {
    char *user;
    char *pass;
    int ipv4_port;
    int client_port;
    char *mail_dir;
    char *transform_command;
    bool transform_enabled;
} ServerConfig;

// Function to log a message with a specific level
void log_message(LogLevel level, const char *format, ...);
void log_info(const char *format, ...);
void log_warn(const char *format, ...);
void log_error(const char *format, ...);
void log_debug(const char *format, ...);

ServerStatus *get_server_status(void);
ServerConfig *get_server_config(void);

void log_new_connection(int socket_fd, const char *ip, int port);
void log_command_received(client_state *client, const char *command_format, ...);
void log_internal_action(const char *action);
void log_disconnection(client_state *client);

#endif // LOGGER_H
