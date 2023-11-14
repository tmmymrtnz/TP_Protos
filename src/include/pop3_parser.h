#ifndef POP3_PARSER_H
#define POP3_PARSER_H

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define MAX_COMMAND_LEN 16
#define MAX_ARG_LEN 64

enum pop3_parser_state {
    POP3_PARSE_COMMAND,
    POP3_PARSE_ARG1,
    POP3_PARSE_ARG2,
    POP3_PARSE_DONE,
};

struct pop3_parser {
    enum pop3_parser_state state;
    char command[MAX_COMMAND_LEN];
    size_t command_len;
    char arg1[MAX_ARG_LEN];
    size_t arg1_len;
    char arg2[MAX_ARG_LEN];
    size_t arg2_len;
    bool authenticated;
};

void pop3_parser_init(struct pop3_parser *parser);
enum pop3_parser_state pop3_parser_feed(struct pop3_parser *parser, char c);

#endif
