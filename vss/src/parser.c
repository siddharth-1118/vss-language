#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parser.h"

static void error_at(VSS_Parser *parser, VSS_Token *token, const char *message) {
    if (parser->panic_mode) return;
    parser->panic_mode = true;
    parser->had_error = true;

    fprintf(stderr, "error line %d, col %d: ", token->line, token->column);
    if (token->type == VSS_TOKEN_EOF) {
        fprintf(stderr, "at end of file: ");
    } else if (token->type == VSS_TOKEN_ERROR) {
        // Already an error token from lexer
    } else {
        fprintf(stderr, "at '%.*s': ", (int)token->length, token->start);
    }
    fprintf(stderr, "%s\n", message);
}

static void advance(VSS_Parser *parser) {
    parser->previous = parser->current;

    for (;;) {
        parser->current = vss_lexer_next(parser->lexer);
        if (parser->current.type != VSS_TOKEN_ERROR) break;
        error_at(parser, &parser->current, parser->current.start);
    }
}

static bool check(VSS_Parser *parser, VSS_TokenType type) {
    return parser->current.type == type;
}

static bool match(VSS_Parser *parser, VSS_TokenType type) {
    if (!check(parser, type)) return false;
    advance(parser);
    return true;
}

static void consume(VSS_Parser *parser, VSS_TokenType type, const char *message) {
    if (parser->current.type == type) {
        advance(parser);
        return;
    }
    error_at(parser, &parser->current, message);
}

static bool is_newline_or_eof(VSS_Parser *parser) {
    return check(parser, VSS_TOKEN_NEWLINE) || check(parser, VSS_TOKEN_EOF);
}

static bool can_start_unary(VSS_TokenType type) {
    switch (type) {
        case VSS_TOKEN_NUMBER:
        case VSS_TOKEN_STRING:
        case VSS_TOKEN_YES:
        case VSS_TOKEN_NO:
        case VSS_TOKEN_EMPTY:
        case VSS_TOKEN_IDENTIFIER:
        case VSS_TOKEN_LEFT_BRACKET:
        case VSS_TOKEN_MAP:
        case VSS_TOKEN_NOT:
        case VSS_TOKEN_MINUS:
        case VSS_TOKEN_SIZE:
        case VSS_TOKEN_EXISTS:
        case VSS_TOKEN_READ:
        case VSS_TOKEN_MINE:
        case VSS_TOKEN_PARENT:
            return true;
        default:
            return false;
    }
}

static bool match_becomes_or_equal(VSS_Parser *parser) {
    return match(parser, VSS_TOKEN_BECOMES) || match(parser, VSS_TOKEN_EQUAL);
}

static void consume_becomes_or_equal(VSS_Parser *parser, const char *message) {
    if (match_becomes_or_equal(parser)) return;
    error_at(parser, &parser->current, message);
}

// Forward declarations of expression parsers
static VSS_Expr *parse_expression(VSS_Parser *parser);
static char *parse_string_value(const char *start, size_t length);

static VSS_Expr *parse_interpolated_string(VSS_Parser *parser, const char *str, int line, int column) {
    const char *p = str;
    const char *start = str;
    VSS_Expr *expr = NULL;

    while (*p) {
        if (*p == '{') {
            size_t len = p - start;
            if (len > 0) {
                char *segment = malloc(len + 1);
                memcpy(segment, start, len);
                segment[len] = '\0';
                VSS_Expr *seg_expr = vss_expr_new_string(segment, line, column);
                free(segment);
                if (!expr) {
                    expr = seg_expr;
                } else {
                    expr = vss_expr_new_binary(VSS_TOKEN_PLUS, expr, seg_expr, line, column);
                }
            }
            
            p++;
            const char *expr_start = p;
            int braces = 1;
            while (*p && braces > 0) {
                if (*p == '{') braces++;
                else if (*p == '}') braces--;
                if (braces > 0) p++;
            }
            if (*p != '}') {
                error_at(parser, &parser->current, "Unterminated interpolation bracket.");
                break;
            }
            
            size_t expr_len = p - expr_start;
            char *expr_str = malloc(expr_len + 1);
            memcpy(expr_str, expr_start, expr_len);
            expr_str[expr_len] = '\0';
            
            VSS_Lexer temp_lexer;
            vss_lexer_init(&temp_lexer, expr_str);
            VSS_Parser temp_parser;
            vss_parser_init(&temp_parser, &temp_lexer);
            VSS_Expr *inner_expr = parse_expression(&temp_parser);
            free(expr_str);
            
            if (temp_parser.had_error) {
                error_at(parser, &parser->current, "Error parsing expression in interpolation.");
            } else {
                if (!expr) {
                    expr = inner_expr;
                } else {
                    expr = vss_expr_new_binary(VSS_TOKEN_PLUS, expr, inner_expr, line, column);
                }
            }
            
            p++;
            start = p;
        } else {
            p++;
        }
    }
    
    if (*start) {
        VSS_Expr *seg_expr = vss_expr_new_string(start, line, column);
        if (!expr) {
            expr = seg_expr;
        } else {
            expr = vss_expr_new_binary(VSS_TOKEN_PLUS, expr, seg_expr, line, column);
        }
    }
    
    if (!expr) {
        expr = vss_expr_new_string("", line, column);
    }
    return expr;
}

static VSS_Expr *parse_expression(VSS_Parser *parser);
static VSS_Expr *parse_or(VSS_Parser *parser);
static VSS_Expr *parse_and(VSS_Parser *parser);
static VSS_Expr *parse_equality(VSS_Parser *parser);
static VSS_Expr *parse_comparison(VSS_Parser *parser);
static VSS_Expr *parse_term(VSS_Parser *parser);
static VSS_Expr *parse_factor(VSS_Parser *parser);
static VSS_Expr *parse_unary(VSS_Parser *parser);
static VSS_Expr *parse_postfix(VSS_Parser *parser);
static VSS_Expr *parse_primary(VSS_Parser *parser);

static VSS_Stmt *parse_statement(VSS_Parser *parser);
static VSS_Block parse_block(VSS_Parser *parser);

static char *safe_strdup(const char *s) {
    if (!s) return NULL;
    char *dup = malloc(strlen(s) + 1);
    if (dup) {
        strcpy(dup, s);
    }
    return dup;
}

static VSS_Block parse_block(VSS_Parser *parser) {
    while (match(parser, VSS_TOKEN_NEWLINE));
    
    VSS_Stmt **statements = NULL;
    size_t count = 0;
    
    while (!check(parser, VSS_TOKEN_FINISH) && 
           !check(parser, VSS_TOKEN_ORWHEN) && 
           !check(parser, VSS_TOKEN_OTHERWISE) && 
           !check(parser, VSS_TOKEN_CASE) && 
           !check(parser, VSS_TOKEN_RESCUE) && 
           !check(parser, VSS_TOKEN_EOF)) {
        VSS_Stmt *stmt = parse_statement(parser);
        if (stmt) {
            statements = realloc(statements, sizeof(VSS_Stmt*) * (count + 1));
            statements[count++] = stmt;
        }
        
        if (check(parser, VSS_TOKEN_EOF)) break;
        
        if (!match(parser, VSS_TOKEN_NEWLINE)) {
            error_at(parser, &parser->current, "Expected newline after statement in block.");
            while (!is_newline_or_eof(parser)) {
                advance(parser);
            }
            if (check(parser, VSS_TOKEN_NEWLINE)) advance(parser);
        }
        while (match(parser, VSS_TOKEN_NEWLINE));
        parser->panic_mode = false;
    }
    
    VSS_Block b;
    b.statements = statements;
    b.count = count;
    return b;
}

static VSS_Expr *parse_expression(VSS_Parser *parser) {
    return parse_or(parser);
}

static VSS_Expr *parse_or(VSS_Parser *parser) {
    VSS_Expr *expr = parse_and(parser);
    while (match(parser, VSS_TOKEN_OR)) {
        VSS_Token op = parser->previous;
        VSS_Expr *right = parse_and(parser);
        expr = vss_expr_new_binary(op.type, expr, right, op.line, op.column);
    }
    return expr;
}

static VSS_Expr *parse_and(VSS_Parser *parser) {
    VSS_Expr *expr = parse_equality(parser);
    while (match(parser, VSS_TOKEN_AND)) {
        VSS_Token op = parser->previous;
        VSS_Expr *right = parse_equality(parser);
        expr = vss_expr_new_binary(op.type, expr, right, op.line, op.column);
    }
    return expr;
}

static VSS_Expr *parse_equality(VSS_Parser *parser) {
    VSS_Expr *expr = parse_comparison(parser);
    while (match(parser, VSS_TOKEN_SAME_AS) || match(parser, VSS_TOKEN_NOT_SAME_AS)) {
        VSS_Token op = parser->previous;
        VSS_Expr *right = parse_comparison(parser);
        expr = vss_expr_new_binary(op.type, expr, right, op.line, op.column);
    }
    return expr;
}

static VSS_Expr *parse_comparison(VSS_Parser *parser) {
    VSS_Expr *expr = parse_term(parser);
    while (match(parser, VSS_TOKEN_ABOVE) || match(parser, VSS_TOKEN_BELOW) ||
           match(parser, VSS_TOKEN_AT_LEAST) || match(parser, VSS_TOKEN_AT_MOST)) {
        VSS_Token op = parser->previous;
        VSS_Expr *right = parse_term(parser);
        expr = vss_expr_new_binary(op.type, expr, right, op.line, op.column);
    }
    return expr;
}

static VSS_Expr *parse_term(VSS_Parser *parser) {
    VSS_Expr *expr = parse_factor(parser);
    while (match(parser, VSS_TOKEN_PLUS) || match(parser, VSS_TOKEN_MINUS)) {
        VSS_Token op = parser->previous;
        VSS_Expr *right = parse_factor(parser);
        expr = vss_expr_new_binary(op.type, expr, right, op.line, op.column);
    }
    return expr;
}

static VSS_Expr *parse_factor(VSS_Parser *parser) {
    VSS_Expr *expr = parse_unary(parser);
    while (match(parser, VSS_TOKEN_STAR) || match(parser, VSS_TOKEN_SLASH) || match(parser, VSS_TOKEN_PERCENT)) {
        VSS_Token op = parser->previous;
        VSS_Expr *right = parse_unary(parser);
        expr = vss_expr_new_binary(op.type, expr, right, op.line, op.column);
    }
    return expr;
}

static VSS_Expr *parse_unary(VSS_Parser *parser) {
    if (match(parser, VSS_TOKEN_NOT) || match(parser, VSS_TOKEN_MINUS)) {
        VSS_Token op = parser->previous;
        VSS_Expr *operand = parse_unary(parser);
        return vss_expr_new_unary(op.type, operand, op.line, op.column);
    }
    return parse_postfix(parser);
}

static VSS_Expr *parse_postfix(VSS_Parser *parser) {
    VSS_Expr *expr = parse_primary(parser);
    for (;;) {
        if (match(parser, VSS_TOKEN_ITEM)) {
            VSS_Token op = parser->previous;
            VSS_Expr *index = parse_unary(parser);
            expr = vss_expr_new_item_access(expr, index, op.line, op.column);
        } else if (match(parser, VSS_TOKEN_FIELD)) {
            VSS_Token op = parser->previous;
            VSS_Expr *field = parse_unary(parser);
            expr = vss_expr_new_field_access(expr, field, op.line, op.column);
        } else if (match(parser, VSS_TOKEN_DOT)) {
            VSS_Token op = parser->previous;
            consume(parser, VSS_TOKEN_IDENTIFIER, "Expected field or method name after '.'.");
            char *field_name = parse_string_value(parser->previous.start, parser->previous.length);
            VSS_Expr *field = vss_expr_new_string(field_name, parser->previous.line, parser->previous.column);
            free(field_name);
            expr = vss_expr_new_field_access(expr, field, op.line, op.column);
        } else if (match(parser, VSS_TOKEN_LEFT_PAREN)) {
            VSS_Token op = parser->previous;
            VSS_Expr **args = NULL;
            size_t count = 0;
            if (!check(parser, VSS_TOKEN_RIGHT_PAREN)) {
                do {
                    VSS_Expr *arg = parse_expression(parser);
                    args = realloc(args, sizeof(VSS_Expr*) * (count + 1));
                    args[count++] = arg;
                } while (match(parser, VSS_TOKEN_COMMA));
            }
            consume(parser, VSS_TOKEN_RIGHT_PAREN, "Expected ')' after arguments.");
            expr = vss_expr_new_call(expr, args, count, op.line, op.column);
        } else if (match(parser, VSS_TOKEN_WITH)) {
            VSS_Token op = parser->previous;
            VSS_Expr **args = NULL;
            size_t count = 0;
            // Parse arguments as space-separated unary expressions
            while (can_start_unary(parser->current.type) && !is_newline_or_eof(parser)) {
                VSS_Expr *arg = parse_unary(parser);
                args = realloc(args, sizeof(VSS_Expr*) * (count + 1));
                args[count++] = arg;
            }
            expr = vss_expr_new_call(expr, args, count, op.line, op.column);
        } else if (can_start_unary(parser->current.type) && !is_newline_or_eof(parser) &&
                   expr->kind != VSS_EXPR_NUMBER &&
                   expr->kind != VSS_EXPR_STRING &&
                   expr->kind != VSS_EXPR_BOOL &&
                   expr->kind != VSS_EXPR_EMPTY &&
                   expr->kind != VSS_EXPR_LIST &&
                   expr->kind != VSS_EXPR_MAP) {
            VSS_Expr **args = NULL;
            size_t count = 0;
            int line = parser->current.line;
            int col = parser->current.column;
            while (can_start_unary(parser->current.type) && !is_newline_or_eof(parser)) {
                VSS_Expr *arg = parse_unary(parser);
                args = realloc(args, sizeof(VSS_Expr*) * (count + 1));
                args[count++] = arg;
            }
            expr = vss_expr_new_call(expr, args, count, line, col);
        } else {
            break;
        }
    }
    return expr;
}

// Parse string characters, handling escape sequences
static char *parse_string_value(const char *start, size_t length) {
    // Strip double quotes
    if (length >= 2 && start[0] == '"' && start[length - 1] == '"') {
        start++;
        length -= 2;
    }
    
    char *result = malloc(length + 1);
    size_t r = 0;
    for (size_t i = 0; i < length; i++) {
        if (start[i] == '\\' && i + 1 < length) {
            i++;
            switch (start[i]) {
                case 'n': result[r++] = '\n'; break;
                case 't': result[r++] = '\t'; break;
                case '"': result[r++] = '"'; break;
                case '\\': result[r++] = '\\'; break;
                default: result[r++] = start[i]; break;
            }
        } else {
            result[r++] = start[i];
        }
    }
    result[r] = '\0';
    return result;
}

static VSS_Expr *parse_primary(VSS_Parser *parser) {
    if (match(parser, VSS_TOKEN_LEFT_PAREN)) {
        VSS_Expr *expr = parse_expression(parser);
        consume(parser, VSS_TOKEN_RIGHT_PAREN, "Expected ')' after expression.");
        return expr;
    }
    if (match(parser, VSS_TOKEN_NUMBER)) {
        double val = strtod(parser->previous.start, NULL);
        return vss_expr_new_number(val, parser->previous.line, parser->previous.column);
    }
    if (match(parser, VSS_TOKEN_STRING)) {
        char *str = parse_string_value(parser->previous.start, parser->previous.length);
        VSS_Expr *expr = NULL;
        if (strchr(str, '{')) {
            expr = parse_interpolated_string(parser, str, parser->previous.line, parser->previous.column);
        } else {
            expr = vss_expr_new_string(str, parser->previous.line, parser->previous.column);
        }
        free(str);
        return expr;
    }
    if (match(parser, VSS_TOKEN_MINE)) {
        return vss_expr_new_mine(parser->previous.line, parser->previous.column);
    }
    if (match(parser, VSS_TOKEN_PARENT)) {
        return vss_expr_new_parent(parser->previous.line, parser->previous.column);
    }
    if (match(parser, VSS_TOKEN_YES)) {
        return vss_expr_new_bool(true, parser->previous.line, parser->previous.column);
    }
    if (match(parser, VSS_TOKEN_NO)) {
        return vss_expr_new_bool(false, parser->previous.line, parser->previous.column);
    }
    if (match(parser, VSS_TOKEN_EMPTY)) {
        return vss_expr_new_empty(parser->previous.line, parser->previous.column);
    }
    if (match(parser, VSS_TOKEN_IDENTIFIER)) {
        char *name = malloc(parser->previous.length + 1);
        memcpy(name, parser->previous.start, parser->previous.length);
        name[parser->previous.length] = '\0';
        VSS_Expr *expr = vss_expr_new_name(name, parser->previous.line, parser->previous.column);
        free(name);
        return expr;
    }
    
    // Built-ins desugared directly
    if (match(parser, VSS_TOKEN_SIZE)) {
        VSS_Token op = parser->previous;
        consume(parser, VSS_TOKEN_OF, "Expected 'of' after 'size'.");
        VSS_Expr *collection = parse_unary(parser);
        VSS_Expr **args = malloc(sizeof(VSS_Expr*));
        args[0] = collection;
        return vss_expr_new_call(vss_expr_new_name("__size", op.line, op.column), args, 1, op.line, op.column);
    }
    if (match(parser, VSS_TOKEN_EXISTS)) {
        VSS_Token op = parser->previous;
        VSS_Expr *path = parse_unary(parser);
        VSS_Expr **args = malloc(sizeof(VSS_Expr*));
        args[0] = path;
        return vss_expr_new_call(vss_expr_new_name("__exists", op.line, op.column), args, 1, op.line, op.column);
    }
    if (match(parser, VSS_TOKEN_READ)) {
        VSS_Token op = parser->previous;
        VSS_Expr *path = parse_unary(parser);
        VSS_Expr **args = malloc(sizeof(VSS_Expr*));
        args[0] = path;
        return vss_expr_new_call(vss_expr_new_name("__read", op.line, op.column), args, 1, op.line, op.column);
    }
    
    // List Literal
    if (match(parser, VSS_TOKEN_LEFT_BRACKET)) {
        VSS_Token op = parser->previous;
        VSS_Expr **elements = NULL;
        size_t count = 0;
        
        while (match(parser, VSS_TOKEN_NEWLINE)); // Skip newlines inside bracket
        
        if (!check(parser, VSS_TOKEN_RIGHT_BRACKET)) {
            do {
                while (match(parser, VSS_TOKEN_NEWLINE));
                VSS_Expr *elem = parse_expression(parser);
                elements = realloc(elements, sizeof(VSS_Expr*) * (count + 1));
                elements[count++] = elem;
                while (match(parser, VSS_TOKEN_NEWLINE));
            } while (match(parser, VSS_TOKEN_COMMA));
        }
        
        while (match(parser, VSS_TOKEN_NEWLINE));
        consume(parser, VSS_TOKEN_RIGHT_BRACKET, "Expected ']' at the end of list literal.");
        return vss_expr_new_list(elements, count, op.line, op.column);
    }
    
    // Map Literal
    if (match(parser, VSS_TOKEN_MAP)) {
        VSS_Token op = parser->previous;
        consume(parser, VSS_TOKEN_LEFT_BRACKET, "Expected '[' after 'map'.");
        
        char **keys = NULL;
        VSS_Expr **values = NULL;
        size_t count = 0;
        
        while (match(parser, VSS_TOKEN_NEWLINE));
        
        while (!check(parser, VSS_TOKEN_RIGHT_BRACKET) && !check(parser, VSS_TOKEN_EOF)) {
            consume(parser, VSS_TOKEN_STRING, "Expected string key in map literal.");
            char *key = parse_string_value(parser->previous.start, parser->previous.length);
            
            consume(parser, VSS_TOKEN_COLON, "Expected ':' after key in map entry.");
            VSS_Expr *val = parse_expression(parser);
            
            keys = realloc(keys, sizeof(char*) * (count + 1));
            values = realloc(values, sizeof(VSS_Expr*) * (count + 1));
            keys[count] = key;
            values[count] = val;
            count++;
            
            while (match(parser, VSS_TOKEN_NEWLINE));
        }
        
        consume(parser, VSS_TOKEN_RIGHT_BRACKET, "Expected ']' at the end of map literal.");
        return vss_expr_new_map(keys, values, count, op.line, op.column);
    }
    
    error_at(parser, &parser->current, "Expected expression.");
    return NULL;
}

static VSS_Stmt *parse_statement(VSS_Parser *parser) {
    // Variable definition: make
    if (match(parser, VSS_TOKEN_MAKE)) {
        VSS_Token op = parser->previous;
        consume(parser, VSS_TOKEN_IDENTIFIER, "Expected variable name after 'make'.");
        char *name = parse_string_value(parser->previous.start, parser->previous.length);
        char *type_name = NULL;
        if (match(parser, VSS_TOKEN_COLON)) {
            consume(parser, VSS_TOKEN_IDENTIFIER, "Expected type name after ':'.");
            type_name = parse_string_value(parser->previous.start, parser->previous.length);
        }
        consume_becomes_or_equal(parser, "Expected 'becomes' or '=' after variable name.");
        VSS_Expr *init = parse_expression(parser);
        VSS_Stmt *stmt = vss_stmt_new_make(name, type_name, init, op.line, op.column);
        free(name);
        if (type_name) free(type_name);
        return stmt;
    }
    
    // Constant definition: keep
    if (match(parser, VSS_TOKEN_KEEP)) {
        VSS_Token op = parser->previous;
        consume(parser, VSS_TOKEN_IDENTIFIER, "Expected constant name after 'keep'.");
        char *name = parse_string_value(parser->previous.start, parser->previous.length);
        char *type_name = NULL;
        if (match(parser, VSS_TOKEN_COLON)) {
            consume(parser, VSS_TOKEN_IDENTIFIER, "Expected type name after ':'.");
            type_name = parse_string_value(parser->previous.start, parser->previous.length);
        }
        consume_becomes_or_equal(parser, "Expected 'becomes' or '=' after constant name.");
        VSS_Expr *init = parse_expression(parser);
        VSS_Stmt *stmt = vss_stmt_new_keep(name, type_name, init, op.line, op.column);
        free(name);
        if (type_name) free(type_name);
        return stmt;
    }
    
    // Output: say
    if (match(parser, VSS_TOKEN_SAY)) {
        VSS_Token op = parser->previous;
        VSS_Expr *expr = parse_expression(parser);
        return vss_stmt_new_say(expr, op.line, op.column);
    }
    
    // Return: send
    if (match(parser, VSS_TOKEN_SEND)) {
        VSS_Token op = parser->previous;
        VSS_Expr *expr = parse_expression(parser);
        return vss_stmt_new_send(expr, op.line, op.column);
    }
    
    // Loop control: leave
    if (match(parser, VSS_TOKEN_LEAVE)) {
        return vss_stmt_new_leave(parser->previous.line, parser->previous.column);
    }
    
    // Loop control: skip
    if (match(parser, VSS_TOKEN_SKIP)) {
        return vss_stmt_new_skip(parser->previous.line, parser->previous.column);
    }
    
    // Import: grab
    if (match(parser, VSS_TOKEN_GRAB)) {
        VSS_Token op = parser->previous;
        consume(parser, VSS_TOKEN_IDENTIFIER, "Expected module name after 'grab'.");
        char *name = parse_string_value(parser->previous.start, parser->previous.length);
        VSS_Stmt *stmt = vss_stmt_new_grab(name, op.line, op.column);
        free(name);
        return stmt;
    }
    
    // File built-ins desugared into call statements
    if (match(parser, VSS_TOKEN_WRITE)) {
        VSS_Token op = parser->previous;
        VSS_Expr *content = parse_expression(parser);
        consume(parser, VSS_TOKEN_INTO, "Expected 'into' after content in 'write' statement.");
        VSS_Expr *path = parse_expression(parser);
        VSS_Expr **args = malloc(sizeof(VSS_Expr*) * 2);
        args[0] = content;
        args[1] = path;
        return vss_stmt_new_expr(vss_expr_new_call(vss_expr_new_name("__write", op.line, op.column), args, 2, op.line, op.column), op.line, op.column);
    }
    if (match(parser, VSS_TOKEN_ADD)) {
        VSS_Token op = parser->previous;
        VSS_Expr *content = parse_expression(parser);
        consume(parser, VSS_TOKEN_INTO, "Expected 'into' after content in 'add' statement.");
        VSS_Expr *path = parse_expression(parser);
        VSS_Expr **args = malloc(sizeof(VSS_Expr*) * 2);
        args[0] = content;
        args[1] = path;
        return vss_stmt_new_expr(vss_expr_new_call(vss_expr_new_name("__add", op.line, op.column), args, 2, op.line, op.column), op.line, op.column);
    }
    if (match(parser, VSS_TOKEN_ERASE)) {
        VSS_Token op = parser->previous;
        VSS_Expr *path = parse_expression(parser);
        VSS_Expr **args = malloc(sizeof(VSS_Expr*));
        args[0] = path;
        return vss_stmt_new_expr(vss_expr_new_call(vss_expr_new_name("__erase", op.line, op.column), args, 1, op.line, op.column), op.line, op.column);
    }
    
    // List collection update: put
    if (match(parser, VSS_TOKEN_PUT)) {
        VSS_Token op = parser->previous;
        VSS_Expr *val = parse_expression(parser);
        consume(parser, VSS_TOKEN_INTO, "Expected 'into' after value in 'put' statement.");
        VSS_Expr *list = parse_expression(parser);
        return vss_stmt_new_put(val, list, op.line, op.column);
    }
    
    // Map collection update: set
    if (match(parser, VSS_TOKEN_SET)) {
        VSS_Token op = parser->previous;
        VSS_Expr *target = parse_expression(parser);
        if (target->kind != VSS_EXPR_FIELD_ACCESS) {
            error_at(parser, &parser->previous, "Expected map field access after 'set'.");
        }
        consume_becomes_or_equal(parser, "Expected 'becomes' or '=' after map field access in 'set' statement.");
        VSS_Expr *val = parse_expression(parser);
        VSS_Expr *map = target->as.field_access.map;
        VSS_Expr *field = target->as.field_access.field;
        free(target);
        return vss_stmt_new_set_field(map, field, val, op.line, op.column);
    }
    
    if (match(parser, VSS_TOKEN_HI)) {
        VSS_Token op = parser->previous;
        consume(parser, VSS_TOKEN_HTMVSS, "Expected 'htmvss' after 'hi'.");
        return vss_stmt_new_hi_htmvss(op.line, op.column);
    }
    
    if (match(parser, VSS_TOKEN_BYE)) {
        VSS_Token op = parser->previous;
        consume(parser, VSS_TOKEN_HTMVSS, "Expected 'htmvss' after 'bye'.");
        return vss_stmt_new_bye_htmvss(op.line, op.column);
    }
    
    // Conditional: when
    if (match(parser, VSS_TOKEN_WHEN)) {
        VSS_Token op = parser->previous;
        VSS_WhenBranch *branches = NULL;
        size_t count = 0;
        
        VSS_Expr *cond = parse_expression(parser);
        VSS_Block block = parse_block(parser);
        
        branches = realloc(branches, sizeof(VSS_WhenBranch) * (count + 1));
        branches[count].condition = cond;
        branches[count].block = block;
        count++;
        
        while (match(parser, VSS_TOKEN_ORWHEN)) {
            VSS_Expr *ocond = parse_expression(parser);
            VSS_Block oblock = parse_block(parser);
            branches = realloc(branches, sizeof(VSS_WhenBranch) * (count + 1));
            branches[count].condition = ocond;
            branches[count].block = oblock;
            count++;
        }
        
        VSS_Block otherwise_branch = {NULL, 0};
        if (match(parser, VSS_TOKEN_OTHERWISE)) {
            otherwise_branch = parse_block(parser);
        }
        
        consume(parser, VSS_TOKEN_FINISH, "Expected 'finish' to end 'when' block.");
        return vss_stmt_new_when(branches, count, otherwise_branch, op.line, op.column);
    }
    
    // Loops: repeat
    if (match(parser, VSS_TOKEN_REPEAT)) {
        VSS_Token op = parser->previous;
        
        if (match(parser, VSS_TOKEN_EACH)) {
            // each loop
            consume(parser, VSS_TOKEN_IDENTIFIER, "Expected variable name after 'each'.");
            char *var_name = parse_string_value(parser->previous.start, parser->previous.length);
            consume(parser, VSS_TOKEN_IN, "Expected 'in' after variable name in 'each' loop.");
            VSS_Expr *collection = parse_expression(parser);
            VSS_Block body = parse_block(parser);
            consume(parser, VSS_TOKEN_FINISH, "Expected 'finish' to end 'repeat each' loop.");
            VSS_Stmt *stmt = vss_stmt_new_repeat_each(var_name, collection, body, op.line, op.column);
            free(var_name);
            return stmt;
        } else {
            // Check if range loop or count loop
            // Since we need to look ahead without consuming (unless it's an identifier),
            // if current is IDENTIFIER and token after it is THROUGH:
            // But we don't have standard lookahead for two tokens. Wait!
            // We can just parse the next expression. If that expression is a name (VSS_EXPR_NAME) and the current token is VSS_TOKEN_THROUGH:
            // This is super clever! We parse the expression. If it is VSS_EXPR_NAME, and we match VSS_TOKEN_THROUGH, we know it's a range loop!
            // If it's anything else, it's a count loop!
            // Wait, does that work?
            // E.g., `repeat i through 1 to 5` -> first we parse `i` as expression. It returns `VSS_EXPR_NAME` for `i`.
            // Then we see `through`! We match `through`. This is perfect!
            // What if it is `repeat 5 times`? First we parse `5` as expression. It returns `VSS_EXPR_NUMBER` for `5`.
            // The next token is `times`. We do NOT match `through`. So we fall back to count loop!
            // This is absolutely brilliant and doesn't require looking ahead in the token stream at all!
            VSS_Expr *first_expr = parse_expression(parser);
            if (first_expr->kind == VSS_EXPR_NAME && match(parser, VSS_TOKEN_THROUGH)) {
                char *var_name = safe_strdup(first_expr->as.name);
                vss_expr_free(first_expr);
                
                VSS_Expr *start = parse_expression(parser);
                consume(parser, VSS_TOKEN_TO, "Expected 'to' in range loop.");
                VSS_Expr *end = parse_expression(parser);
                VSS_Block body = parse_block(parser);
                consume(parser, VSS_TOKEN_FINISH, "Expected 'finish' to end range loop.");
                VSS_Stmt *stmt = vss_stmt_new_repeat_range(var_name, start, end, body, op.line, op.column);
                free(var_name);
                return stmt;
            } else {
                consume(parser, VSS_TOKEN_TIMES, "Expected 'times' after count expression in repeat loop.");
                VSS_Block body = parse_block(parser);
                consume(parser, VSS_TOKEN_FINISH, "Expected 'finish' to end repeat loop.");
                return vss_stmt_new_repeat_count(first_expr, body, op.line, op.column);
            }
        }
    }
    
    // Loops: during
    if (match(parser, VSS_TOKEN_DURING)) {
        VSS_Token op = parser->previous;
        VSS_Expr *cond = parse_expression(parser);
        VSS_Block body = parse_block(parser);
        consume(parser, VSS_TOKEN_FINISH, "Expected 'finish' to end 'during' loop.");
        return vss_stmt_new_during(cond, body, op.line, op.column);
    }
    
    // Task Definition: task
    if (match(parser, VSS_TOKEN_TASK)) {
        VSS_Token op = parser->previous;
        consume(parser, VSS_TOKEN_IDENTIFIER, "Expected task name after 'task'.");
        char *name = parse_string_value(parser->previous.start, parser->previous.length);
        
        char **params = NULL;
        size_t count = 0;
        if (match(parser, VSS_TOKEN_NEEDS)) {
            while (match(parser, VSS_TOKEN_IDENTIFIER)) {
                char *p = parse_string_value(parser->previous.start, parser->previous.length);
                params = realloc(params, sizeof(char*) * (count + 1));
                params[count++] = p;
            }
        }
        
        VSS_Block body = parse_block(parser);
        consume(parser, VSS_TOKEN_FINISH, "Expected 'finish' to end task definition.");
        VSS_Stmt *stmt = vss_stmt_new_task(name, params, count, body, op.line, op.column);
        free(name);
        return stmt;
    }
    
    // Error Handling: attempt
    if (match(parser, VSS_TOKEN_ATTEMPT)) {
        VSS_Token op = parser->previous;
        VSS_Block try_body = parse_block(parser);
        consume(parser, VSS_TOKEN_RESCUE, "Expected 'rescue' after 'attempt' block.");
        consume(parser, VSS_TOKEN_IDENTIFIER, "Expected problem variable name after 'rescue'.");
        char *prob_var = parse_string_value(parser->previous.start, parser->previous.length);
        VSS_Block rescue_body = parse_block(parser);
        consume(parser, VSS_TOKEN_FINISH, "Expected 'finish' to end 'attempt' block.");
        VSS_Stmt *stmt = vss_stmt_new_attempt(try_body, prob_var, rescue_body, op.line, op.column);
        free(prob_var);
        return stmt;
    }
    
    // Choose: choose
    if (match(parser, VSS_TOKEN_CHOOSE)) {
        VSS_Token op = parser->previous;
        VSS_Expr *expr = parse_expression(parser);
        
        while (match(parser, VSS_TOKEN_NEWLINE));
        
        VSS_ChooseCase *cases = NULL;
        size_t count = 0;
        
        while (match(parser, VSS_TOKEN_CASE)) {
            VSS_Expr *case_expr = parse_expression(parser);
            VSS_Block case_block = parse_block(parser);
            cases = realloc(cases, sizeof(VSS_ChooseCase) * (count + 1));
            cases[count].expr = case_expr;
            cases[count].block = case_block;
            count++;
            while (match(parser, VSS_TOKEN_NEWLINE));
        }
        
        VSS_Block otherwise_branch = {NULL, 0};
        if (match(parser, VSS_TOKEN_OTHERWISE)) {
            otherwise_branch = parse_block(parser);
        }
        
        consume(parser, VSS_TOKEN_FINISH, "Expected 'finish' to end 'choose' block.");
        return vss_stmt_new_choose(expr, cases, count, otherwise_branch, op.line, op.column);
    }
    
    // Choices: choices
    if (match(parser, VSS_TOKEN_CHOICES)) {
        VSS_Token op = parser->previous;
        consume(parser, VSS_TOKEN_IDENTIFIER, "Expected enum name after 'choices'.");
        char *name = parse_string_value(parser->previous.start, parser->previous.length);
        
        while (match(parser, VSS_TOKEN_NEWLINE));
        
        char **members = NULL;
        size_t count = 0;
        
        while (!check(parser, VSS_TOKEN_FINISH) && !check(parser, VSS_TOKEN_EOF)) {
            consume(parser, VSS_TOKEN_IDENTIFIER, "Expected enum member name.");
            char *member = parse_string_value(parser->previous.start, parser->previous.length);
            members = realloc(members, sizeof(char*) * (count + 1));
            members[count++] = member;
            
            while (match(parser, VSS_TOKEN_NEWLINE));
        }
        consume(parser, VSS_TOKEN_FINISH, "Expected 'finish' to end enum declaration.");
        return vss_stmt_new_choices(name, members, count, op.line, op.column);
    }

    // Interface: interface
    if (match(parser, VSS_TOKEN_INTERFACE)) {
        VSS_Token op = parser->previous;
        consume(parser, VSS_TOKEN_IDENTIFIER, "Expected interface name.");
        char *name = parse_string_value(parser->previous.start, parser->previous.length);
        while (match(parser, VSS_TOKEN_NEWLINE));
        VSS_Stmt **task_decls = NULL;
        size_t count = 0;
        while (match(parser, VSS_TOKEN_TASK)) {
            VSS_Token task_op = parser->previous;
            consume(parser, VSS_TOKEN_IDENTIFIER, "Expected task name in interface.");
            char *task_name = parse_string_value(parser->previous.start, parser->previous.length);
            char **params = NULL;
            size_t param_count = 0;
            if (match(parser, VSS_TOKEN_NEEDS)) {
                while (match(parser, VSS_TOKEN_IDENTIFIER)) {
                    char *p = parse_string_value(parser->previous.start, parser->previous.length);
                    params = realloc(params, sizeof(char*) * (param_count + 1));
                    params[param_count++] = p;
                }
            }
            VSS_Block empty_body = {NULL, 0};
            VSS_Stmt *task_decl = vss_stmt_new_task(task_name, params, param_count, empty_body, task_op.line, task_op.column);
            free(task_name);
            task_decls = realloc(task_decls, sizeof(VSS_Stmt*) * (count + 1));
            task_decls[count++] = task_decl;
            while (match(parser, VSS_TOKEN_NEWLINE));
        }
        consume(parser, VSS_TOKEN_FINISH, "Expected 'finish' to end interface declaration.");
        return vss_stmt_new_interface(name, task_decls, count, op.line, op.column);
    }

    if (check(parser, VSS_TOKEN_IDENTIFIER)) {
        VSS_Lexer temp = *parser->lexer;
        VSS_Token next = vss_lexer_next(&temp);
        while (next.type == VSS_TOKEN_NEWLINE) {
            next = vss_lexer_next(&temp);
        }
        if (next.type == VSS_TOKEN_TASK) {
            advance(parser);
            char *name = parse_string_value(parser->previous.start, parser->previous.length);
            while (match(parser, VSS_TOKEN_NEWLINE));
            VSS_Stmt **task_decls = NULL;
            size_t count = 0;
            while (match(parser, VSS_TOKEN_TASK)) {
                VSS_Token task_op = parser->previous;
                consume(parser, VSS_TOKEN_IDENTIFIER, "Expected task name in interface.");
                char *task_name = parse_string_value(parser->previous.start, parser->previous.length);
                char **params = NULL;
                size_t param_count = 0;
                if (match(parser, VSS_TOKEN_NEEDS)) {
                    while (match(parser, VSS_TOKEN_IDENTIFIER)) {
                        char *p = parse_string_value(parser->previous.start, parser->previous.length);
                        params = realloc(params, sizeof(char*) * (param_count + 1));
                        params[param_count++] = p;
                    }
                }
                VSS_Block empty_body = {NULL, 0};
                VSS_Stmt *task_decl = vss_stmt_new_task(task_name, params, param_count, empty_body, task_op.line, task_op.column);
                free(task_name);
                task_decls = realloc(task_decls, sizeof(VSS_Stmt*) * (count + 1));
                task_decls[count++] = task_decl;
                while (match(parser, VSS_TOKEN_NEWLINE));
            }
            consume(parser, VSS_TOKEN_FINISH, "Expected 'finish' to end interface declaration.");
            return vss_stmt_new_interface(name, task_decls, count, parser->previous.line, parser->previous.column);
        }
    }

    // Object: object
    if (match(parser, VSS_TOKEN_OBJECT)) {
        VSS_Token op = parser->previous;
        consume(parser, VSS_TOKEN_IDENTIFIER, "Expected object name.");
        char *name = parse_string_value(parser->previous.start, parser->previous.length);
        
        char *parent_name = NULL;
        if (match(parser, VSS_TOKEN_EXTENDS)) {
            consume(parser, VSS_TOKEN_IDENTIFIER, "Expected parent object name after 'extends'.");
            parent_name = parse_string_value(parser->previous.start, parser->previous.length);
        }
        
        char **interfaces = NULL;
        size_t interface_count = 0;
        if (match(parser, VSS_TOKEN_IMPLEMENTS)) {
            do {
                consume(parser, VSS_TOKEN_IDENTIFIER, "Expected interface name after 'implements'.");
                char *iface = parse_string_value(parser->previous.start, parser->previous.length);
                interfaces = realloc(interfaces, sizeof(char*) * (interface_count + 1));
                interfaces[interface_count++] = iface;
            } while (match(parser, VSS_TOKEN_COMMA));
        }
        
        while (match(parser, VSS_TOKEN_NEWLINE));
        
        VSS_Stmt **members = NULL;
        size_t member_count = 0;
        
        while (!check(parser, VSS_TOKEN_FINISH) && !check(parser, VSS_TOKEN_EOF)) {
            VSS_Stmt *m = parse_statement(parser);
            if (m) {
                members = realloc(members, sizeof(VSS_Stmt*) * (member_count + 1));
                members[member_count++] = m;
            }
            while (match(parser, VSS_TOKEN_NEWLINE));
        }
        consume(parser, VSS_TOKEN_FINISH, "Expected 'finish' to end object declaration.");
        return vss_stmt_new_object(name, parent_name, interfaces, interface_count, members, member_count, op.line, op.column);
    }

    // Assignment or Expression Statement
    VSS_Expr *expr = parse_expression(parser);
    if (match_becomes_or_equal(parser)) {
        VSS_Expr *val = parse_expression(parser);
        if (expr->kind == VSS_EXPR_NAME) {
            char *name = safe_strdup(expr->as.name);
            int line = expr->line;
            int col = expr->column;
            vss_expr_free(expr);
            return vss_stmt_new_assign(name, val, line, col);
        } else if (expr->kind == VSS_EXPR_FIELD_ACCESS) {
            VSS_Expr *map = expr->as.field_access.map;
            VSS_Expr *field = expr->as.field_access.field;
            int line = expr->line;
            int col = expr->column;
            expr->as.field_access.map = NULL;
            expr->as.field_access.field = NULL;
            vss_expr_free(expr);
            return vss_stmt_new_set_field(map, field, val, line, col);
        } else {
            error_at(parser, &parser->previous, "Invalid assignment target.");
            vss_expr_free(expr);
            vss_expr_free(val);
            return NULL;
        }
    }
    
    return vss_stmt_new_expr(expr, expr->line, expr->column);
}

void vss_parser_init(VSS_Parser *parser, VSS_Lexer *lexer) {
    parser->lexer = lexer;
    parser->had_error = false;
    parser->panic_mode = false;
    advance(parser); // Populate current token
}

VSS_Block vss_parse_program(VSS_Parser *parser) {
    VSS_Stmt **statements = NULL;
    size_t count = 0;
    
    while (match(parser, VSS_TOKEN_NEWLINE));
    
    while (!check(parser, VSS_TOKEN_EOF)) {
        VSS_Stmt *stmt = parse_statement(parser);
        if (stmt) {
            statements = realloc(statements, sizeof(VSS_Stmt*) * (count + 1));
            statements[count++] = stmt;
        }
        
        if (check(parser, VSS_TOKEN_EOF)) break;
        
        // Statements must end with newlines
        if (!match(parser, VSS_TOKEN_NEWLINE)) {
            error_at(parser, &parser->current, "Expected newline after statement.");
            // Simple recovery: consume until newline or EOF
            while (!is_newline_or_eof(parser)) {
                advance(parser);
            }
            if (check(parser, VSS_TOKEN_NEWLINE)) advance(parser);
        }
        
        while (match(parser, VSS_TOKEN_NEWLINE));
        parser->panic_mode = false; // Reset panic mode at statement boundary
    }
    
    VSS_Block b;
    b.statements = statements;
    b.count = count;
    return b;
}
