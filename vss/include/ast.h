#ifndef VSS_AST_H
#define VSS_AST_H

#include "common.h"
#include "token.h"

typedef enum {
    EXPR_NUMBER,
    EXPR_STRING,
    EXPR_BOOL,
    EXPR_EMPTY,
    EXPR_NAME,
    EXPR_BINARY,
    EXPR_UNARY,
    EXPR_LIST,
    EXPR_MAP,
    EXPR_ITEM_ACCESS,
    EXPR_FIELD_ACCESS,
    EXPR_CALL
} ExprKind;

typedef struct Expr Expr;

struct Expr {
    ExprKind kind;
    int line;
    int column;
    union {
        double number;
        char *string;
        bool boolean;
        char *name;
        struct {
            TokenType op;
            Expr *left;
            Expr *right;
        } binary;
        struct {
            TokenType op;
            Expr *operand;
        } unary;
        struct {
            Expr **elements;
            size_t count;
        } list;
        struct {
            char **keys;
            Expr **values;
            size_t count;
        } map;
        struct {
            Expr *list;
            Expr *index;
        } item_access;
        struct {
            Expr *map;
            Expr *field;
        } field_access;
        struct {
            Expr *callee;
            Expr **args;
            size_t count;
        } call;
    } as;
};

typedef enum {
    STMT_MAKE,
    STMT_KEEP,
    STMT_ASSIGN,
    STMT_SAY,
    STMT_WHEN,
    STMT_REPEAT_COUNT,
    STMT_REPEAT_RANGE,
    STMT_REPEAT_EACH,
    STMT_DURING,
    STMT_LEAVE,
    STMT_SKIP,
    STMT_TASK,
    STMT_SEND,
    STMT_GRAB,
    STMT_ATTEMPT,
    STMT_CHOOSE,
    STMT_PUT,
    STMT_SET_FIELD,
    STMT_HI_HTMVSS,
    STMT_BYE_HTMVSS,
    STMT_EXPR
} StmtKind;

typedef struct Stmt Stmt;

typedef struct {
    Stmt **statements;
    size_t count;
} Block;

typedef struct {
    Expr *condition;
    Block block;
} WhenBranch;

typedef struct {
    Expr *expr;
    Block block;
} ChooseCase;

struct Stmt {
    StmtKind kind;
    int line;
    int column;
    union {
        struct {
            char *name;
            Expr *initializer;
        } make;
        struct {
            char *name;
            Expr *initializer;
        } keep;
        struct {
            char *name;
            Expr *value;
        } assign;
        struct {
            Expr *expression;
        } say;
        struct {
            WhenBranch *branches;
            size_t branch_count;
            Block otherwise_branch;
        } when;
        struct {
            Expr *count_expr;
            Block body;
        } repeat_count;
        struct {
            char *var_name;
            Expr *start;
            Expr *end;
            Block body;
        } repeat_range;
        struct {
            char *var_name;
            Expr *collection;
            Block body;
        } repeat_each;
        struct {
            Expr *condition;
            Block body;
        } during;
        struct {
            char *name;
            char **params;
            size_t param_count;
            Block body;
        } task;
        struct {
            Expr *expression;
        } send;
        struct {
            char *module_name;
        } grab;
        struct {
            Block try_body;
            char *problem_var;
            Block rescue_body;
        } attempt;
        struct {
            Expr *expr;
            ChooseCase *cases;
            size_t case_count;
            Block otherwise_branch;
        } choose;
        struct {
            Expr *value;
            Expr *list;
        } put;
        struct {
            Expr *map;
            Expr *field;
            Expr *value;
        } set_field;
        struct {
            Expr *expression;
        } expr_stmt;
    } as;
};

// Constructor helpers for Expressions
Expr *expr_new_number(double value, int line, int column);
Expr *expr_new_string(const char *value, int line, int column);
Expr *expr_new_bool(bool value, int line, int column);
Expr *expr_new_empty(int line, int column);
Expr *expr_new_name(const char *name, int line, int column);
Expr *expr_new_binary(TokenType op, Expr *left, Expr *right, int line, int column);
Expr *expr_new_unary(TokenType op, Expr *operand, int line, int column);
Expr *expr_new_list(Expr **elements, size_t count, int line, int column);
Expr *expr_new_map(char **keys, Expr **values, size_t count, int line, int column);
Expr *expr_new_item_access(Expr *list, Expr *index, int line, int column);
Expr *expr_new_field_access(Expr *map, Expr *field, int line, int column);
Expr *expr_new_call(Expr *callee, Expr **args, size_t count, int line, int column);

void expr_free(Expr *expr);

// Constructor helpers for Statements
Stmt *stmt_new_make(const char *name, Expr *initializer, int line, int column);
Stmt *stmt_new_keep(const char *name, Expr *initializer, int line, int column);
Stmt *stmt_new_assign(const char *name, Expr *value, int line, int column);
Stmt *stmt_new_say(Expr *expression, int line, int column);
Stmt *stmt_new_when(WhenBranch *branches, size_t branch_count, Block otherwise_branch, int line, int column);
Stmt *stmt_new_repeat_count(Expr *count_expr, Block body, int line, int column);
Stmt *stmt_new_repeat_range(const char *var_name, Expr *start, Expr *end, Block body, int line, int column);
Stmt *stmt_new_repeat_each(const char *var_name, Expr *collection, Block body, int line, int column);
Stmt *stmt_new_during(Expr *condition, Block body, int line, int column);
Stmt *stmt_new_leave(int line, int column);
Stmt *stmt_new_skip(int line, int column);
Stmt *stmt_new_task(const char *name, char **params, size_t param_count, Block body, int line, int column);
Stmt *stmt_new_send(Expr *expression, int line, int column);
Stmt *stmt_new_grab(const char *module_name, int line, int column);
Stmt *stmt_new_attempt(Block try_body, const char *problem_var, Block rescue_body, int line, int column);
Stmt *stmt_new_choose(Expr *expr, ChooseCase *cases, size_t case_count, Block otherwise_branch, int line, int column);
Stmt *stmt_new_put(Expr *value, Expr *list, int line, int column);
Stmt *stmt_new_set_field(Expr *map, Expr *field, Expr *value, int line, int column);
Stmt *stmt_new_hi_htmvss(int line, int column);
Stmt *stmt_new_bye_htmvss(int line, int column);
Stmt *stmt_new_expr(Expr *expression, int line, int column);

void stmt_free(Stmt *stmt);
void block_free(Block block);

#endif
