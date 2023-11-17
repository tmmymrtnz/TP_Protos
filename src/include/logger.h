#ifndef LOGGER_H
#define LOGGER_H

#include <stdio.h>
#include <time.h>
#include <sys/types.h>
#include "pop3_commands.h"

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

// Function to log a message with a specific level
void log_message(LogLevel level, const char *format, ...);
void log_info(const char *format, ...);
void log_warn(const char *format, ...);
void log_error(const char *format, ...);
void log_debug(const char *format, ...);

ServerStatus *get_server_status(void);

void log_new_connection(int socket_fd, const char *ip, int port);
void log_command_received(client_state *client, const char *command_format, ...);
void log_internal_action(const char *action);
void log_disconnection(client_state *client);

#endif // LOGGER_H
