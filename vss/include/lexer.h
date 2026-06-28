#ifndef VSS_LEXER_H
#define VSS_LEXER_H

#include "token.h"

typedef struct {
    const char *source;
    const char *start;
    const char *current;
    int line;
    int column;
    int token_column;
} Lexer;

void lexer_init(Lexer *lexer, const char *source);
Token lexer_next(Lexer *lexer);

#endif
