#include <ctype.h>
#include <string.h>

#include "lexer.h"

static bool is_at_end(Lexer *lexer) {
    return *lexer->current == '\0';
}

static char advance_char(Lexer *lexer) {
    char c = *lexer->current;
    lexer->current++;
    lexer->column++;
    return c;
}

static char peek_char(Lexer *lexer) {
    return *lexer->current;
}

static char peek_next_char(Lexer *lexer) {
    if (is_at_end(lexer)) {
        return '\0';
    }
    return lexer->current[1];
}

static Token make_token(Lexer *lexer, TokenType type) {
    Token token;
    token.type = type;
    token.start = lexer->start;
    token.length = (size_t)(lexer->current - lexer->start);
    token.line = lexer->line;
    token.column = lexer->token_column;
    return token;
}

static Token error_token(Lexer *lexer, const char *message) {
    Token token;
    token.type = TOKEN_ERROR;
    token.start = message;
    token.length = strlen(message);
    token.line = lexer->line;
    token.column = lexer->token_column;
    return token;
}

static void skip_spaces(Lexer *lexer) {
    for (;;) {
        char c = peek_char(lexer);
        if (c == ' ' || c == '\t' || c == '\r') {
            advance_char(lexer);
        } else {
            break;
        }
    }
}

static bool is_name_start(char c) {
    return isalpha((unsigned char)c) || c == '_';
}

static bool is_name_part(char c) {
    return isalnum((unsigned char)c) || c == '_';
}

static TokenType keyword_type(const char *start, size_t length) {
    struct KeywordEntry {
        const char *text;
        TokenType type;
    };

    static const struct KeywordEntry keywords[] = {
        {"say", TOKEN_SAY},
        {"make", TOKEN_MAKE},
        {"keep", TOKEN_KEEP},
        {"becomes", TOKEN_BECOMES},
        {"when", TOKEN_WHEN},
        {"orwhen", TOKEN_ORWHEN},
        {"otherwise", TOKEN_OTHERWISE},
        {"finish", TOKEN_FINISH},
        {"repeat", TOKEN_REPEAT},
        {"times", TOKEN_TIMES},
        {"through", TOKEN_THROUGH},
        {"to", TOKEN_TO},
        {"each", TOKEN_EACH},
        {"in", TOKEN_IN},
        {"during", TOKEN_DURING},
        {"leave", TOKEN_LEAVE},
        {"skip", TOKEN_SKIP},
        {"task", TOKEN_TASK},
        {"needs", TOKEN_NEEDS},
        {"send", TOKEN_SEND},
        {"with", TOKEN_WITH},
        {"yes", TOKEN_YES},
        {"no", TOKEN_NO},
        {"empty", TOKEN_EMPTY},
        {"and", TOKEN_AND},
        {"or", TOKEN_OR},
        {"not", TOKEN_NOT},
        {"above", TOKEN_ABOVE},
        {"below", TOKEN_BELOW},
        {"at_least", TOKEN_AT_LEAST},
        {"at_most", TOKEN_AT_MOST},
        {"same_as", TOKEN_SAME_AS},
        {"not_same_as", TOKEN_NOT_SAME_AS},
        {"item", TOKEN_ITEM},
        {"field", TOKEN_FIELD},
        {"put", TOKEN_PUT},
        {"into", TOKEN_INTO},
        {"map", TOKEN_MAP},
        {"set", TOKEN_SET},
        {"grab", TOKEN_GRAB},
        {"attempt", TOKEN_ATTEMPT},
        {"rescue", TOKEN_RESCUE},
        {"read", TOKEN_READ},
        {"write", TOKEN_WRITE},
        {"add", TOKEN_ADD},
        {"erase", TOKEN_ERASE},
        {"exists", TOKEN_EXISTS},
        {"size", TOKEN_SIZE},
        {"of", TOKEN_OF},
        {"choose", TOKEN_CHOOSE},
        {"case", TOKEN_CASE},
        {"note", TOKEN_NOTE},
        {"hi", TOKEN_HI},
        {"bye", TOKEN_BYE},
        {"htmvss", TOKEN_HTMVSS}
    };

    size_t count = sizeof(keywords) / sizeof(keywords[0]);
    for (size_t i = 0; i < count; i++) {
        if (strlen(keywords[i].text) == length && strncmp(keywords[i].text, start, length) == 0) {
            return keywords[i].type;
        }
    }
    return TOKEN_IDENTIFIER;
}

static Token identifier(Lexer *lexer) {
    while (is_name_part(peek_char(lexer))) {
        advance_char(lexer);
    }

    TokenType type = keyword_type(lexer->start, (size_t)(lexer->current - lexer->start));
    return make_token(lexer, type);
}

static Token number(Lexer *lexer) {
    while (isdigit((unsigned char)peek_char(lexer))) {
        advance_char(lexer);
    }

    if (peek_char(lexer) == '.' && isdigit((unsigned char)peek_next_char(lexer))) {
        advance_char(lexer);
        while (isdigit((unsigned char)peek_char(lexer))) {
            advance_char(lexer);
        }
    }

    return make_token(lexer, TOKEN_NUMBER);
}

static Token string(Lexer *lexer) {
    while (!is_at_end(lexer) && peek_char(lexer) != '"') {
        if (peek_char(lexer) == '\n') {
            lexer->line++;
            lexer->column = 1;
        }
        if (peek_char(lexer) == '\\' && peek_next_char(lexer) != '\0') {
            advance_char(lexer);
        }
        advance_char(lexer);
    }

    if (is_at_end(lexer)) {
        return error_token(lexer, "Unterminated string.");
    }

    advance_char(lexer);
    return make_token(lexer, TOKEN_STRING);
}

void lexer_init(Lexer *lexer, const char *source) {
    lexer->source = source;
    lexer->start = source;
    lexer->current = source;
    lexer->line = 1;
    lexer->column = 1;
    lexer->token_column = 1;
}

Token lexer_next(Lexer *lexer) {
    skip_spaces(lexer);
    lexer->start = lexer->current;
    lexer->token_column = lexer->column;

    if (is_at_end(lexer)) {
        return make_token(lexer, TOKEN_EOF);
    }

    char c = advance_char(lexer);

    if (c == '\n') {
        Token token = make_token(lexer, TOKEN_NEWLINE);
        lexer->line++;
        lexer->column = 1;
        return token;
    }

    if (is_name_start(c)) {
        Token t = identifier(lexer);
        if (t.type == TOKEN_NOTE) {
            while (!is_at_end(lexer) && peek_char(lexer) != '\n') {
                advance_char(lexer);
            }
            return lexer_next(lexer);
        }
        return t;
    }

    if (isdigit((unsigned char)c)) {
        return number(lexer);
    }

    switch (c) {
        case '"': return string(lexer);
        case '+': return make_token(lexer, TOKEN_PLUS);
        case '-': return make_token(lexer, TOKEN_MINUS);
        case '*': return make_token(lexer, TOKEN_STAR);
        case '/': return make_token(lexer, TOKEN_SLASH);
        case '%': return make_token(lexer, TOKEN_PERCENT);
        case '[': return make_token(lexer, TOKEN_LEFT_BRACKET);
        case ']': return make_token(lexer, TOKEN_RIGHT_BRACKET);
        case ',': return make_token(lexer, TOKEN_COMMA);
        case ':': return make_token(lexer, TOKEN_COLON);
        default: return error_token(lexer, "Unexpected character.");
    }
}
