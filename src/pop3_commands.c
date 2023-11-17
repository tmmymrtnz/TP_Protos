#include "include/pop3_commands.h"
#include "auth.h"
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdlib.h>
#include "include/user.h"
#include "include/admin_commands.h"
#include "include/logger.h"

#define BASE_DIR "src/maildir/"

// Helper function to send a response to the client
void send_response(int client_socket, const char *response) {
    ssize_t bytes_sent = send(client_socket, response, strlen(response), 0);
    if (bytes_sent == -1) {
        log_message(LOG_ERROR, "Error sending response: %s", strerror(errno));
    }
    if (bytes_sent > 0) {
        ServerStatus *server_status = get_server_status();
        server_status->bytes_transmitted += bytes_sent;
    }
}

void cleanup_deleted_messages(client_state *client) {
    for (int i = 0; i < MAX_MESSAGES; ++i) {
        if (client->deleted_messages[i]) {
            char mail_file[256];
            snprintf(mail_file, sizeof(mail_file), "%s%s/cur/mail%d.eml", BASE_DIR, client->username, i + 1);
            remove(mail_file);
        }
    }
}

int is_regular_file(const char *path) {
    struct stat path_stat;
    if (stat(path, &path_stat) != 0) {
        return 0; // Cannot stat file, assume it's not regular
    }
    return S_ISREG(path_stat.st_mode);
}

int handle_user_command(client_state *client, char *arg) {
    log_command_received(client, "USER", arg);
    // Ensure the client has provided an argument
    if (!arg || strlen(arg) == 0) {
        // If an error occurs
        send_response(client->fd, "-ERR User required\r\n");
        return - 1; // Return an error
    }

    strncpy(client->username, arg, sizeof(client->username) - 1);
    if(validate_username(client->username) == 0) {
        send_response(client->fd, "+OK User name accepted, send PASS command\r\n");
        return 0; // Success
    }

    return - 1;
}

int handle_pass_command(client_state *client, char *password) {
    log_command_received(client, "PASS", password);
    // Ensure the client has provided a password
    if (!password || strlen(password) == 0) {
        send_response(client->fd, "-ERR Password required\r\n");
        return -1; // Return an error
    }

    // Authenticate the user
    if (validate_user_credentials(client->username, password) == 0) {
        client->authenticated = true;
        send_response(client->fd, "+OK Logged in\r\n");
        return 0; // Success
    } else {
        send_response(client->fd, "-ERR Invalid credentials\r\n");
        return -1; // Return an error
    }
}

void handle_list_command(client_state *client) {
    log_command_received(client, "LIST", NULL);
    if (!client->authenticated) {
        send_response(client->fd, "-ERR Not logged in\r\n");
        return;
    }

    char user_dir[256];
    snprintf(user_dir, sizeof(user_dir), "%s%s/cur", BASE_DIR, client->username);

    DIR *dir = opendir(user_dir);
    if (!dir) {
        send_response(client->fd, "-ERR Mailbox not found\r\n");
        return;
    }

    struct dirent *entry;
    char response[512];
    int count = 0;
    char list_response[4096] = "+OK Mailbox scan listing follows\r\n";

    while ((entry = readdir(dir))) {
        char full_path[512];
        snprintf(full_path, sizeof(full_path), "%s/%s", user_dir, entry->d_name);
        if (is_regular_file(full_path)) {
            // Get file size
            struct stat file_stat;
            if (stat(full_path, &file_stat) == 0) {
                count++;
                snprintf(response, sizeof(response), "%d %ld\r\n", count, file_stat.st_size);
                strncat(list_response, response, sizeof(list_response) - strlen(list_response) - 1);
            }
        }
    }

    send(client->fd, list_response, strlen(list_response), 0);
    send(client->fd, ".\r\n", 3, 0);

    closedir(dir);
}


int handle_retr_command(client_state *client, int mail_number) {
    log_command_received(client, "RETR", "%d", mail_number);
    if (!client->authenticated) {
        send_response(client->fd, "-ERR Not logged in\r\n");
        return -1;
    }

    char mail_file[256];
    snprintf(mail_file, sizeof(mail_file), "%s%s/cur/mail%d.eml", BASE_DIR, client->username, mail_number);

    FILE *file = fopen(mail_file, "r");
    if (!file) {
        send_response(client->fd, "-ERR Message not found\r\n");
        return -1;
    }

    // Get file size
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    // Send response with message size
    char size_response[256];
    snprintf(size_response, sizeof(size_response), "+OK %ld octets\r\n", file_size);
    send_response(client->fd, size_response);

    // Send message content
    char buffer[1024];
    size_t bytes_read;
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        if (send(client->fd, buffer, bytes_read, 0) == -1) {
            // Handle send errors
            fclose(file);
            return -1;
        }
    }

    // Send the final period to indicate the end of the email content
    send_response(client->fd, "\r\n.\r\n");

    fclose(file);
    return 0;
}

int handle_dele_command(client_state *client, int mail_number) {
    log_command_received(client, "DELE", "%d", mail_number);
    if (!client->authenticated) {
        send_response(client->fd, "-ERR Not logged in\r\n");
        return -1;
    }

    // Check if mail_number is within the valid range
    if (mail_number <= 0 || mail_number > MAX_MESSAGES) {
        send_response(client->fd, "-ERR Invalid message number\r\n");
        return -1;
    }

    // Mark the message as deleted
    client->deleted_messages[mail_number - 1] = true;
    send_response(client->fd, "+OK Message marked for deletion\r\n");
    return 0;
}

void handle_rset_command(client_state *client) {
    log_command_received(client, "RSET", NULL);
    if (!client->authenticated) {
        send_response(client->fd, "-ERR Not logged in\r\n");
        return;
    }

    // Reset all deletion marks
    for (int i = 0; i < MAX_MESSAGES; ++i) {
        client->deleted_messages[i] = false;
    }

    send_response(client->fd, "+OK Reset state\r\n");
}

void handle_stat_command(client_state *client) {
    log_command_received(client, "STAT", NULL);
    if (!client->authenticated) {
        send_response(client->fd, "-ERR Not logged in\r\n");
        return;
    }

    char user_dir[256];
    snprintf(user_dir, sizeof(user_dir), "%s%s/cur", BASE_DIR, client->username);

    DIR *dir = opendir(user_dir);
    if (!dir) {
        send_response(client->fd, "-ERR Mailbox not found\r\n");
        return;
    }

    struct dirent *entry;
    int count = 0;
    long total_size = 0;
    int mail_index = 0;  // Used to keep track of the mail number

    while ((entry = readdir(dir))) {
        char full_path[512];
        snprintf(full_path, sizeof(full_path), "%s/%s", user_dir, entry->d_name);
        if (is_regular_file(full_path)) {
            mail_index++;

            // Skip this message if it's marked for deletion
            if (client->deleted_messages[mail_index - 1]) {
                continue;
            }

            struct stat file_stat;
            if (stat(full_path, &file_stat) == 0) {
                count++;
                total_size += file_stat.st_size;
            }
        }
    }

    char response[512];
    snprintf(response, sizeof(response), "+OK %d %ld\r\n", count, total_size);
    send_response(client->fd, response);

    closedir(dir);
}

void handle_capa_command(client_state *client) {
    log_command_received(client, "CAPA", NULL);
    if (is_admin(client->username)) {
        send_response(client->fd, "+OK Capability list follows\r\n"
                                 "CAPA\r\n"
                                 "QUIT\r\n"
                                 "ALL_CONNEC\r\n"
                                 "CURR_CONNEC\r\n"
                                 "BTYES_TRANS\r\n"
                                 "USERS\r\n"
                                 "STATUS\r\n"
                                 "MAX_USERS <int>\r\n"
                                 "DELETE_USER <username>\r\n"
                                 "ADD_USER <username> <password>\r\n"
                                 "RESET_USER_PASSWORD <username>\r\n"
                                 "CHANGE_PASSWORD <old_password> <new_password>\r\n"
                                 ".\r\n");
    } else {
        send_response(client->fd, "+OK Capability list follows\r\n"
                                 "QUIT\r\n"
                                 "USER\r\n"
                                 "PASS\r\n"
                                 "CAPA\r\n"
                                 "DELE\r\n"
                                 "STAT\r\n"
                                 "LIST\r\n"
                                 "RETR\r\n"
                                 "RSET\r\n"
                                 ".\r\n");
    }
}

// Helper function to reset the read buffer after processing a command
static void reset_read_buffer(client_state *client) {
    memset(client->read_buffer, 0, sizeof(client->read_buffer));
    client->read_buffer_pos = 0;
}

// This function processes a command from the POP3 client and handles it appropriately.
// It returns 0 if the command was processed successfully, or -1 if the client should be disconnected.
int process_pop3_command(client_state *client) {
    // Ensure there's a complete command to process
    char *end_of_command = strstr(client->read_buffer, "\r\n");
    if (!end_of_command) {
        // Command is incomplete, return and wait for more data
        return 0;
    }

    // Null-terminate the command and prepare for the next command
    *end_of_command = '\0';

    // Log the command for debugging purposes
    log_command_received(client, "%s", client->read_buffer);

    // Process the USER command
    if (strncmp(client->read_buffer, "USER ", 5) == 0) {
        char *username = client->read_buffer + 5;
        strncpy(client->username, username, sizeof(client->username) - 1);
        send_response(client->fd, "+OK User name accepted, send PASS command\r\n");
    }
    // Process the PASS command
    else if (strncmp(client->read_buffer, "PASS ", 5) == 0) {
        char *password = client->read_buffer + 5;
        if (handle_pass_command(client, password) == 0) {
            client->authenticated = true;
        }
    }
    // Process the QUIT command
    else if (strncmp(client->read_buffer, "QUIT", 4) == 0) {
        if (client->authenticated) {
            cleanup_deleted_messages(client);
        }
        send_response(client->fd, "+OK See-ya\r\n");
        return -1; // Return -1 to close the connection
    }
    // Process other commands only if authenticated
    else if (client->authenticated) {
        // Process the LIST command
        if (strncmp(client->read_buffer, "LIST", 4) == 0) {
            handle_list_command(client);
        }
        // Process the RETR command
        else if (strncmp(client->read_buffer, "RETR ", 5) == 0) {
            int mail_number = atoi(client->read_buffer + 5);
            if (handle_retr_command(client, mail_number) == -1) {
                // Handle error (if any)
            }
        }
        // Process the DELE command
        else if (strncmp(client->read_buffer, "DELE ", 5) == 0) {
            int mail_number = atoi(client->read_buffer + 5);
            if (handle_dele_command(client, mail_number) == -1) {
                // Handle error (if any)
            }
        }
        else if (strncmp(client->read_buffer, "RSET", 4) == 0) {
            handle_rset_command(client);
        }
        else if (strncmp(client->read_buffer, "STAT", 4) == 0) {
            handle_stat_command(client);
        }
        else if (strncmp(client->read_buffer, "CAPA", 4) == 0) {
            handle_capa_command(client);
        }
        else if (is_admin(client->username)) {
            if (strncmp(client->read_buffer, "ALL_CONNEC", 10) == 0) {
                handle_all_connec_command(client);
            }
            else if (strncmp(client->read_buffer, "CURR_CONNEC", 11) == 0) {
                handle_curr_connec_command(client);
            }
            else if (strncmp(client->read_buffer, "BYTES_TRANS", 11) == 0) {
                handle_bytes_trans_command(client);
            }
            else if (strncmp(client->read_buffer, "USERS", 5) == 0) {
                handle_users_command(client);
            }
            else if (strncmp(client->read_buffer, "STATUS", 6) == 0) {
                handle_status_command(client);
            }
            else if (strncmp(client->read_buffer, "MAX_USERS", 9) == 0) {
                int new_max_users = -1; // Default to an invalid value

                // Check if there is a space after "MAX_USERS"
                if (client->read_buffer[9] == ' ') {
                    // Extract the integer value if present
                    if (sscanf(client->read_buffer + 10, "%d", &new_max_users) < 1) {
                        send_response(client->fd, "-ERR Invalid argument for MAX_USERS command\r\n");
                        log_error("Invalid argument for MAX_USERS command: %s", client->read_buffer);
                        reset_read_buffer(client);
                        return 0; // Return 0 to keep the connection open
                    }
                }
                
                // Call the handler with the new max users value or -1 if not provided
                handle_max_users_command(client, new_max_users);
            }
            else if (strncmp(client->read_buffer, "DELETE_USER ", 12) == 0) {
                char username[USERNAME_MAX_LENGTH];
                sscanf(client->read_buffer + 12, "%s", username);
                handle_delete_user_command(client, username);
            }
            else if (strncmp(client->read_buffer, "ADD_USER ", 9) == 0) {
                char username[USERNAME_MAX_LENGTH], password[PASSWORD_MAX_LENGTH];
                sscanf(client->read_buffer + 9, "%s %s", username, password);
                handle_add_user_command(client, username, password);
            }
            else if (strncmp(client->read_buffer, "RESET_USER_PASSWORD ", 20) == 0) {
                char username[USERNAME_MAX_LENGTH];
                sscanf(client->read_buffer + 20, "%s", username);
                handle_reset_user_password_command(client, username);
            }
            else if (strncmp(client->read_buffer, "CHANGE_PASSWORD ", 16) == 0) {
                char old_password[USERNAME_MAX_LENGTH], new_password[PASSWORD_MAX_LENGTH];
                sscanf(client->read_buffer + 16, "%s %s", old_password, new_password);
                handle_change_password_command(client, old_password, new_password);
            }
            else {
                send_response(client->fd, "-ERR Unknown admin command\r\n");
            }

        }
        else {
            send_response(client->fd, "-ERR Unknown command or insufficient privileges\r\n");
        }
    } 
    
    else {
        send_response(client->fd, "-ERR Please authenticate using USER and PASS commands\r\n");
    }

    // Reset the read buffer for the next command
    reset_read_buffer(client);

    return 0; // Keep the connection open unless QUIT was received
}

