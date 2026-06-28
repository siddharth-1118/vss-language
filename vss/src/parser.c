#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parser.h"

static void error_at(Parser *parser, Token *token, const char *message) {
    if (parser->panic_mode) return;
    parser->panic_mode = true;
    parser->had_error = true;

    fprintf(stderr, "error line %d, col %d: ", token->line, token->column);
    if (token->type == TOKEN_EOF) {
        fprintf(stderr, "at end of file: ");
    } else if (token->type == TOKEN_ERROR) {
        // Already an error token from lexer
    } else {
        fprintf(stderr, "at '%.*s': ", (int)token->length, token->start);
    }
    fprintf(stderr, "%s\n", message);
}

static void advance(Parser *parser) {
    parser->previous = parser->current;

    for (;;) {
        parser->current = lexer_next(parser->lexer);
        if (parser->current.type != TOKEN_ERROR) break;
        error_at(parser, &parser->current, parser->current.start);
    }
}

static bool check(Parser *parser, TokenType type) {
    return parser->current.type == type;
}

static bool match(Parser *parser, TokenType type) {
    if (!check(parser, type)) return false;
    advance(parser);
    return true;
}

static void consume(Parser *parser, TokenType type, const char *message) {
    if (parser->current.type == type) {
        advance(parser);
        return;
    }
    error_at(parser, &parser->current, message);
}

static bool is_newline_or_eof(Parser *parser) {
    return check(parser, TOKEN_NEWLINE) || check(parser, TOKEN_EOF);
}

static bool can_start_unary(TokenType type) {
    switch (type) {
        case TOKEN_NUMBER:
        case TOKEN_STRING:
        case TOKEN_YES:
        case TOKEN_NO:
        case TOKEN_EMPTY:
        case TOKEN_IDENTIFIER:
        case TOKEN_LEFT_BRACKET:
        case TOKEN_MAP:
        case TOKEN_NOT:
        case TOKEN_MINUS:
        case TOKEN_SIZE:
        case TOKEN_EXISTS:
        case TOKEN_READ:
            return true;
        default:
            return false;
    }
}

// Forward declarations of expression parsers
static Expr *parse_expression(Parser *parser);
static Expr *parse_or(Parser *parser);
static Expr *parse_and(Parser *parser);
static Expr *parse_equality(Parser *parser);
static Expr *parse_comparison(Parser *parser);
static Expr *parse_term(Parser *parser);
static Expr *parse_factor(Parser *parser);
static Expr *parse_unary(Parser *parser);
static Expr *parse_postfix(Parser *parser);
static Expr *parse_primary(Parser *parser);

static Stmt *parse_statement(Parser *parser);
static Block parse_block(Parser *parser);

static char *safe_strdup(const char *s) {
    if (!s) return NULL;
    char *dup = malloc(strlen(s) + 1);
    if (dup) {
        strcpy(dup, s);
    }
    return dup;
}

static Block parse_block(Parser *parser) {
    while (match(parser, TOKEN_NEWLINE));
    
    Stmt **statements = NULL;
    size_t count = 0;
    
    while (!check(parser, TOKEN_FINISH) && 
           !check(parser, TOKEN_ORWHEN) && 
           !check(parser, TOKEN_OTHERWISE) && 
           !check(parser, TOKEN_CASE) && 
           !check(parser, TOKEN_RESCUE) && 
           !check(parser, TOKEN_EOF)) {
        Stmt *stmt = parse_statement(parser);
        if (stmt) {
            statements = realloc(statements, sizeof(Stmt*) * (count + 1));
            statements[count++] = stmt;
        }
        
        if (check(parser, TOKEN_EOF)) break;
        
        if (!match(parser, TOKEN_NEWLINE)) {
            error_at(parser, &parser->current, "Expected newline after statement in block.");
            while (!is_newline_or_eof(parser)) {
                advance(parser);
            }
            if (check(parser, TOKEN_NEWLINE)) advance(parser);
        }
        while (match(parser, TOKEN_NEWLINE));
        parser->panic_mode = false;
    }
    
    Block b;
    b.statements = statements;
    b.count = count;
    return b;
}

static Expr *parse_expression(Parser *parser) {
    return parse_or(parser);
}

static Expr *parse_or(Parser *parser) {
    Expr *expr = parse_and(parser);
    while (match(parser, TOKEN_OR)) {
        Token op = parser->previous;
        Expr *right = parse_and(parser);
        expr = expr_new_binary(op.type, expr, right, op.line, op.column);
    }
    return expr;
}

static Expr *parse_and(Parser *parser) {
    Expr *expr = parse_equality(parser);
    while (match(parser, TOKEN_AND)) {
        Token op = parser->previous;
        Expr *right = parse_equality(parser);
        expr = expr_new_binary(op.type, expr, right, op.line, op.column);
    }
    return expr;
}

static Expr *parse_equality(Parser *parser) {
    Expr *expr = parse_comparison(parser);
    while (match(parser, TOKEN_SAME_AS) || match(parser, TOKEN_NOT_SAME_AS)) {
        Token op = parser->previous;
        Expr *right = parse_comparison(parser);
        expr = expr_new_binary(op.type, expr, right, op.line, op.column);
    }
    return expr;
}

static Expr *parse_comparison(Parser *parser) {
    Expr *expr = parse_term(parser);
    while (match(parser, TOKEN_ABOVE) || match(parser, TOKEN_BELOW) ||
           match(parser, TOKEN_AT_LEAST) || match(parser, TOKEN_AT_MOST)) {
        Token op = parser->previous;
        Expr *right = parse_term(parser);
        expr = expr_new_binary(op.type, expr, right, op.line, op.column);
    }
    return expr;
}

static Expr *parse_term(Parser *parser) {
    Expr *expr = parse_factor(parser);
    while (match(parser, TOKEN_PLUS) || match(parser, TOKEN_MINUS)) {
        Token op = parser->previous;
        Expr *right = parse_factor(parser);
        expr = expr_new_binary(op.type, expr, right, op.line, op.column);
    }
    return expr;
}

static Expr *parse_factor(Parser *parser) {
    Expr *expr = parse_unary(parser);
    while (match(parser, TOKEN_STAR) || match(parser, TOKEN_SLASH) || match(parser, TOKEN_PERCENT)) {
        Token op = parser->previous;
        Expr *right = parse_unary(parser);
        expr = expr_new_binary(op.type, expr, right, op.line, op.column);
    }
    return expr;
}

static Expr *parse_unary(Parser *parser) {
    if (match(parser, TOKEN_NOT) || match(parser, TOKEN_MINUS)) {
        Token op = parser->previous;
        Expr *operand = parse_unary(parser);
        return expr_new_unary(op.type, operand, op.line, op.column);
    }
    return parse_postfix(parser);
}

static Expr *parse_postfix(Parser *parser) {
    Expr *expr = parse_primary(parser);
    for (;;) {
        if (match(parser, TOKEN_ITEM)) {
            Token op = parser->previous;
            Expr *index = parse_unary(parser);
            expr = expr_new_item_access(expr, index, op.line, op.column);
        } else if (match(parser, TOKEN_FIELD)) {
            Token op = parser->previous;
            Expr *field = parse_unary(parser);
            expr = expr_new_field_access(expr, field, op.line, op.column);
        } else if (match(parser, TOKEN_WITH)) {
            Token op = parser->previous;
            Expr **args = NULL;
            size_t count = 0;
            // Parse arguments as space-separated unary expressions
            while (can_start_unary(parser->current.type) && !is_newline_or_eof(parser)) {
                Expr *arg = parse_unary(parser);
                args = realloc(args, sizeof(Expr*) * (count + 1));
                args[count++] = arg;
            }
            expr = expr_new_call(expr, args, count, op.line, op.column);
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

static Expr *parse_primary(Parser *parser) {
    if (match(parser, TOKEN_NUMBER)) {
        double val = strtod(parser->previous.start, NULL);
        return expr_new_number(val, parser->previous.line, parser->previous.column);
    }
    if (match(parser, TOKEN_STRING)) {
        char *str = parse_string_value(parser->previous.start, parser->previous.length);
        Expr *expr = expr_new_string(str, parser->previous.line, parser->previous.column);
        free(str);
        return expr;
    }
    if (match(parser, TOKEN_YES)) {
        return expr_new_bool(true, parser->previous.line, parser->previous.column);
    }
    if (match(parser, TOKEN_NO)) {
        return expr_new_bool(false, parser->previous.line, parser->previous.column);
    }
    if (match(parser, TOKEN_EMPTY)) {
        return expr_new_empty(parser->previous.line, parser->previous.column);
    }
    if (match(parser, TOKEN_IDENTIFIER)) {
        char *name = malloc(parser->previous.length + 1);
        memcpy(name, parser->previous.start, parser->previous.length);
        name[parser->previous.length] = '\0';
        Expr *expr = expr_new_name(name, parser->previous.line, parser->previous.column);
        free(name);
        return expr;
    }
    
    // Built-ins desugared directly
    if (match(parser, TOKEN_SIZE)) {
        Token op = parser->previous;
        consume(parser, TOKEN_OF, "Expected 'of' after 'size'.");
        Expr *collection = parse_unary(parser);
        Expr **args = malloc(sizeof(Expr*));
        args[0] = collection;
        return expr_new_call(expr_new_name("__size", op.line, op.column), args, 1, op.line, op.column);
    }
    if (match(parser, TOKEN_EXISTS)) {
        Token op = parser->previous;
        Expr *path = parse_unary(parser);
        Expr **args = malloc(sizeof(Expr*));
        args[0] = path;
        return expr_new_call(expr_new_name("__exists", op.line, op.column), args, 1, op.line, op.column);
    }
    if (match(parser, TOKEN_READ)) {
        Token op = parser->previous;
        Expr *path = parse_unary(parser);
        Expr **args = malloc(sizeof(Expr*));
        args[0] = path;
        return expr_new_call(expr_new_name("__read", op.line, op.column), args, 1, op.line, op.column);
    }
    
    // List Literal
    if (match(parser, TOKEN_LEFT_BRACKET)) {
        Token op = parser->previous;
        Expr **elements = NULL;
        size_t count = 0;
        
        while (match(parser, TOKEN_NEWLINE)); // Skip newlines inside bracket
        
        if (!check(parser, TOKEN_RIGHT_BRACKET)) {
            do {
                while (match(parser, TOKEN_NEWLINE));
                Expr *elem = parse_expression(parser);
                elements = realloc(elements, sizeof(Expr*) * (count + 1));
                elements[count++] = elem;
                while (match(parser, TOKEN_NEWLINE));
            } while (match(parser, TOKEN_COMMA));
        }
        
        while (match(parser, TOKEN_NEWLINE));
        consume(parser, TOKEN_RIGHT_BRACKET, "Expected ']' at the end of list literal.");
        return expr_new_list(elements, count, op.line, op.column);
    }
    
    // Map Literal
    if (match(parser, TOKEN_MAP)) {
        Token op = parser->previous;
        consume(parser, TOKEN_LEFT_BRACKET, "Expected '[' after 'map'.");
        
        char **keys = NULL;
        Expr **values = NULL;
        size_t count = 0;
        
        while (match(parser, TOKEN_NEWLINE));
        
        while (!check(parser, TOKEN_RIGHT_BRACKET) && !check(parser, TOKEN_EOF)) {
            consume(parser, TOKEN_STRING, "Expected string key in map literal.");
            char *key = parse_string_value(parser->previous.start, parser->previous.length);
            
            consume(parser, TOKEN_COLON, "Expected ':' after key in map entry.");
            Expr *val = parse_expression(parser);
            
            keys = realloc(keys, sizeof(char*) * (count + 1));
            values = realloc(values, sizeof(Expr*) * (count + 1));
            keys[count] = key;
            values[count] = val;
            count++;
            
            while (match(parser, TOKEN_NEWLINE));
        }
        
        consume(parser, TOKEN_RIGHT_BRACKET, "Expected ']' at the end of map literal.");
        return expr_new_map(keys, values, count, op.line, op.column);
    }
    
    error_at(parser, &parser->current, "Expected expression.");
    return NULL;
}

static Stmt *parse_statement(Parser *parser) {
    // Variable definition: make
    if (match(parser, TOKEN_MAKE)) {
        Token op = parser->previous;
        consume(parser, TOKEN_IDENTIFIER, "Expected variable name after 'make'.");
        char *name = parse_string_value(parser->previous.start, parser->previous.length);
        consume(parser, TOKEN_BECOMES, "Expected 'becomes' after variable name.");
        Expr *init = parse_expression(parser);
        Stmt *stmt = stmt_new_make(name, init, op.line, op.column);
        free(name);
        return stmt;
    }
    
    // Constant definition: keep
    if (match(parser, TOKEN_KEEP)) {
        Token op = parser->previous;
        consume(parser, TOKEN_IDENTIFIER, "Expected constant name after 'keep'.");
        char *name = parse_string_value(parser->previous.start, parser->previous.length);
        consume(parser, TOKEN_BECOMES, "Expected 'becomes' after constant name.");
        Expr *init = parse_expression(parser);
        Stmt *stmt = stmt_new_keep(name, init, op.line, op.column);
        free(name);
        return stmt;
    }
    
    // Output: say
    if (match(parser, TOKEN_SAY)) {
        Token op = parser->previous;
        Expr *expr = parse_expression(parser);
        return stmt_new_say(expr, op.line, op.column);
    }
    
    // Return: send
    if (match(parser, TOKEN_SEND)) {
        Token op = parser->previous;
        Expr *expr = parse_expression(parser);
        return stmt_new_send(expr, op.line, op.column);
    }
    
    // Loop control: leave
    if (match(parser, TOKEN_LEAVE)) {
        return stmt_new_leave(parser->previous.line, parser->previous.column);
    }
    
    // Loop control: skip
    if (match(parser, TOKEN_SKIP)) {
        return stmt_new_skip(parser->previous.line, parser->previous.column);
    }
    
    // Import: grab
    if (match(parser, TOKEN_GRAB)) {
        Token op = parser->previous;
        consume(parser, TOKEN_IDENTIFIER, "Expected module name after 'grab'.");
        char *name = parse_string_value(parser->previous.start, parser->previous.length);
        Stmt *stmt = stmt_new_grab(name, op.line, op.column);
        free(name);
        return stmt;
    }
    
    // File built-ins desugared into call statements
    if (match(parser, TOKEN_WRITE)) {
        Token op = parser->previous;
        Expr *content = parse_expression(parser);
        consume(parser, TOKEN_INTO, "Expected 'into' after content in 'write' statement.");
        Expr *path = parse_expression(parser);
        Expr **args = malloc(sizeof(Expr*) * 2);
        args[0] = content;
        args[1] = path;
        return stmt_new_expr(expr_new_call(expr_new_name("__write", op.line, op.column), args, 2, op.line, op.column), op.line, op.column);
    }
    if (match(parser, TOKEN_ADD)) {
        Token op = parser->previous;
        Expr *content = parse_expression(parser);
        consume(parser, TOKEN_INTO, "Expected 'into' after content in 'add' statement.");
        Expr *path = parse_expression(parser);
        Expr **args = malloc(sizeof(Expr*) * 2);
        args[0] = content;
        args[1] = path;
        return stmt_new_expr(expr_new_call(expr_new_name("__add", op.line, op.column), args, 2, op.line, op.column), op.line, op.column);
    }
    if (match(parser, TOKEN_ERASE)) {
        Token op = parser->previous;
        Expr *path = parse_expression(parser);
        Expr **args = malloc(sizeof(Expr*));
        args[0] = path;
        return stmt_new_expr(expr_new_call(expr_new_name("__erase", op.line, op.column), args, 1, op.line, op.column), op.line, op.column);
    }
    
    // List collection update: put
    if (match(parser, TOKEN_PUT)) {
        Token op = parser->previous;
        Expr *val = parse_expression(parser);
        consume(parser, TOKEN_INTO, "Expected 'into' after value in 'put' statement.");
        Expr *list = parse_expression(parser);
        return stmt_new_put(val, list, op.line, op.column);
    }
    
    // Map collection update: set
    if (match(parser, TOKEN_SET)) {
        Token op = parser->previous;
        Expr *target = parse_expression(parser);
        if (target->kind != EXPR_FIELD_ACCESS) {
            error_at(parser, &parser->previous, "Expected map field access after 'set'.");
        }
        consume(parser, TOKEN_BECOMES, "Expected 'becomes' after map field access in 'set' statement.");
        Expr *val = parse_expression(parser);
        Expr *map = target->as.field_access.map;
        Expr *field = target->as.field_access.field;
        free(target);
        return stmt_new_set_field(map, field, val, op.line, op.column);
    }
    
    if (match(parser, TOKEN_HI)) {
        Token op = parser->previous;
        consume(parser, TOKEN_HTMVSS, "Expected 'htmvss' after 'hi'.");
        return stmt_new_hi_htmvss(op.line, op.column);
    }
    
    if (match(parser, TOKEN_BYE)) {
        Token op = parser->previous;
        consume(parser, TOKEN_HTMVSS, "Expected 'htmvss' after 'bye'.");
        return stmt_new_bye_htmvss(op.line, op.column);
    }
    
    // Conditional: when
    if (match(parser, TOKEN_WHEN)) {
        Token op = parser->previous;
        WhenBranch *branches = NULL;
        size_t count = 0;
        
        Expr *cond = parse_expression(parser);
        Block block = parse_block(parser);
        
        branches = realloc(branches, sizeof(WhenBranch) * (count + 1));
        branches[count].condition = cond;
        branches[count].block = block;
        count++;
        
        while (match(parser, TOKEN_ORWHEN)) {
            Expr *ocond = parse_expression(parser);
            Block oblock = parse_block(parser);
            branches = realloc(branches, sizeof(WhenBranch) * (count + 1));
            branches[count].condition = ocond;
            branches[count].block = oblock;
            count++;
        }
        
        Block otherwise_branch = {NULL, 0};
        if (match(parser, TOKEN_OTHERWISE)) {
            otherwise_branch = parse_block(parser);
        }
        
        consume(parser, TOKEN_FINISH, "Expected 'finish' to end 'when' block.");
        return stmt_new_when(branches, count, otherwise_branch, op.line, op.column);
    }
    
    // Loops: repeat
    if (match(parser, TOKEN_REPEAT)) {
        Token op = parser->previous;
        
        if (match(parser, TOKEN_EACH)) {
            // each loop
            consume(parser, TOKEN_IDENTIFIER, "Expected variable name after 'each'.");
            char *var_name = parse_string_value(parser->previous.start, parser->previous.length);
            consume(parser, TOKEN_IN, "Expected 'in' after variable name in 'each' loop.");
            Expr *collection = parse_expression(parser);
            Block body = parse_block(parser);
            consume(parser, TOKEN_FINISH, "Expected 'finish' to end 'repeat each' loop.");
            Stmt *stmt = stmt_new_repeat_each(var_name, collection, body, op.line, op.column);
            free(var_name);
            return stmt;
        } else {
            // Check if range loop or count loop
            // Since we need to look ahead without consuming (unless it's an identifier),
            // if current is IDENTIFIER and token after it is THROUGH:
            // But we don't have standard lookahead for two tokens. Wait!
            // We can just parse the next expression. If that expression is a name (EXPR_NAME) and the current token is TOKEN_THROUGH:
            // This is super clever! We parse the expression. If it is EXPR_NAME, and we match TOKEN_THROUGH, we know it's a range loop!
            // If it's anything else, it's a count loop!
            // Wait, does that work?
            // E.g., `repeat i through 1 to 5` -> first we parse `i` as expression. It returns `EXPR_NAME` for `i`.
            // Then we see `through`! We match `through`. This is perfect!
            // What if it is `repeat 5 times`? First we parse `5` as expression. It returns `EXPR_NUMBER` for `5`.
            // The next token is `times`. We do NOT match `through`. So we fall back to count loop!
            // This is absolutely brilliant and doesn't require looking ahead in the token stream at all!
            Expr *first_expr = parse_expression(parser);
            if (first_expr->kind == EXPR_NAME && match(parser, TOKEN_THROUGH)) {
                char *var_name = safe_strdup(first_expr->as.name);
                expr_free(first_expr);
                
                Expr *start = parse_expression(parser);
                consume(parser, TOKEN_TO, "Expected 'to' in range loop.");
                Expr *end = parse_expression(parser);
                Block body = parse_block(parser);
                consume(parser, TOKEN_FINISH, "Expected 'finish' to end range loop.");
                Stmt *stmt = stmt_new_repeat_range(var_name, start, end, body, op.line, op.column);
                free(var_name);
                return stmt;
            } else {
                consume(parser, TOKEN_TIMES, "Expected 'times' after count expression in repeat loop.");
                Block body = parse_block(parser);
                consume(parser, TOKEN_FINISH, "Expected 'finish' to end repeat loop.");
                return stmt_new_repeat_count(first_expr, body, op.line, op.column);
            }
        }
    }
    
    // Loops: during
    if (match(parser, TOKEN_DURING)) {
        Token op = parser->previous;
        Expr *cond = parse_expression(parser);
        Block body = parse_block(parser);
        consume(parser, TOKEN_FINISH, "Expected 'finish' to end 'during' loop.");
        return stmt_new_during(cond, body, op.line, op.column);
    }
    
    // Task Definition: task
    if (match(parser, TOKEN_TASK)) {
        Token op = parser->previous;
        consume(parser, TOKEN_IDENTIFIER, "Expected task name after 'task'.");
        char *name = parse_string_value(parser->previous.start, parser->previous.length);
        
        char **params = NULL;
        size_t count = 0;
        if (match(parser, TOKEN_NEEDS)) {
            while (match(parser, TOKEN_IDENTIFIER)) {
                char *p = parse_string_value(parser->previous.start, parser->previous.length);
                params = realloc(params, sizeof(char*) * (count + 1));
                params[count++] = p;
            }
        }
        
        Block body = parse_block(parser);
        consume(parser, TOKEN_FINISH, "Expected 'finish' to end task definition.");
        Stmt *stmt = stmt_new_task(name, params, count, body, op.line, op.column);
        free(name);
        return stmt;
    }
    
    // Error Handling: attempt
    if (match(parser, TOKEN_ATTEMPT)) {
        Token op = parser->previous;
        Block try_body = parse_block(parser);
        consume(parser, TOKEN_RESCUE, "Expected 'rescue' after 'attempt' block.");
        consume(parser, TOKEN_IDENTIFIER, "Expected problem variable name after 'rescue'.");
        char *prob_var = parse_string_value(parser->previous.start, parser->previous.length);
        Block rescue_body = parse_block(parser);
        consume(parser, TOKEN_FINISH, "Expected 'finish' to end 'attempt' block.");
        Stmt *stmt = stmt_new_attempt(try_body, prob_var, rescue_body, op.line, op.column);
        free(prob_var);
        return stmt;
    }
    
    // Choose: choose
    if (match(parser, TOKEN_CHOOSE)) {
        Token op = parser->previous;
        Expr *expr = parse_expression(parser);
        
        while (match(parser, TOKEN_NEWLINE));
        
        ChooseCase *cases = NULL;
        size_t count = 0;
        
        while (match(parser, TOKEN_CASE)) {
            Expr *case_expr = parse_expression(parser);
            Block case_block = parse_block(parser);
            cases = realloc(cases, sizeof(ChooseCase) * (count + 1));
            cases[count].expr = case_expr;
            cases[count].block = case_block;
            count++;
            while (match(parser, TOKEN_NEWLINE));
        }
        
        Block otherwise_branch = {NULL, 0};
        if (match(parser, TOKEN_OTHERWISE)) {
            otherwise_branch = parse_block(parser);
        }
        
        consume(parser, TOKEN_FINISH, "Expected 'finish' to end 'choose' block.");
        return stmt_new_choose(expr, cases, count, otherwise_branch, op.line, op.column);
    }
    
    // Assignment or Expression Statement
    // If it starts with an identifier, check if the next is BECOMES
    if (check(parser, TOKEN_IDENTIFIER)) {
        // Let's inspect the next token. But we don't have lookahead unless we change the lexer.
        // Wait, can we peek at the token after current?
        // Wait, the lexer has current and start pointers. We can peek at the next token by temporarily
        // copying the lexer state and calls lexer_next.
        // Let's do that! That is very simple and preserves lexer isolation.
        Lexer temp = *parser->lexer;
        Token next = lexer_next(&temp);
        // Note: next might skip spaces depending on lexer_next. Yes, lexer_next skips spaces and returns the next actual token.
        if (next.type == TOKEN_BECOMES) {
            advance(parser); // Consume the identifier
            char *name = parse_string_value(parser->previous.start, parser->previous.length);
            consume(parser, TOKEN_BECOMES, "Expected 'becomes' after identifier.");
            Expr *val = parse_expression(parser);
            Stmt *stmt = stmt_new_assign(name, val, parser->previous.line, parser->previous.column);
            free(name);
            return stmt;
        }
    }
    
    // Expression statement
    Expr *expr = parse_expression(parser);
    return stmt_new_expr(expr, expr->line, expr->column);
}

void parser_init(Parser *parser, Lexer *lexer) {
    parser->lexer = lexer;
    parser->had_error = false;
    parser->panic_mode = false;
    advance(parser); // Populate current token
}

Block parse_program(Parser *parser) {
    Stmt **statements = NULL;
    size_t count = 0;
    
    while (match(parser, TOKEN_NEWLINE));
    
    while (!check(parser, TOKEN_EOF)) {
        Stmt *stmt = parse_statement(parser);
        if (stmt) {
            statements = realloc(statements, sizeof(Stmt*) * (count + 1));
            statements[count++] = stmt;
        }
        
        if (check(parser, TOKEN_EOF)) break;
        
        // Statements must end with newlines
        if (!match(parser, TOKEN_NEWLINE)) {
            error_at(parser, &parser->current, "Expected newline after statement.");
            // Simple recovery: consume until newline or EOF
            while (!is_newline_or_eof(parser)) {
                advance(parser);
            }
            if (check(parser, TOKEN_NEWLINE)) advance(parser);
        }
        
        while (match(parser, TOKEN_NEWLINE));
        parser->panic_mode = false; // Reset panic mode at statement boundary
    }
    
    Block b;
    b.statements = statements;
    b.count = count;
    return b;
}
