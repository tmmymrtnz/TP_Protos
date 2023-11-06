#ifndef POP3_COMMANDS_H
#define POP3_COMMANDS_H

char* handle_user_command(int client_socket, char* arg);
int handle_pass_command(int client_socket, char *username, char *password);
void handle_list_command(int client_socket, char* username);
void handle_retr_command(int client_socket, char* username, int mail_number);
void handle_dele_command(int client_socket, char* username, int mail_number);

#endif
