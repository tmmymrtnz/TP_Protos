#include "include/pop3_commands.h"
#include "auth.h"
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <limits.h>
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
#define MAX_COMMAND_LENGTH 100
#define BUFFER_CHUNK_SIZE 4096
#define RESPONSE_SIZE 8192*3 

char* read_mail_file(const char* file_path) {
    FILE *file = fopen(file_path, "r");
    if (!file) {
        perror("fopen");
        return NULL;
    }

    // Seek to the end of the file to determine its size
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    rewind(file); // Go back to the start of the file

    // Allocate memory for the file content
    char *content = malloc(file_size + 1);
    if (!content) {
        perror("malloc");
        fclose(file);
        return NULL;
    }

    // Read the file into memory
    if (fread(content, 1, file_size, file) != (size_t)file_size) {
        perror("fread");
        free(content);
        fclose(file);
        return NULL;
    }

    // Null-terminate the string
    content[file_size] = '\0';

    fclose(file);
    return content;
}

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
    ServerConfig *server_config = get_server_config();
    char directory_path[256];
    snprintf(directory_path, sizeof(directory_path), "%s%s/cur/", server_config->mail_dir, client->username);

    for (int i = 0; i < client->delete_size; ++i) {
        char file_path[512];
        snprintf(file_path, sizeof(file_path), "%s%s", directory_path, client->deleted_files[i]);
        remove(file_path);
    }
    // Reset the delete_size after cleaning up
    client->delete_size = 0;
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

    ServerConfig *server_config = get_server_config();

    char user_dir[256];
    snprintf(user_dir, sizeof(user_dir), "%s%s/cur", server_config->mail_dir, client->username);

    DIR *dir = opendir(user_dir);
    if (!dir) {
        send_response(client->fd, "-ERR Mailbox not found\r\n");
        return;
    }

    struct dirent *entry;
    char response[512];
    int count = 0;
    char *list_response = NULL;
    size_t list_size = 0;

    while ((entry = readdir(dir))) {
        char full_path[512];
        snprintf(full_path, sizeof(full_path), "%s/%s", user_dir, entry->d_name);
        if (is_regular_file(full_path)) {
            // Get file size
            struct stat file_stat;
            if (stat(full_path, &file_stat) == 0) {
                count++;
                snprintf(response, sizeof(response), "%d %ld\r\n", count, file_stat.st_size);

                // Resize the list_response buffer to append the new entry
                size_t response_len = strlen(response);
                list_response = realloc(list_response, list_size + response_len + 1);
                if (!list_response) {
                    send_response(client->fd, "-ERR Memory allocation failed\r\n");
                    closedir(dir);
                    return;
                }

                strcpy(list_response + list_size, response);
                list_size += response_len;
            }
        }
    }

    char end_marker[] = ".\r\n";
    list_response = realloc(list_response, list_size + sizeof(end_marker));
    if (!list_response) {
        send_response(client->fd, "-ERR Memory allocation failed\r\n");
        closedir(dir);
        return;
    }
    strcpy(list_response + list_size, end_marker);

    send(client->fd, "+OK Mailbox scan listing follows\r\n", 35, 0);
    send(client->fd, list_response, list_size + sizeof(end_marker) - 1, 0);

    free(list_response);
    closedir(dir);
}
    
void send_with_byte_stuffing(int client_fd, const char *buffer, size_t buffer_size) {
    char temp_buffer[BUFFER_CHUNK_SIZE * 2];  // Temporary buffer to hold stuffed data
    size_t temp_index = 0;  // Index for the temporary buffer

    for (size_t i = 0; i < buffer_size; i++) {
        // Check if a new line starts with a period
        if (i == 0 || buffer[i - 1] == '\n') {
            if (buffer[i] == '.') {
                // Add an extra period
                temp_buffer[temp_index++] = '.';
                // Check for buffer overflow
                if (temp_index == sizeof(temp_buffer)) {
                    send(client_fd, temp_buffer, temp_index, 0);
                    temp_index = 0;
                }
            }
        }
        // Add the current character
        temp_buffer[temp_index++] = buffer[i];
        // Check for buffer overflow
        if (temp_index == sizeof(temp_buffer)) {
            send(client_fd, temp_buffer, temp_index, 0);
            temp_index = 0;
        }
    }

    // Send any remaining data in the buffer
    if (temp_index > 0) {
        send(client_fd, temp_buffer, temp_index, 0);
    }
}

int handle_retr_command(client_state *client, int mail_number) {
    log_command_received(client, "RETR", "%d", mail_number);
    if (!client->authenticated) {
        send_response(client->fd, "-ERR Not logged in\r\n");
        return -1;
    }

    ServerConfig *server_config = get_server_config();
    

    char directory_path[256];
    snprintf(directory_path, sizeof(directory_path), "%s%s/cur/", server_config->mail_dir, client->username);

    DIR *dir = opendir(directory_path);
    if (dir == NULL) {
        send_response(client->fd, "-ERR Unable to open mail directory\r\n");
        return -1;
    }

    struct dirent *entry;
    int file_count = 0;
    char mail_file[512] = {0}; // Increased size for path concatenation
    char full_path[512] = {0}; // Buffer for full file path

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue; // Skip '.' and '..' directories
        }

        snprintf(full_path, sizeof(full_path), "%s%s", directory_path, entry->d_name);

        if (is_regular_file(full_path)) {
            file_count++;
            if (file_count == mail_number) {
                strncpy(mail_file, full_path, sizeof(mail_file));
                break;
            }
        }
    }
    closedir(dir);

    if (mail_file[0] == '\0') {
        send_response(client->fd, "-ERR Mail not found\r\n");
        return -1;
    }

    FILE *file = fopen(mail_file, "r");
    if (!file) {
        send_response(client->fd, "-ERR Unable to open mail file\r\n");
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

    // Send message content with byte-stuffing correction
    char buffer[1024];
    size_t bytes_read;
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        send_with_byte_stuffing(client->fd, buffer, bytes_read);
    }

    // Send the final period to indicate the end of the email content
    send_response(client->fd, "\r\n.\r\n");

    fclose(file);
    return 0;
}



int handle_dele_command(client_state *client, int mail_number) {
    if (!client->authenticated) {
        send_response(client->fd, "-ERR Not logged in\r\n");
        return -1;
    }

    char directory_path[256];
    snprintf(directory_path, sizeof(directory_path), "%s%s/cur/", BASE_DIR, client->username);

    DIR *dir = opendir(directory_path);
    if (dir == NULL) {
        send_response(client->fd, "-ERR Unable to open mail directory\r\n");
        return -1;
    }

    struct dirent *entry;
    int file_count = 0;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        file_count++;
        if (file_count == mail_number) {
            // Check if already marked for deletion
            for (int i = 0; i < client->delete_size; ++i) {
                if (strcmp(client->deleted_files[i], entry->d_name) == 0) {
                    closedir(dir);
                    send_response(client->fd, "-ERR Message already marked for deletion\r\n");
                    return -1;
                }
            }

            // Mark for deletion
            strncpy(client->deleted_files[client->delete_size], entry->d_name, MAX_MESSAGES);
            client->delete_size++;
            closedir(dir);
            send_response(client->fd, "+OK Message marked for deletion\r\n");
            return 0;
        }
    }

    closedir(dir);
    send_response(client->fd, "-ERR Message not found\r\n");
    return -1;
}

void handle_rset_command(client_state *client) {
    log_command_received(client, "RSET", NULL);
    if (!client->authenticated) {
        send_response(client->fd, "-ERR Not logged in\r\n");
        return;
    }

    // Reset all deletion marks and delete_size
    memset(client->deleted_files, 0, sizeof(client->deleted_files));
    client->delete_size = 0;

    send_response(client->fd, "+OK Reset state\r\n");
}


void handle_stat_command(client_state *client) {
    if (!client->authenticated) {
        send_response(client->fd, "-ERR Not logged in\r\n");
        return;
    }

    char user_dir[256];
    ServerConfig *server_config = get_server_config();
    snprintf(user_dir, sizeof(user_dir), "%s%s/cur", server_config->mail_dir, client->username);

    DIR *dir = opendir(user_dir);
    if (!dir) {
        send_response(client->fd, "-ERR Mailbox not found\r\n");
        return;
    }

    struct dirent *entry;
    int count = 0;
    long total_size = 0;

    while ((entry = readdir(dir))) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        char full_path[512];
        snprintf(full_path, sizeof(full_path), "%s/%s", user_dir, entry->d_name);

        if (is_regular_file(full_path)) {
            // Check if the file is marked for deletion
            bool is_deleted = false;
            for (int i = 0; i < client->delete_size; ++i) {
                if (strcmp(client->deleted_files[i], entry->d_name) == 0) {
                    is_deleted = true;
                    break;
                }
            }

            if (is_deleted) {
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
        } 
            else if (client->authenticated) {// Process other commands only if authenticated
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
















