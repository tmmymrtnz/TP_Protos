#include "pop3_parser.h"
#include <stdio.h>
#include <string.h>

void pop3_parser_init(struct pop3_parser *parser) {
    memset(parser, 0, sizeof(struct pop3_parser));
    parser->state = POP3_PARSE_COMMAND;
    parser->authenticated = false;
}

bool authenticate_user(const char *username, const char *password) {
    // Replace this with your actual authentication logic
    // For simplicity, always return true in this example
    username = username;
    password = password;
    return true;
}

bool authenticate_password(const char* provided_password) {
    // Compare the provided password with the correct password
    if (strcmp(provided_password, "correct_password") == 0) {
        return true;  // Passwords match, authentication successful
    }
    return false;  // Passwords do not match, authentication failed
}

enum pop3_parser_state pop3_parser_feed(struct pop3_parser *parser, char c) {
    if (parser->state == POP3_PARSE_DONE) {
        if (!parser->authenticated) {
            if (strcmp(parser->command, "USER") == 0) {
                // Handle USER command, set the authenticated flag if successful
                if (authenticate_user(parser->arg1, parser->arg2)) {
                    printf("Authenticated1. %s\n", parser->arg1);
                    printf("Authenticated2. %s\n", parser->arg2);
                    parser->authenticated = true;
                } else {
                    printf("Authentication failed.\n");
                }
            } else if (strcmp(parser->command, "PASS") == 0) {
                // Handle PASS command to authenticate the user
                if (authenticate_password(parser->arg1)) {
                    parser->authenticated = true;
                } else {
                    printf("Authentication failed.\n");
                }
            } else {
                // Reject commands other than USER and PASS if not authenticated
                printf("Authentication required.\n");
                parser->authenticated = false; // Reset authentication state
            }
        } else {
            if (strcmp(parser->command, "USER") == 0) {
                printf("USER command fail\n");
                parser->authenticated = false;
            } else if (strcmp(parser->command, "LIST") == 0) {
                printf("LIST command\n");
            } else if (strcmp(parser->command, "RETR") == 0) {
                printf("RETR command\n");
            } else {
                // Handle other authenticated commands
            }
        }

        // Reset the parser state and buffer lengths
        parser->state = POP3_PARSE_COMMAND;
        parser->command_len = 0;
        parser->arg1_len = 0;
        parser->arg2_len = 0;
    } else {
        switch (parser->state) {
            case POP3_PARSE_COMMAND:
                if (c == ' ') {
                    parser->state = POP3_PARSE_ARG1;
                } else if (c == '\r' || c == '\n') {
                    parser->state = POP3_PARSE_DONE;
                } else if (parser->command_len < MAX_COMMAND_LEN - 1) {
                    parser->command[parser->command_len++] = c;
                }
                break;
            case POP3_PARSE_ARG1:
                if (c == ' ') {
                    parser->state = POP3_PARSE_ARG2;
                } else if (c == '\r' || c == '\n') {
                    parser->state = POP3_PARSE_DONE;
                } else if (parser->arg1_len < MAX_ARG_LEN - 1) {
                    parser->arg1[parser->arg1_len++] = c;
                }
                break;
            case POP3_PARSE_ARG2:
                if (c == '\r' || c == '\n') {
                    parser->state = POP3_PARSE_DONE;
                } else if (parser->arg2_len < MAX_ARG_LEN - 1) {
                    parser->arg2[parser->arg2_len++] = c;
                }
                break;
            case POP3_PARSE_DONE:
            // Handle POP3_PARSE_DONE state
            // (Optionally, you can leave this empty or with a comment if no specific action is needed)
            break;
        }
    }

    return parser->state;
}
