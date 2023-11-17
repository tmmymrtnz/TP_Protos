#include "include/admin_commands.h"
#include <stdio.h>
#include "include/user.h"
#include "include/logger.h"

// Example implementation of admin commands

void handle_all_connec_command(client_state *client) {
    ServerStatus *status = get_server_status();
    char response[256];
    snprintf(response, sizeof(response), "+OK Total connections: %zu\r\n", status->total_connections);
    send_response(client->fd, response);
    log_info("Admin command executed by %s: Total Connections", client->username);
}

void handle_curr_connec_command(client_state *client) {
    ServerStatus *status = get_server_status();
    char response[256];
    snprintf(response, sizeof(response), "+OK Current connections: %zu\r\n", status->current_connections);
    send_response(client->fd, response);
    log_info("Admin command executed by %s: Current Connections", client->username);
}

void handle_bytes_trans_command(client_state *client) {
    ServerStatus *status = get_server_status();
    char response[256];
    snprintf(response, sizeof(response), "+OK Bytes transmitted: %ld\r\n", status->bytes_transmitted);
    send_response(client->fd, response);
    log_info("Admin command executed by %s: Bytes Transmitted", client->username);
}

void handle_users_command(client_state *client) {
    char response[BUFFER_SIZE];
    strcpy(response, "+OK List of users:\r\n");

    for (int i = 0; i < usersStruct->count; i++) {
        char user_info[500]; // Adjust size as needed
        snprintf(user_info, sizeof(user_info), "Username: %s, Admin: %d, Connected: %d\r\n",
                 usersStruct->users[i].username,
                 usersStruct->users[i].isAdmin,
                 usersStruct->users[i].isConnected);
        strncat(response, user_info, sizeof(response) - strlen(response) - 1);
    }

    strncat(response, ".\r\n", sizeof(response) - strlen(response) - 1);
    send_response(client->fd, response);
}

void handle_status_command(client_state *client) {
    ServerStatus *status = get_server_status();
    char response[512];

    snprintf(response, sizeof(response),
             "+OK Server Status:\r\n"
             "Total connections: %zu\r\n"
             "Current connections: %zu\r\n"
             "Bytes transmitted: %ld\r\n"
             ".\r\n",
             status->total_connections,
             status->current_connections,
             status->bytes_transmitted);

    send_response(client->fd, response);
    log_info("Admin command executed by %s: Server Status", client->username);
}

void handle_max_users_command(client_state *client, int max_users) {
    char response[BUFFER_SIZE];

    if (max_users == -1) {
        // If new_max_users is -1, it means we just want to retrieve the current setting
        snprintf(response, sizeof(response), "+OK Current maximum users: %d\r\n", usersStruct->max_users);
    } else {
        // Here, set the new max users value
        usersStruct->max_users = max_users;
        snprintf(response, sizeof(response), "+OK Maximum users set to: %d\r\n", max_users);
    }
    
    send_response(client->fd, response);
}

void handle_delete_user_command(client_state *client, const char *username) {
    if (delete_user(username) == 0) {
        send_response(client->fd, "+OK User deleted successfully.\r\n");
    } else {
        send_response(client->fd, "-ERR Failed to delete user.\r\n");
    }
}

void handle_add_user_command(client_state *client, const char *username, const char *password) {
    if (add_user(username, password) == 0) {
        send_response(client->fd, "+OK User added successfully.\r\n");
    } else {
        send_response(client->fd, "-ERR Failed to add user.\r\n");
    }
}

void handle_reset_user_password_command(client_state *client, const char *username) {
    if (reset_user_password(username) == 0) {
        send_response(client->fd, "+OK User password reset successfully.\r\n");
    } else {
        send_response(client->fd, "-ERR Failed to reset user's password.\r\n");
    }
}

void handle_change_password_command(client_state *client, const char *old_password, const char *new_password) {
    if (change_password(client->username, old_password, new_password) == 0) {
        send_response(client->fd, "+OK Password changed successfully.\r\n");
    } else {
        send_response(client->fd, "-ERR Failed to change password.\r\n");
    }
}
