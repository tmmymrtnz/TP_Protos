#ifndef POP3_COMMANDS_H
#define POP3_COMMANDS_H

#include <stdbool.h>

// Define the client state structure used to manage the state of a POP3 session.
typedef struct {
    int fd;                       // Client socket file descriptor
    char read_buffer[1024];       // Buffer to store incoming data
    char write_buffer[1024];      // Buffer to store outgoing data
    int read_buffer_pos;          // Position in the read buffer
    int write_buffer_pos;         // Position in the write buffer
    bool authenticated;           // Flag indicating if the user is authenticated
    char username[128];           // The authenticated user's username
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

// Processes a command received from the POP3 client.
int process_pop3_command(client_state *client);

#endif // POP3_COMMANDS_H
