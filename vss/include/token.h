#ifndef VSS_TOKEN_H
#define VSS_TOKEN_H

#include "common.h"

typedef enum {
    TOKEN_EOF,
    TOKEN_ERROR,
    TOKEN_NEWLINE,

    TOKEN_IDENTIFIER,
    TOKEN_NUMBER,
    TOKEN_STRING,

    TOKEN_SAY,
    TOKEN_MAKE,
    TOKEN_KEEP,
    TOKEN_BECOMES,
    TOKEN_WHEN,
    TOKEN_ORWHEN,
    TOKEN_OTHERWISE,
    TOKEN_FINISH,
    TOKEN_REPEAT,
    TOKEN_TIMES,
    TOKEN_THROUGH,
    TOKEN_TO,
    TOKEN_EACH,
    TOKEN_IN,
    TOKEN_DURING,
    TOKEN_LEAVE,
    TOKEN_SKIP,
    TOKEN_TASK,
    TOKEN_NEEDS,
    TOKEN_SEND,
    TOKEN_WITH,
    TOKEN_YES,
    TOKEN_NO,
    TOKEN_EMPTY,
    TOKEN_AND,
    TOKEN_OR,
    TOKEN_NOT,
    TOKEN_ABOVE,
    TOKEN_BELOW,
    TOKEN_AT_LEAST,
    TOKEN_AT_MOST,
    TOKEN_SAME_AS,
    TOKEN_NOT_SAME_AS,
    TOKEN_ITEM,
    TOKEN_FIELD,
    TOKEN_PUT,
    TOKEN_INTO,
    TOKEN_MAP,
    TOKEN_SET,
    TOKEN_GRAB,
    TOKEN_ATTEMPT,
    TOKEN_RESCUE,
    TOKEN_READ,
    TOKEN_WRITE,
    TOKEN_ADD,
    TOKEN_ERASE,
    TOKEN_EXISTS,
    TOKEN_SIZE,
    TOKEN_OF,
    TOKEN_CHOOSE,
    TOKEN_CASE,
    TOKEN_NOTE,
    TOKEN_HI,
    TOKEN_BYE,
    TOKEN_HTMVSS,

    TOKEN_PLUS,
    TOKEN_MINUS,
    TOKEN_STAR,
    TOKEN_SLASH,
    TOKEN_PERCENT,
    TOKEN_LEFT_BRACKET,
    TOKEN_RIGHT_BRACKET,
    TOKEN_COMMA,
    TOKEN_COLON
} TokenType;

typedef struct {
    TokenType type;
    const char *start;
    size_t length;
    int line;
    int column;
} Token;

const char *token_type_name(TokenType type);

#endif
