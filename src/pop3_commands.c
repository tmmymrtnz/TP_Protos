#include "pop3_commands.h"
#include "auth.h"
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <sys/socket.h>
#include <sys/types.h>

#define BASE_DIR "mailserver/"

char* handle_user_command(int client_socket, char* arg) {
    static char stored_username[128];  // static so it retains its value across function calls
    strncpy(stored_username, arg, sizeof(stored_username) - 1);
    send(client_socket, "+OK\r\n", 5, 0);
    return stored_username;
}


int handle_pass_command(int client_socket, char *username, char *password) {
    if (authenticate(username, password)) {
        send(client_socket, "+OK Logged in\r\n", 15, 0);
        return 1; // Authentication successful
    } else {
        send(client_socket, "-ERR Invalid credentials\r\n", 26, 0);
        return 0; // Authentication failed
    }
}

void handle_list_command(int client_socket, char* username) {
    char user_dir[256];
    snprintf(user_dir, sizeof(user_dir), "%s/%s", BASE_DIR, username);

    DIR* dir = opendir(user_dir);
    if (!dir) {
        send(client_socket, "-ERR Cannot open mailbox\r\n", 26, 0);
        return;
    }

    struct dirent* entry;
    int count = 0;
    while ((entry = readdir(dir))) {
        if (entry->d_type == DT_REG) { // If it's a regular file
            count++;
        }
    }

    char response[512];
    snprintf(response, sizeof(response), "+OK %d messages\r\n", count);
    send(client_socket, response, strlen(response), 0);

    closedir(dir);
}

void handle_retr_command(int client_socket, char* username, int mail_number) {
    char mail_file[256];
    snprintf(mail_file, sizeof(mail_file), "%s/%s/mail%d.txt", BASE_DIR, username, mail_number);

    FILE* file = fopen(mail_file, "r");
    if (!file) {
        send(client_socket, "-ERR Message not found\r\n", 23, 0);
        return;
    }

    char buffer[1024];
    size_t bytes_read;
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        send(client_socket, buffer, bytes_read, 0);
    }

    fclose(file);
}

void handle_dele_command(int client_socket, char* username, int mail_number) {
    char mail_file[256];
    snprintf(mail_file, sizeof(mail_file), "%s/%s/mail%d.txt", BASE_DIR, username, mail_number);

    if (remove(mail_file) == 0) {
        send(client_socket, "+OK Message deleted\r\n", 21, 0);
    } else {
        send(client_socket, "-ERR Message not found\r\n", 23, 0);
    }
}
