#ifndef VSS_PARSER_H
#define VSS_PARSER_H

#include "lexer.h"
#include "ast.h"

typedef struct {
    Lexer *lexer;
    Token current;
    Token previous;
    bool had_error;
    bool panic_mode;
} Parser;

void parser_init(Parser *parser, Lexer *lexer);
Block parse_program(Parser *parser);

#endif
