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
            return true;
        default:
            return false;
    }
}

// Forward declarations of expression parsers
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
    if (match(parser, VSS_TOKEN_NUMBER)) {
        double val = strtod(parser->previous.start, NULL);
        return vss_expr_new_number(val, parser->previous.line, parser->previous.column);
    }
    if (match(parser, VSS_TOKEN_STRING)) {
        char *str = parse_string_value(parser->previous.start, parser->previous.length);
        VSS_Expr *expr = vss_expr_new_string(str, parser->previous.line, parser->previous.column);
        free(str);
        return expr;
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
        consume(parser, VSS_TOKEN_BECOMES, "Expected 'becomes' after variable name.");
        VSS_Expr *init = parse_expression(parser);
        VSS_Stmt *stmt = vss_stmt_new_make(name, init, op.line, op.column);
        free(name);
        return stmt;
    }
    
    // Constant definition: keep
    if (match(parser, VSS_TOKEN_KEEP)) {
        VSS_Token op = parser->previous;
        consume(parser, VSS_TOKEN_IDENTIFIER, "Expected constant name after 'keep'.");
        char *name = parse_string_value(parser->previous.start, parser->previous.length);
        consume(parser, VSS_TOKEN_BECOMES, "Expected 'becomes' after constant name.");
        VSS_Expr *init = parse_expression(parser);
        VSS_Stmt *stmt = vss_stmt_new_keep(name, init, op.line, op.column);
        free(name);
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
        consume(parser, VSS_TOKEN_BECOMES, "Expected 'becomes' after map field access in 'set' statement.");
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
    
    // Assignment or Expression Statement
    // If it starts with an identifier, check if the next is BECOMES
    if (check(parser, VSS_TOKEN_IDENTIFIER)) {
        // Let's inspect the next token. But we don't have lookahead unless we change the lexer.
        // Wait, can we peek at the token after current?
        // Wait, the lexer has current and start pointers. We can peek at the next token by temporarily
        // copying the lexer state and calls vss_lexer_next.
        // Let's do that! That is very simple and preserves lexer isolation.
        VSS_Lexer temp = *parser->lexer;
        VSS_Token next = vss_lexer_next(&temp);
        // Note: next might skip spaces depending on vss_lexer_next. Yes, vss_lexer_next skips spaces and returns the next actual token.
        if (next.type == VSS_TOKEN_BECOMES) {
            advance(parser); // Consume the identifier
            char *name = parse_string_value(parser->previous.start, parser->previous.length);
            consume(parser, VSS_TOKEN_BECOMES, "Expected 'becomes' after identifier.");
            VSS_Expr *val = parse_expression(parser);
            VSS_Stmt *stmt = vss_stmt_new_assign(name, val, parser->previous.line, parser->previous.column);
            free(name);
            return stmt;
        }
    }
    
    // Expression statement
    VSS_Expr *expr = parse_expression(parser);
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
