#include "include/admin_commands.h"
#include <stdio.h>
#include "include/user.h"
#include "include/logger.h"
#include <string.h>
#include <ctype.h>
#include "include/pop3_commands.h"

// Example implementation of admin commands


void handle_all_connec_command(int socket_fd) {
    ServerStatus *status = get_server_status();
    char response[256];
    snprintf(response, sizeof(response), "ALL_CONNEC -> +OK\r\n- %zu\r\n", status->total_connections);
    send_response(socket_fd, response);
    log_info("Admin command executed by default: All Connections");
}

void handle_curr_connec_command(int socket_fd) {
    ServerStatus *status = get_server_status();
    char response[256];
    snprintf(response, sizeof(response), "CURR_CONNEC -> +OK\r\n- %zu\r\n", status->current_connections);
    send_response(socket_fd, response);
    log_info("Admin command executed by default: Current Connections");
}

void handle_bytes_trans_command(int socket_fd) {
    ServerStatus *status = get_server_status();
    char response[256];
    snprintf(response, sizeof(response), "BYTES_TRANS -> +OK\r\n- %ld\r\n", status->bytes_transmitted);
    send_response(socket_fd, response);
    log_info("Admin command executed by %s: Bytes Transmitted", "default");
}

void handle_users_command(int socket_fd) {
    char response[BUFFER_SIZE];
    TUsers *usersStruct = getUsersStruct();
    strcpy(response, "USERS -> +OK\r\n- List of users:\r\n");

    for (int i = 0; i < usersStruct->count; i++) {
        char user_info[500]; // Adjust size as needed
        snprintf(user_info, sizeof(user_info), "Username: %s, Admin: %d, Connected: %d\r\n",
                 usersStruct->users[i].username,
                 usersStruct->users[i].isAdmin,
                 usersStruct->users[i].isConnected);
        strncat(response, user_info, sizeof(response) - strlen(response) - 1);
    }

    strncat(response, ".\r\n", sizeof(response) - strlen(response) - 1);
    send_response(socket_fd, response);
    log_info("Admin command executed by %s: List Users", "default");
}

void handle_status_command(int socket_fd) {
    ServerStatus *status = get_server_status();
    char response[512];

    snprintf(response, sizeof(response),
         "STATUS -> +OK\r\n"
         "- ALL_CONNEC %zu\r\n"
         "- CURR_CONNEC %zu\r\n"
         "- BYTES_TRANS %ld\r\n"
         ".\r\n",
         status->total_connections,
         status->current_connections,
         status->bytes_transmitted);

    send_response(socket_fd, response);
    log_info("Admin command executed by %s: Server Status", "default");
}

void handle_max_users_command(int socket_fd, int max_users) {
    char response[BUFFER_SIZE];
    TUsers *usersStruct = getUsersStruct();

    if (max_users == -1) {
        // If new_max_users is -1, it means we just want to retrieve the current setting
        snprintf(response, sizeof(response), "MAX_USERS -> +OK\r\n- %d\r\n", usersStruct->max_users);
    } else {
        if (max_users < usersStruct->count) {
            snprintf(response, sizeof(response), "-ERR Cannot set maximum users to %d, there are currently %d users connected\r\n",
                     max_users, usersStruct->count);
            send_response(socket_fd, response);
            return;
        }
        // Here, set the new max users value
        usersStruct->max_users = max_users;
        usersStruct->users = realloc(usersStruct->users, sizeof(TUser) * max_users);
        snprintf(response, sizeof(response), "MAX_USERS %d -> +OK\r\n", max_users);
    }
    
    send_response(socket_fd, response);
}


void handle_delete_user_command(int socket_fd, const char *username) {
    if (delete_user(username) == 0) {
        send_response(socket_fd, "DELETE_USER -> +OK");
        log_info("Admin command executed by %s: User %s deleted", "default", username);
    } else {
        send_response(socket_fd, "-ERR Failed to delete user.\r\n");
        log_info("Admin command executed by %s: Failed to delete user %s", "default", username);
    }
}

void handle_add_user_command(int socket_fd, const char *username, const char *password) {
    if (add_user(username, password) == 0) {
        send_response(socket_fd, "ADD_USER -> +OK\r\n");
        log_info("Admin command executed by %s: User %s added", "default", username);
    } else {
        send_response(socket_fd, "-ERR Failed to add user.\r\n");
        log_info("Admin command executed by %s: Failed to add user %s", "default", username);
    }
}

void handle_reset_user_password_command(int socket_fd, const char *username) {
    if (reset_user_password(username) == 0) {
        send_response(socket_fd, "RESET_USER_PASSWORD -> +OK\r\n");
        log_info("Admin command executed by %s: Password reset for user %s", "default", username);
    } else {
        send_response(socket_fd, "-ERR\r\n");
        log_info("Admin command executed by %s: Failed to reset password for user %s", "default", username);
    }
}

void handle_change_password_command(int socket_fd, const char *old_password, const char *new_password) {
    if (change_password("default", old_password, new_password) == 0) {
        send_response(socket_fd, "CHANGE_PASSWORD -> +OK\r\n");
        log_info("Admin command executed by %s: Password changed", "default");
    } else {
        send_response(socket_fd, "-ERR Failed to change password.\r\n");
        log_info("Admin command executed by %s: Failed to change password", "default");
    }
}

void handle_help_command(int socket_fd) {
    char response[BUFFER_SIZE]; // Ensure BUFFER_SIZE is sufficiently large

    snprintf(response, sizeof(response),
             "HELP -> +OK\r\n"
             "Usage: ./bin/popadmin [OPTION]...\r\n\r\n"
             "   -h                                 Help.\r\n"
             "   -A <username> <password>          Add a user to the server. Requires username and password.\r\n"
             "   -D <username>                     Delete a user from the server. Requires username.\r\n"
             "   -R <username>                     Reset a user's password. Requires username.\r\n"
             "   -C <old_password> <new_password>  Change password. Requires old and new passwords.\r\n"
             "   -m <optional: max_users>          Set/Get the maximum number of users. Optionally requires max users count.\r\n"
             "   -d                                Get the path to the mail directory.\r\n"
             "   -M <path>                         Set the mail directory path. Requires maildir path.\r\n"
             "   -a                                Get the total number of connections.\r\n"
             "   -c                                Get the current number of connections.\r\n"
             "   -b                                Get the number of bytes transferred.\r\n"
             "   -s                                Get the server status.\r\n"
             "   -u                                List all users.\r\n"
             // Add other command descriptions as needed
             ".\r\n"); // Ensure to end with the termination sequence
    send_response(socket_fd, response);
}


void handle_get_maildir_command(int socket_fd) {
    char response[BUFFER_SIZE];
    ServerConfig *server_config = get_server_config();
    snprintf(response, sizeof(response), "GET_MAILDIR -> +OK\r\n- %s\r\n", server_config->mail_dir);
    send_response(socket_fd, response);
    log_info("Admin command executed by %s: Get Mail Directory", "default");
}

void handle_set_maildir_command(int socket_fd, char *maildir) {
    ServerConfig *server_config = get_server_config();

    // Check if the last character is not '/', if so, add it
    size_t maildir_len = strlen(maildir);
    char *new_maildir = malloc(maildir_len + 2); // +1 for potential '/' and +1 for '\0'
    if (new_maildir == NULL) {
        perror("Failed to allocate memory for new maildir");
        send_response(socket_fd, "-ERR Server error: memory allocation failed\r\n");
        return;
    }

    strcpy(new_maildir, maildir);
    if (new_maildir[maildir_len - 1] != '/') {
        new_maildir[maildir_len] = '/';
        new_maildir[maildir_len + 1] = '\0';
    }

    // Free the old mail_dir and assign the new one
    free(server_config->mail_dir);
    server_config->mail_dir = new_maildir;

    char response[BUFFER_SIZE];
    snprintf(response, sizeof(response), "SET_MAILDIR -> +OK\r\n- %s\r\n", server_config->mail_dir);
    send_response(socket_fd, response);
    log_info("Admin command executed by %s: Set Mail Directory to %s", "default", server_config->mail_dir);
}

int process_admin_command(char * buffer, int socket_fd) {
    char *command = strtok(buffer, "\r\n");
    char *commands[MAX_COMMANDS];
    int num_commands = 0;

    while (command != NULL && num_commands < MAX_COMMANDS) {
        commands[num_commands++] = command;
        command = strtok(NULL, "\r\n");
    }

    for (int i = 0; i < num_commands; ++i) {
        command = commands[i];
        // log_command_received(socket_fd, "%s", command);

        int case_sensitive = 1;
        for (int j = 0; command[j] && case_sensitive; ++j) {
            if (command[j] == ' ')
                case_sensitive = 0;
            else if (isalpha(command[j]) && islower(command[j]))
                command[j] = toupper(command[j]);
        }

        if (strncmp(command, "ALL_CONNEC", 10) == 0) {
            handle_all_connec_command(socket_fd);
        } else if (strncmp(command, "CURR_CONNEC", 11) == 0) {
            handle_curr_connec_command(socket_fd);
        } else if (strncmp(command, "BYTES_TRANS", 11) == 0) {
            handle_bytes_trans_command(socket_fd);
        } else if (strncmp(command, "USERS", 5) == 0) {
            handle_users_command(socket_fd);
        } else if (strncmp(command, "STATUS", 6) == 0) {
            handle_status_command(socket_fd);
        }else if (strncmp(command, "MAX_USERS", 9) == 0) {
            int new_max_users = -1; // Default to an invalid value

            // Check if there is a space after "MAX_USERS"
            if (buffer[9] == ' ') {
                // Extract the integer value if present
                if (sscanf(command + 10, "%d", &new_max_users) < 1) {
                    send_response(socket_fd, "-ERR Invalid argument for MAX_USERS command\r\n");
                    log_error("Invalid argument for MAX_USERS command: %s", command);
                    memset(buffer, 0, BUFFER_SIZE);
                    return 0; // Return 0 to keep the connection open
                }
            }
            
            // Call the handler with the new max users value or -1 if not provided
            handle_max_users_command(socket_fd, new_max_users);
        }
        else if (strncmp(command, "DELETE_USER ", 12) == 0) {
            char username[USERNAME_MAX_LENGTH];
            sscanf(command + 12, "%s", username);
            handle_delete_user_command(socket_fd, username);
        }
        else if (strncmp(command, "ADD_USER ", 9) == 0) {
            char username[USERNAME_MAX_LENGTH], password[PASSWORD_MAX_LENGTH];
            sscanf(command + 9, "%s %s", username, password);
            handle_add_user_command(socket_fd, username, password);
        }
        else if (strncmp(command, "RESET_USER_PASSWORD ", 20) == 0) {
            char username[USERNAME_MAX_LENGTH];
            sscanf(command + 20, "%s", username);
            handle_reset_user_password_command(socket_fd, username);
        }
        else if (strncmp(command, "CHANGE_PASSWORD ", 16) == 0) {
            char old_password[USERNAME_MAX_LENGTH], new_password[PASSWORD_MAX_LENGTH];
            sscanf(command + 16, "%s %s", old_password, new_password);
            handle_change_password_command(socket_fd, old_password, new_password);
        }
        // else if (strncmp(command, "TRANSFORM ", 10) == 0) {
        //     char transform[10];
        //     sscanf(command + 10, "%s", transform);
        //     handle_set_transform_command(socket_fd, transform);
        // }
        else if (strncmp(command, "HELP", 4) == 0) {
            handle_help_command(socket_fd);
        }
        else if (strncmp(command, "GET_MAILDIR", 11) == 0) {
            handle_get_maildir_command(socket_fd);
        }
        // Handle the SET_MAILDIR command
        else if (strncmp(command, "SET_MAILDIR ", 12) == 0) {
            char maildir[BUFFER];
            sscanf(command + 12, "%s", maildir);
            handle_set_maildir_command(socket_fd, maildir);
        }
        else {
            send_response(socket_fd, "-ERR Unknown admin command\r\n");
        }

    }

    memset(buffer, 0, BUFFER_SIZE);
    return 0;
}
