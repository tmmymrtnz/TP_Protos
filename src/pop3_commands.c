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
#include <ctype.h>
#include "include/user.h"
#include "include/admin_commands.h"
#include "include/logger.h"
#include "include/transform_mail.h"

#define BASE_DIR "src/maildir/"

#define MAX_COMMANDS 10

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


int handle_retr_command(client_state* client, int mail_number) {
    
    log_command_received(client, "RETR", "%d", mail_number);
    
    if (!client->authenticated) {
        send_response(client->fd, "-ERR Not logged in\r\n");
        return -1;
    }
    TUsers *usersStruct = getUsersStruct();

    char mail_file[256];
    snprintf(mail_file, sizeof(mail_file), "%s%s/cur/mail%d.eml", BASE_DIR, client->username, mail_number);


        // Transform the email using transform_mail function
        char* transformed_content = transform_mail(mail_file,usersStruct->transform_app);
        if (transformed_content == NULL) {
            send_response(client->fd, "-ERR Transformation failed\r\n");
            return -1;
        }

        // Calculate the size of transformed content
        long transformed_size = strlen(transformed_content);

        // Send the transformed message content
        send_response(client->fd, "+OK Transformed content follows\r\n");

       ssize_t bytes_sent = 0;
        while (bytes_sent < transformed_size) {
            ssize_t sent = send(client->fd, transformed_content + bytes_sent, transformed_size - bytes_sent, 0);
        if (sent == -1) {
            free(transformed_content);
            send_response(client->fd, "-ERR Failed to send transformed content\r\n");
            return -1;
        }
        bytes_sent += sent;
        }


        free(transformed_content);


    // Send the final period to indicate the end of the email content
    send_response(client->fd, "\r\n.\r\n");
    return 0;
}

int handle_dele_command(client_state *client, int mail_number) {
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
                                 "TRANSFORM <transform application>\r\n"
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

void handle_uidl_command(client_state *client, char *argument) {
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

    if (argument) {
        int mail_number = atoi(argument);
        handle_uidl_for_specific_message(client, dir, mail_number, user_dir);
    } else {
        handle_uidl_for_all_messages(client, dir, user_dir);
    }

    closedir(dir);
}

void handle_uidl_for_specific_message(client_state *client, DIR *dir, int mail_number, const char *user_dir) {
    struct dirent *entry;
    int count = 0;
    char file_path[512]; // Buffer to hold the full path of the file

    while ((entry = readdir(dir))) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue; // Skip . and .. entries
        }

        // Construct the full path to the file
        snprintf(file_path, sizeof(file_path), "%s/%s", user_dir, entry->d_name);

        if (is_regular_file(file_path)) {
            count++;
            if (count == mail_number) {
                char unique_id[256];
                generate_unique_id(entry->d_name, unique_id, sizeof(unique_id));

                char response[512];
                snprintf(response, sizeof(response), "+OK %d %s\r\n", mail_number, unique_id);
                send_response(client->fd, response);
                return;
            }
        }
    }

    send_response(client->fd, "-ERR No such message\r\n");
}

void handle_uidl_for_all_messages(client_state *client, DIR *dir, const char *user_dir) {
    struct dirent *entry;
    int count = 0;
    char uidl_response[4096] = "+OK\r\n";
    char file_path[512]; // Buffer to hold the full path of the file

    while ((entry = readdir(dir))) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue; // Skip . and .. entries
        }

        // Construct the full path to the file
        snprintf(file_path, sizeof(file_path), "%s/%s", user_dir, entry->d_name);
        
        if (is_regular_file(file_path)) {
            count++;
            char unique_id[256];
            generate_unique_id(entry->d_name, unique_id, sizeof(unique_id));

            char response[512];
            snprintf(response, sizeof(response), "%d %s\r\n", count, unique_id);
            strncat(uidl_response, response, sizeof(uidl_response) - strlen(uidl_response) - 1);
        }
    }

    strncat(uidl_response, ".\r\n", sizeof(uidl_response) - strlen(uidl_response) - 1);
    send_response(client->fd, uidl_response);
}

void generate_unique_id(const char *filename, char *unique_id, size_t max_size) {
    // Generate a unique identifier based on filename or other attributes
    snprintf(unique_id, max_size, "%s", filename);
}

// Helper function to reset the read buffer after processing a command
static void reset_read_buffer(client_state *client) {
    memset(client->read_buffer, 0, sizeof(client->read_buffer));
    client->read_buffer_pos = 0;
}

// This function processes a command from the POP3 client and handles it appropriately.
// It returns 0 if the command was processed successfully, or -1 if the client should be disconnected.
int process_pop3_command(client_state *client) {
    // Split commands by line separator "\r\n"
    char *command = strtok(client->read_buffer, "\r\n");
    char *commands[MAX_COMMANDS];
    int num_commands = 0;

    while (command != NULL && num_commands < MAX_COMMANDS) {
        commands[num_commands++] = command;
        command = strtok(NULL, "\r\n");
    }

    // Process each command separately
    for (int i = 0; i < num_commands; ++i) {
        command = commands[i];

        // Log the command for debugging purposes
        log_command_received(client, "%s", command);

        // Convert the commands to uppercase
        int case_sensitive = 1;

         for (int j = 0; command[j] && case_sensitive; ++j) {
            if(command[j]==' ')
                case_sensitive = 0;
            else if (isalpha(command[j]) && islower(command[j]))
                command[j] = toupper(command[j]);
        }

        // Process each command separately
        if (strncmp(command, "USER ", 5) == 0) {
            // Process the USER command
            char *username = command + 5;
            strncpy(client->username, username, sizeof(client->username) - 1);
            send_response(client->fd, "+OK User name accepted, send PASS command\r\n");
        } else if (strncmp(command, "PASS ", 5) == 0) {
            // Process the PASS command
            char *password = command + 5;
            if (handle_pass_command(client, password) == 0) {
                client->authenticated = true;
            }
        } else if (strncmp(command, "QUIT", 4) == 0) {
            // Process the QUIT command
            if (client->authenticated) {
                cleanup_deleted_messages(client);
            }
            send_response(client->fd, "+OK See-ya\r\n");
            return -1; // Return -1 to close the connection
        } else if(client->authenticated && is_admin(client->username)) {
            if (strncmp(command, "ALL_CONNEC", 10) == 0) {
                    handle_all_connec_command(client);
                }
                else if (strncmp(command, "CURR_CONNEC", 11) == 0) {
                    handle_curr_connec_command(client);
                }
                else if (strncmp(command, "BYTES_TRANS", 11) == 0) {
                    handle_bytes_trans_command(client);
                }
                else if (strncmp(command, "USERS", 5) == 0) {
                    handle_users_command(client);
                }
                else if (strncmp(command, "STATUS", 6) == 0) {
                    handle_status_command(client);
                }
                else if (strncmp(command, "MAX_USERS", 9) == 0) {
                    int new_max_users = -1; // Default to an invalid value

                    // Check if there is a space after "MAX_USERS"
                    if (client->read_buffer[9] == ' ') {
                        // Extract the integer value if present
                        if (sscanf(command + 10, "%d", &new_max_users) < 1) {
                            send_response(client->fd, "-ERR Invalid argument for MAX_USERS command\r\n");
                            log_error("Invalid argument for MAX_USERS command: %s", command);
                            reset_read_buffer(client);
                            return 0; // Return 0 to keep the connection open
                        }
                    }
                    
                    // Call the handler with the new max users value or -1 if not provided
                    handle_max_users_command(client, new_max_users);
                }
                else if (strncmp(command, "DELETE_USER ", 12) == 0) {
                    char username[USERNAME_MAX_LENGTH];
                    sscanf(command + 12, "%s", username);
                    handle_delete_user_command(client, username);
                }
                else if (strncmp(command, "ADD_USER ", 9) == 0) {
                    char username[USERNAME_MAX_LENGTH], password[PASSWORD_MAX_LENGTH];
                    sscanf(command + 9, "%s %s", username, password);
                    handle_add_user_command(client, username, password);
                }
                else if (strncmp(command, "RESET_USER_PASSWORD ", 20) == 0) {
                    char username[USERNAME_MAX_LENGTH];
                    sscanf(command + 20, "%s", username);
                    handle_reset_user_password_command(client, username);
                }
                else if (strncmp(command, "CHANGE_PASSWORD ", 16) == 0) {
                    char old_password[USERNAME_MAX_LENGTH], new_password[PASSWORD_MAX_LENGTH];
                    sscanf(command + 16, "%s %s", old_password, new_password);
                    handle_change_password_command(client, old_password, new_password);
                }
                else if (strncmp(command, "TRANSFORM ", 10) == 0) {
                    char transform[10];
                    sscanf(command + 10, "%s", transform);
                    handle_set_transform_command(client, transform);
                }
                else if (strncmp(command, "CAPA", 4) == 0) {
                handle_capa_command(client);
                }
                else {
                    send_response(client->fd, "-ERR Unknown admin command\r\n");
                }
                
        }else if (client->authenticated) {// Process other commands only if authenticated
            // Process the LIST command
            if (strncmp(command, "LIST", 4) == 0) {
                handle_list_command(client);
            }
            // Process the RETR command
            else if (strncmp(command, "RETR ", 5) == 0) {
                int mail_number = atoi(command + 5);
                if (handle_retr_command(client, mail_number) == -1) {
                    // Handle error (if any)
                }
            }
            // Process the DELE command
            else if (strncmp(command, "DELE ", 5) == 0) {
                int mail_number = atoi(command + 5);
                if (handle_dele_command(client, mail_number) == -1) {
                    // Handle error (if any)
                }
            }
            else if (strncmp(client->read_buffer, "UIDL", 4) == 0) {
                char *argument = NULL;
                if (strlen(client->read_buffer) > 5) {
                    argument = client->read_buffer + 5;
                }
                handle_uidl_command(client, argument);
            }
            else if (strncmp(command, "RSET", 4) == 0) {
                handle_rset_command(client);
            }
            else if (strncmp(command, "STAT", 4) == 0) {
                handle_stat_command(client);
            }
            else if (strncmp(command, "CAPA", 4) == 0) {
                handle_capa_command(client);
            }
           else {
                // Handle unknown commands or insufficient privileges
                send_response(client->fd, "-ERR Unknown command or insufficient privileges\r\n");
            }
        }else {
            // Authentication required for other commands
            send_response(client->fd, "-ERR Please authenticate using USER and PASS commands\r\n");
        }
    }

    // Reset the read buffer for the next command
    reset_read_buffer(client);

    return 0; // Keep the connection open unless QUIT was received
}
