#ifndef ADMIN_COMMANDS_H
#define ADMIN_COMMANDS_H

#include "pop3_commands.h" // Assuming this is your main header file

#define BUFFER_SIZE 1024

// Function declarations for admin commands
void handle_all_connec_command(client_state *client);
void handle_curr_connec_command(client_state *client);
void handle_bytes_trans_command(client_state *client);
void handle_users_command(client_state *client);
void handle_status_command(client_state *client);
void handle_max_users_command(client_state *client, int max_users);
void handle_delete_user_command(client_state *client, const char *username);
void handle_add_user_command(client_state *client, const char *username, const char *password);
void handle_reset_user_password_command(client_state *client, const char *username);
void handle_change_password_command(client_state *client, const char *old_password, const char *new_password);
void handle_set_transform_command(client_state *client, char * transform);

#endif // ADMIN_COMMANDS_H
