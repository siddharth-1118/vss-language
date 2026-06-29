#ifndef VSS_AST_H
#define VSS_AST_H

#include "common.h"
#include "token.h"

typedef enum {
    VSS_EXPR_NUMBER,
    VSS_EXPR_STRING,
    VSS_EXPR_BOOL,
    VSS_EXPR_EMPTY,
    VSS_EXPR_NAME,
    VSS_EXPR_BINARY,
    VSS_EXPR_UNARY,
    VSS_EXPR_LIST,
    VSS_EXPR_MAP,
    VSS_EXPR_ITEM_ACCESS,
    VSS_EXPR_FIELD_ACCESS,
    VSS_EXPR_CALL
} VSS_ExprKind;

typedef struct VSS_Expr VSS_Expr;

struct VSS_Expr {
    VSS_ExprKind kind;
    int line;
    int column;
    union {
        double number;
        char *string;
        bool boolean;
        char *name;
        struct {
            VSS_TokenType op;
            VSS_Expr *left;
            VSS_Expr *right;
        } binary;
        struct {
            VSS_TokenType op;
            VSS_Expr *operand;
        } unary;
        struct {
            VSS_Expr **elements;
            size_t count;
        } list;
        struct {
            char **keys;
            VSS_Expr **values;
            size_t count;
        } map;
        struct {
            VSS_Expr *list;
            VSS_Expr *index;
        } item_access;
        struct {
            VSS_Expr *map;
            VSS_Expr *field;
        } field_access;
        struct {
            VSS_Expr *callee;
            VSS_Expr **args;
            size_t count;
        } call;
    } as;
};

typedef enum {
    VSS_STMT_MAKE,
    VSS_STMT_KEEP,
    VSS_STMT_ASSIGN,
    VSS_STMT_SAY,
    VSS_STMT_WHEN,
    VSS_STMT_REPEAT_COUNT,
    VSS_STMT_REPEAT_RANGE,
    VSS_STMT_REPEAT_EACH,
    VSS_STMT_DURING,
    VSS_STMT_LEAVE,
    VSS_STMT_SKIP,
    VSS_STMT_TASK,
    VSS_STMT_SEND,
    VSS_STMT_GRAB,
    VSS_STMT_ATTEMPT,
    VSS_STMT_CHOOSE,
    VSS_STMT_PUT,
    VSS_STMT_SET_FIELD,
    VSS_STMT_HI_HTMVSS,
    VSS_STMT_BYE_HTMVSS,
    VSS_STMT_EXPR
} VSS_StmtKind;

typedef struct VSS_Stmt VSS_Stmt;

typedef struct {
    VSS_Stmt **statements;
    size_t count;
} VSS_Block;

typedef struct {
    VSS_Expr *condition;
    VSS_Block block;
} VSS_WhenBranch;

typedef struct {
    VSS_Expr *expr;
    VSS_Block block;
} VSS_ChooseCase;

struct VSS_Stmt {
    VSS_StmtKind kind;
    int line;
    int column;
    union {
        struct {
            char *name;
            VSS_Expr *initializer;
        } make;
        struct {
            char *name;
            VSS_Expr *initializer;
        } keep;
        struct {
            char *name;
            VSS_Expr *value;
        } assign;
        struct {
            VSS_Expr *expression;
        } say;
        struct {
            VSS_WhenBranch *branches;
            size_t branch_count;
            VSS_Block otherwise_branch;
        } when;
        struct {
            VSS_Expr *count_expr;
            VSS_Block body;
        } repeat_count;
        struct {
            char *var_name;
            VSS_Expr *start;
            VSS_Expr *end;
            VSS_Block body;
        } repeat_range;
        struct {
            char *var_name;
            VSS_Expr *collection;
            VSS_Block body;
        } repeat_each;
        struct {
            VSS_Expr *condition;
            VSS_Block body;
        } during;
        struct {
            char *name;
            char **params;
            size_t param_count;
            VSS_Block body;
        } task;
        struct {
            VSS_Expr *expression;
        } send;
        struct {
            char *module_name;
        } grab;
        struct {
            VSS_Block try_body;
            char *problem_var;
            VSS_Block rescue_body;
        } attempt;
        struct {
            VSS_Expr *expr;
            VSS_ChooseCase *cases;
            size_t case_count;
            VSS_Block otherwise_branch;
        } choose;
        struct {
            VSS_Expr *value;
            VSS_Expr *list;
        } put;
        struct {
            VSS_Expr *map;
            VSS_Expr *field;
            VSS_Expr *value;
        } set_field;
        struct {
            VSS_Expr *expression;
        } expr_stmt;
    } as;
};

// Constructor helpers for Expressions
VSS_Expr *vss_expr_new_number(double value, int line, int column);
VSS_Expr *vss_expr_new_string(const char *value, int line, int column);
VSS_Expr *vss_expr_new_bool(bool value, int line, int column);
VSS_Expr *vss_expr_new_empty(int line, int column);
VSS_Expr *vss_expr_new_name(const char *name, int line, int column);
VSS_Expr *vss_expr_new_binary(VSS_TokenType op, VSS_Expr *left, VSS_Expr *right, int line, int column);
VSS_Expr *vss_expr_new_unary(VSS_TokenType op, VSS_Expr *operand, int line, int column);
VSS_Expr *vss_expr_new_list(VSS_Expr **elements, size_t count, int line, int column);
VSS_Expr *vss_expr_new_map(char **keys, VSS_Expr **values, size_t count, int line, int column);
VSS_Expr *vss_expr_new_item_access(VSS_Expr *list, VSS_Expr *index, int line, int column);
VSS_Expr *vss_expr_new_field_access(VSS_Expr *map, VSS_Expr *field, int line, int column);
VSS_Expr *vss_expr_new_call(VSS_Expr *callee, VSS_Expr **args, size_t count, int line, int column);

void vss_expr_free(VSS_Expr *expr);

// Constructor helpers for Statements
VSS_Stmt *vss_stmt_new_make(const char *name, VSS_Expr *initializer, int line, int column);
VSS_Stmt *vss_stmt_new_keep(const char *name, VSS_Expr *initializer, int line, int column);
VSS_Stmt *vss_stmt_new_assign(const char *name, VSS_Expr *value, int line, int column);
VSS_Stmt *vss_stmt_new_say(VSS_Expr *expression, int line, int column);
VSS_Stmt *vss_stmt_new_when(VSS_WhenBranch *branches, size_t branch_count, VSS_Block otherwise_branch, int line, int column);
VSS_Stmt *vss_stmt_new_repeat_count(VSS_Expr *count_expr, VSS_Block body, int line, int column);
VSS_Stmt *vss_stmt_new_repeat_range(const char *var_name, VSS_Expr *start, VSS_Expr *end, VSS_Block body, int line, int column);
VSS_Stmt *vss_stmt_new_repeat_each(const char *var_name, VSS_Expr *collection, VSS_Block body, int line, int column);
VSS_Stmt *vss_stmt_new_during(VSS_Expr *condition, VSS_Block body, int line, int column);
VSS_Stmt *vss_stmt_new_leave(int line, int column);
VSS_Stmt *vss_stmt_new_skip(int line, int column);
VSS_Stmt *vss_stmt_new_task(const char *name, char **params, size_t param_count, VSS_Block body, int line, int column);
VSS_Stmt *vss_stmt_new_send(VSS_Expr *expression, int line, int column);
VSS_Stmt *vss_stmt_new_grab(const char *module_name, int line, int column);
VSS_Stmt *vss_stmt_new_attempt(VSS_Block try_body, const char *problem_var, VSS_Block rescue_body, int line, int column);
VSS_Stmt *vss_stmt_new_choose(VSS_Expr *expr, VSS_ChooseCase *cases, size_t case_count, VSS_Block otherwise_branch, int line, int column);
VSS_Stmt *vss_stmt_new_put(VSS_Expr *value, VSS_Expr *list, int line, int column);
VSS_Stmt *vss_stmt_new_set_field(VSS_Expr *map, VSS_Expr *field, VSS_Expr *value, int line, int column);
VSS_Stmt *vss_stmt_new_hi_htmvss(int line, int column);
VSS_Stmt *vss_stmt_new_bye_htmvss(int line, int column);
VSS_Stmt *vss_stmt_new_expr(VSS_Expr *expression, int line, int column);

void vss_stmt_free(VSS_Stmt *stmt);
void vss_block_free(VSS_Block block);

#endif
