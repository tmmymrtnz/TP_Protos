#ifndef ADMIN_COMMANDS_H
#define ADMIN_COMMANDS_H

#include "pop3_commands.h" // Assuming this is your main header file

#define BUFFER_SIZE 1024
#define MAX_COMMANDS 10

// Function declarations for admin commands
void handle_all_connec_command(int socket_fd);
void handle_curr_connec_command(int socket_fd);
void handle_bytes_trans_command(int socket_fd);
void handle_users_command(int socket_fd);
void handle_status_command(int socket_fd);
void handle_max_users_command(int socket_fd, int max_users);
void handle_delete_user_command(int socket_fd, const char *username);
void handle_add_user_command(int socket_fd, const char *username, const char *password);
void handle_reset_user_password_command(int socket_fd, const char *username);
void handle_change_password_command(int socket_fd, const char *old_password, const char *new_password);
void handle_help_command(int socket_fd);
int process_admin_command(char * buffer, int socket_fd);

#endif // ADMIN_COMMANDS_H
