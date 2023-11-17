#ifndef POP3_COMMANDS_H
#define POP3_COMMANDS_H

#include <stdbool.h>

#define BUFFER 1024
#define MAX_MESSAGES 128
#define USERNAME_SIZE 128

// Define the client state structure used to manage the state of a POP3 session.
typedef struct {
    int fd;                       // Client socket file descriptor
    char read_buffer[BUFFER];       // Buffer to store incoming data
    char write_buffer[BUFFER];      // Buffer to store outgoing data
    int read_buffer_pos;          // Position in the read buffer
    int write_buffer_pos;         // Position in the write buffer
    bool authenticated;           // Flag indicating if the user is authenticated
    char username[USERNAME_SIZE];           // The authenticated user's username
    bool deleted_messages[MAX_MESSAGES];      // Array of flags indicating if a mail has been deleted
    // ... other fields for state management
} client_state;

// Checks if the given path is a regular file.
int is_regular_file(const char *path);

// Handles the USER command from the POP3 client.
int handle_user_command(client_state *client, char *arg);

// Handles the PASS command from the POP3 client.
int handle_pass_command(client_state *client, char *password);

// Handles the LIST command from the POP3 client.
void handle_list_command(client_state *client);

// Handles the RETR command from the POP3 client.
int handle_retr_command(client_state *client, int mail_number);

// Handles the DELE command from the POP3 client.
int handle_dele_command(client_state *client, int mail_number);

void handle_rset_command(client_state *client);

void handle_stat_command(client_state *client);

void handle_capa_command(client_state *client);

void send_response(int client_socket, const char *response);

// Processes a command received from the POP3 client.
int process_pop3_command(client_state *client);

#endif // POP3_COMMANDS_H
