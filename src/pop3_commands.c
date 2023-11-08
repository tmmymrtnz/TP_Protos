#include "pop3_commands.h"
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

#define BASE_DIR "mailserver/"

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
        send(client->fd, "-ERR Username required\r\n", 23, 0);
        return -1; // Return an error
    }

    strncpy(client->username, arg, sizeof(client->username) - 1);
    send(client->fd, "+OK User accepted\r\n", 19, 0);
    return 0; // Success
}

int handle_pass_command(client_state *client, char *password) {
    // Ensure the client has provided a password
    if (!password || strlen(password) == 0) {
        send(client->fd, "-ERR Password required\r\n", 24, 0);
        return -1; // Return an error
    }

    // Authenticate the user
    if (authenticate(client->username, password)) {
        client->authenticated = true;
        send(client->fd, "+OK Logged in\r\n", 15, 0);
        return 0; // Success
    } else {
        send(client->fd, "-ERR Invalid credentials\r\n", 26, 0);
        return -1; // Return an error
    }
}

void handle_list_command(client_state *client) {
    if (!client->authenticated) {
        send(client->fd, "-ERR Not logged in\r\n", 20, 0);
        return;
    }

    char user_dir[256];
    snprintf(user_dir, sizeof(user_dir), "%s%s", BASE_DIR, client->username);

    DIR *dir = opendir(user_dir);
    if (!dir) {
        send(client->fd, "-ERR Cannot open mailbox\r\n", 26, 0);
        return;
    }

    struct dirent *entry;
    char response[512];
    int count = 0;
    char list_response[1024] = "+OK Mailbox scan listing follows\r\n";

    while ((entry = readdir(dir))) {
        char full_path[512];
        snprintf(full_path, sizeof(full_path), "%s/%s", user_dir, entry->d_name);
        if (is_regular_file(full_path)) {
            count++;
            snprintf(response, sizeof(response), "%d %s\r\n", count, entry->d_name);
            strncat(list_response, response, sizeof(list_response) - strlen(list_response) - 1);
        }
    }

    send(client->fd, list_response, strlen(list_response), 0);
    closedir(dir);
}

int handle_retr_command(client_state *client, int mail_number) {
    if (!client->authenticated) {
        send(client->fd, "-ERR Not logged in\r\n", 20, 0);
        return -1;
    }

    char mail_file[256];
    snprintf(mail_file, sizeof(mail_file), "%s%s/mail%d.txt", BASE_DIR, client->username, mail_number);

    FILE *file = fopen(mail_file, "r");
    if (!file) {
        send(client->fd, "-ERR Message not found\r\n", 23, 0);
        return -1;
    }

    send(client->fd, "+OK Message follows\r\n", 21, 0);
    char buffer[1024];
    size_t bytes_read;
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        if (send(client->fd, buffer, bytes_read, 0) == -1) {
            if (errno == EWOULDBLOCK || errno == EAGAIN) {
                // Handle the case where the client's send buffer is full
                // This should ideally be handled with select() to wait until the socket is writable
            } else {
                // Handle other send errors
                perror("send");
                fclose(file);
                return -1;
            }
        }
    }

    fclose(file);
    return 0;
}

int handle_dele_command(client_state *client, int mail_number) {
    if (!client->authenticated) {
        send(client->fd, "-ERR Not logged in\r\n", 20, 0);
        return -1;
    }

    char mail_file[256];
    snprintf(mail_file, sizeof(mail_file), "%s%s/mail%d.txt", BASE_DIR, client->username, mail_number);

    if (remove(mail_file) == 0) {
        send(client->fd, "+OK Message deleted\r\n", 21, 0);
        return 0;
    } else {
        send(client->fd, "-ERR Message not found\r\n", 23, 0);
        return -1;
    }
}

// Helper function to reset the read buffer after processing a command
static void reset_read_buffer(client_state *client) {
    memset(client->read_buffer, 0, sizeof(client->read_buffer));
    client->read_buffer_pos = 0;
}

// Helper function to send a response to the client
static void send_response(int client_socket, const char *response) {
    send(client_socket, response, strlen(response), 0);
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
    printf("Received command: %s\n", client->read_buffer);

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
        // Process the QUIT command
        else if (strncmp(client->read_buffer, "QUIT", 4) == 0) {
            send_response(client->fd, "+OK Goodbye\r\n");
            return -1; // Signal to disconnect the client
        }
        else {
            send_response(client->fd, "-ERR Unknown command\r\n");
        }
    } else {
        send_response(client->fd, "-ERR Please authenticate using USER and PASS commands\r\n");
    }

    // Reset the read buffer for the next command
    reset_read_buffer(client);

    return 0; // Keep the connection open unless QUIT was received
}

