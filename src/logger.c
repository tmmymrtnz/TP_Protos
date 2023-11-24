#include "include/logger.h"
#include <stdarg.h>
#include "include/pop3_commands.h"

static ServerStatus server_status = {
    .total_connections = 0,
    .current_connections = 0,
    .bytes_transmitted = 0
};

static ServerConfig server_config = {
    .ipv4_port = DEFAULT_IPV4_PORT,
    .ipv6_port = DEFAULT_IPV6_PORT,
    .mail_dir = "src/maildir/",
    .transform_command = "cat",
    .transform_enabled = false
};

ServerConfig *get_server_config(void) {
    return &server_config;
}

ServerStatus *get_server_status(void) {
    return &server_status;
}

static void log_vmessage(LogLevel level, const char *format, va_list args) {
    const char *level_strings[] = { "DEBUG", "INFO", "WARN", "ERROR" };

    // Getting current time
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char formatted_time[20];
    strftime(formatted_time, sizeof(formatted_time), "%Y-%m-%d %H:%M:%S", tm_info);

    // Printing log message
    fprintf(stderr, "[%s] [%s] ", formatted_time, level_strings[level]);
    vfprintf(stderr, format, args);
    fprintf(stderr, "\n");
}

void log_message(LogLevel level, const char *format, ...) {
    va_list args;
    va_start(args, format);
    log_vmessage(level, format, args);
    va_end(args);
}

void log_debug(const char *format, ...) {
    va_list args;
    va_start(args, format);
    log_vmessage(LOG_DEBUG, format, args);
    va_end(args);
}

void log_info(const char *format, ...) {
    va_list args;
    va_start(args, format);
    log_vmessage(LOG_INFO, format, args);
    va_end(args);
}

void log_warn(const char *format, ...) {
    va_list args;
    va_start(args, format);
    log_vmessage(LOG_WARN, format, args);
    va_end(args);
}

void log_error(const char *format, ...) {
    va_list args;
    va_start(args, format);
    log_vmessage(LOG_ERROR, format, args);
    va_end(args);
}

void log_new_connection(int socket_fd, const char *ip, int port) {
    log_info("New connection: Socket FD = %d, IP = %s, Port = %d", socket_fd, ip, port);
}

void log_command_received(client_state *client, const char *command_format, ...) {
    if (!client || !command_format) {
        log_error("log_command_received called with null arguments");
        return;
    }

    char formatted_command[1024];
    va_list args;

    va_start(args, command_format);
    vsnprintf(formatted_command, sizeof(formatted_command), command_format, args);
    va_end(args);

    formatted_command[sizeof(formatted_command) - 1] = '\0'; // Ensure null-termination

    const char *username = client->username[0] != '\0' ? client->username : "unknown";
    log_info("Command received from user '%s' (FD: %d): %s", username, client->fd, formatted_command);
}

void log_disconnection(client_state *client) {
    log_info("Disconnection: Socket FD = %d, User = %s", client->fd, client->username[0] != '\0' ? client->username : "unknown");
}
