#include <stdlib.h>
#include <string.h>

#include "ast.h"

// Helper to duplicate string safely
static char *safe_strdup(const char *s) {
    if (!s) return NULL;
    char *dup = malloc(strlen(s) + 1);
    if (dup) {
        strcpy(dup, s);
    }
    return dup;
}

Expr *expr_new_number(double value, int line, int column) {
    Expr *expr = malloc(sizeof(Expr));
    if (expr) {
        expr->kind = EXPR_NUMBER;
        expr->line = line;
        expr->column = column;
        expr->as.number = value;
    }
    return expr;
}

Expr *expr_new_string(const char *value, int line, int column) {
    Expr *expr = malloc(sizeof(Expr));
    if (expr) {
        expr->kind = EXPR_STRING;
        expr->line = line;
        expr->column = column;
        expr->as.string = safe_strdup(value);
    }
    return expr;
}

Expr *expr_new_bool(bool value, int line, int column) {
    Expr *expr = malloc(sizeof(Expr));
    if (expr) {
        expr->kind = EXPR_BOOL;
        expr->line = line;
        expr->column = column;
        expr->as.boolean = value;
    }
    return expr;
}

Expr *expr_new_empty(int line, int column) {
    Expr *expr = malloc(sizeof(Expr));
    if (expr) {
        expr->kind = EXPR_EMPTY;
        expr->line = line;
        expr->column = column;
    }
    return expr;
}

Expr *expr_new_name(const char *name, int line, int column) {
    Expr *expr = malloc(sizeof(Expr));
    if (expr) {
        expr->kind = EXPR_NAME;
        expr->line = line;
        expr->column = column;
        expr->as.name = safe_strdup(name);
    }
    return expr;
}

Expr *expr_new_binary(TokenType op, Expr *left, Expr *right, int line, int column) {
    Expr *expr = malloc(sizeof(Expr));
    if (expr) {
        expr->kind = EXPR_BINARY;
        expr->line = line;
        expr->column = column;
        expr->as.binary.op = op;
        expr->as.binary.left = left;
        expr->as.binary.right = right;
    }
    return expr;
}

Expr *expr_new_unary(TokenType op, Expr *operand, int line, int column) {
    Expr *expr = malloc(sizeof(Expr));
    if (expr) {
        expr->kind = EXPR_UNARY;
        expr->line = line;
        expr->column = column;
        expr->as.unary.op = op;
        expr->as.unary.operand = operand;
    }
    return expr;
}

Expr *expr_new_list(Expr **elements, size_t count, int line, int column) {
    Expr *expr = malloc(sizeof(Expr));
    if (expr) {
        expr->kind = EXPR_LIST;
        expr->line = line;
        expr->column = column;
        expr->as.list.elements = elements;
        expr->as.list.count = count;
    }
    return expr;
}

Expr *expr_new_map(char **keys, Expr **values, size_t count, int line, int column) {
    Expr *expr = malloc(sizeof(Expr));
    if (expr) {
        expr->kind = EXPR_MAP;
        expr->line = line;
        expr->column = column;
        expr->as.map.keys = keys;
        expr->as.map.values = values;
        expr->as.map.count = count;
    }
    return expr;
}

Expr *expr_new_item_access(Expr *list, Expr *index, int line, int column) {
    Expr *expr = malloc(sizeof(Expr));
    if (expr) {
        expr->kind = EXPR_ITEM_ACCESS;
        expr->line = line;
        expr->column = column;
        expr->as.item_access.list = list;
        expr->as.item_access.index = index;
    }
    return expr;
}

Expr *expr_new_field_access(Expr *map, Expr *field, int line, int column) {
    Expr *expr = malloc(sizeof(Expr));
    if (expr) {
        expr->kind = EXPR_FIELD_ACCESS;
        expr->line = line;
        expr->column = column;
        expr->as.field_access.map = map;
        expr->as.field_access.field = field;
    }
    return expr;
}

Expr *expr_new_call(Expr *callee, Expr **args, size_t count, int line, int column) {
    Expr *expr = malloc(sizeof(Expr));
    if (expr) {
        expr->kind = EXPR_CALL;
        expr->line = line;
        expr->column = column;
        expr->as.call.callee = callee;
        expr->as.call.args = args;
        expr->as.call.count = count;
    }
    return expr;
}

void expr_free(Expr *expr) {
    if (!expr) return;
    switch (expr->kind) {
        case EXPR_NUMBER:
        case EXPR_BOOL:
        case EXPR_EMPTY:
            break;
        case EXPR_STRING:
            free(expr->as.string);
            break;
        case EXPR_NAME:
            free(expr->as.name);
            break;
        case EXPR_BINARY:
            expr_free(expr->as.binary.left);
            expr_free(expr->as.binary.right);
            break;
        case EXPR_UNARY:
            expr_free(expr->as.unary.operand);
            break;
        case EXPR_LIST:
            for (size_t i = 0; i < expr->as.list.count; i++) {
                expr_free(expr->as.list.elements[i]);
            }
            free(expr->as.list.elements);
            break;
        case EXPR_MAP:
            for (size_t i = 0; i < expr->as.map.count; i++) {
                free(expr->as.map.keys[i]);
                expr_free(expr->as.map.values[i]);
            }
            free(expr->as.map.keys);
            free(expr->as.map.values);
            break;
        case EXPR_ITEM_ACCESS:
            expr_free(expr->as.item_access.list);
            expr_free(expr->as.item_access.index);
            break;
        case EXPR_FIELD_ACCESS:
            expr_free(expr->as.field_access.map);
            expr_free(expr->as.field_access.field);
            break;
        case EXPR_CALL:
            expr_free(expr->as.call.callee);
            for (size_t i = 0; i < expr->as.call.count; i++) {
                expr_free(expr->as.call.args[i]);
            }
            free(expr->as.call.args);
            break;
    }
    free(expr);
}

Stmt *stmt_new_make(const char *name, Expr *initializer, int line, int column) {
    Stmt *stmt = malloc(sizeof(Stmt));
    if (stmt) {
        stmt->kind = STMT_MAKE;
        stmt->line = line;
        stmt->column = column;
        stmt->as.make.name = safe_strdup(name);
        stmt->as.make.initializer = initializer;
    }
    return stmt;
}

Stmt *stmt_new_keep(const char *name, Expr *initializer, int line, int column) {
    Stmt *stmt = malloc(sizeof(Stmt));
    if (stmt) {
        stmt->kind = STMT_KEEP;
        stmt->line = line;
        stmt->column = column;
        stmt->as.keep.name = safe_strdup(name);
        stmt->as.keep.initializer = initializer;
    }
    return stmt;
}

Stmt *stmt_new_assign(const char *name, Expr *value, int line, int column) {
    Stmt *stmt = malloc(sizeof(Stmt));
    if (stmt) {
        stmt->kind = STMT_ASSIGN;
        stmt->line = line;
        stmt->column = column;
        stmt->as.assign.name = safe_strdup(name);
        stmt->as.assign.value = value;
    }
    return stmt;
}

Stmt *stmt_new_say(Expr *expression, int line, int column) {
    Stmt *stmt = malloc(sizeof(Stmt));
    if (stmt) {
        stmt->kind = STMT_SAY;
        stmt->line = line;
        stmt->column = column;
        stmt->as.say.expression = expression;
    }
    return stmt;
}

Stmt *stmt_new_when(WhenBranch *branches, size_t branch_count, Block otherwise_branch, int line, int column) {
    Stmt *stmt = malloc(sizeof(Stmt));
    if (stmt) {
        stmt->kind = STMT_WHEN;
        stmt->line = line;
        stmt->column = column;
        stmt->as.when.branches = branches;
        stmt->as.when.branch_count = branch_count;
        stmt->as.when.otherwise_branch = otherwise_branch;
    }
    return stmt;
}

Stmt *stmt_new_repeat_count(Expr *count_expr, Block body, int line, int column) {
    Stmt *stmt = malloc(sizeof(Stmt));
    if (stmt) {
        stmt->kind = STMT_REPEAT_COUNT;
        stmt->line = line;
        stmt->column = column;
        stmt->as.repeat_count.count_expr = count_expr;
        stmt->as.repeat_count.body = body;
    }
    return stmt;
}

Stmt *stmt_new_repeat_range(const char *var_name, Expr *start, Expr *end, Block body, int line, int column) {
    Stmt *stmt = malloc(sizeof(Stmt));
    if (stmt) {
        stmt->kind = STMT_REPEAT_RANGE;
        stmt->line = line;
        stmt->column = column;
        stmt->as.repeat_range.var_name = safe_strdup(var_name);
        stmt->as.repeat_range.start = start;
        stmt->as.repeat_range.end = end;
        stmt->as.repeat_range.body = body;
    }
    return stmt;
}

Stmt *stmt_new_repeat_each(const char *var_name, Expr *collection, Block body, int line, int column) {
    Stmt *stmt = malloc(sizeof(Stmt));
    if (stmt) {
        stmt->kind = STMT_REPEAT_EACH;
        stmt->line = line;
        stmt->column = column;
        stmt->as.repeat_each.var_name = safe_strdup(var_name);
        stmt->as.repeat_each.collection = collection;
        stmt->as.repeat_each.body = body;
    }
    return stmt;
}

Stmt *stmt_new_during(Expr *condition, Block body, int line, int column) {
    Stmt *stmt = malloc(sizeof(Stmt));
    if (stmt) {
        stmt->kind = STMT_DURING;
        stmt->line = line;
        stmt->column = column;
        stmt->as.during.condition = condition;
        stmt->as.during.body = body;
    }
    return stmt;
}

Stmt *stmt_new_leave(int line, int column) {
    Stmt *stmt = malloc(sizeof(Stmt));
    if (stmt) {
        stmt->kind = STMT_LEAVE;
        stmt->line = line;
        stmt->column = column;
    }
    return stmt;
}

Stmt *stmt_new_skip(int line, int column) {
    Stmt *stmt = malloc(sizeof(Stmt));
    if (stmt) {
        stmt->kind = STMT_SKIP;
        stmt->line = line;
        stmt->column = column;
    }
    return stmt;
}

Stmt *stmt_new_task(const char *name, char **params, size_t param_count, Block body, int line, int column) {
    Stmt *stmt = malloc(sizeof(Stmt));
    if (stmt) {
        stmt->kind = STMT_TASK;
        stmt->line = line;
        stmt->column = column;
        stmt->as.task.name = safe_strdup(name);
        stmt->as.task.params = params;
        stmt->as.task.param_count = param_count;
        stmt->as.task.body = body;
    }
    return stmt;
}

Stmt *stmt_new_send(Expr *expression, int line, int column) {
    Stmt *stmt = malloc(sizeof(Stmt));
    if (stmt) {
        stmt->kind = STMT_SEND;
        stmt->line = line;
        stmt->column = column;
        stmt->as.send.expression = expression;
    }
    return stmt;
}

Stmt *stmt_new_grab(const char *module_name, int line, int column) {
    Stmt *stmt = malloc(sizeof(Stmt));
    if (stmt) {
        stmt->kind = STMT_GRAB;
        stmt->line = line;
        stmt->column = column;
        stmt->as.grab.module_name = safe_strdup(module_name);
    }
    return stmt;
}

Stmt *stmt_new_attempt(Block try_body, const char *problem_var, Block rescue_body, int line, int column) {
    Stmt *stmt = malloc(sizeof(Stmt));
    if (stmt) {
        stmt->kind = STMT_ATTEMPT;
        stmt->line = line;
        stmt->column = column;
        stmt->as.attempt.try_body = try_body;
        stmt->as.attempt.problem_var = safe_strdup(problem_var);
        stmt->as.attempt.rescue_body = rescue_body;
    }
    return stmt;
}

Stmt *stmt_new_choose(Expr *expr, ChooseCase *cases, size_t case_count, Block otherwise_branch, int line, int column) {
    Stmt *stmt = malloc(sizeof(Stmt));
    if (stmt) {
        stmt->kind = STMT_CHOOSE;
        stmt->line = line;
        stmt->column = column;
        stmt->as.choose.expr = expr;
        stmt->as.choose.cases = cases;
        stmt->as.choose.case_count = case_count;
        stmt->as.choose.otherwise_branch = otherwise_branch;
    }
    return stmt;
}

Stmt *stmt_new_put(Expr *value, Expr *list, int line, int column) {
    Stmt *stmt = malloc(sizeof(Stmt));
    if (stmt) {
        stmt->kind = STMT_PUT;
        stmt->line = line;
        stmt->column = column;
        stmt->as.put.value = value;
        stmt->as.put.list = list;
    }
    return stmt;
}

Stmt *stmt_new_set_field(Expr *map, Expr *field, Expr *value, int line, int column) {
    Stmt *stmt = malloc(sizeof(Stmt));
    if (stmt) {
        stmt->kind = STMT_SET_FIELD;
        stmt->line = line;
        stmt->column = column;
        stmt->as.set_field.map = map;
        stmt->as.set_field.field = field;
        stmt->as.set_field.value = value;
    }
    return stmt;
}

Stmt *stmt_new_hi_htmvss(int line, int column) {
    Stmt *stmt = malloc(sizeof(Stmt));
    if (stmt) {
        stmt->kind = STMT_HI_HTMVSS;
        stmt->line = line;
        stmt->column = column;
    }
    return stmt;
}

Stmt *stmt_new_bye_htmvss(int line, int column) {
    Stmt *stmt = malloc(sizeof(Stmt));
    if (stmt) {
        stmt->kind = STMT_BYE_HTMVSS;
        stmt->line = line;
        stmt->column = column;
    }
    return stmt;
}

Stmt *stmt_new_expr(Expr *expression, int line, int column) {
    Stmt *stmt = malloc(sizeof(Stmt));
    if (stmt) {
        stmt->kind = STMT_EXPR;
        stmt->line = line;
        stmt->column = column;
        stmt->as.expr_stmt.expression = expression;
    }
    return stmt;
}

void block_free(Block block) {
    if (block.statements) {
        for (size_t i = 0; i < block.count; i++) {
            stmt_free(block.statements[i]);
        }
        free(block.statements);
    }
}

void stmt_free(Stmt *stmt) {
    if (!stmt) return;
    switch (stmt->kind) {
        case STMT_MAKE:
            free(stmt->as.make.name);
            expr_free(stmt->as.make.initializer);
            break;
        case STMT_KEEP:
            free(stmt->as.keep.name);
            expr_free(stmt->as.keep.initializer);
            break;
        case STMT_ASSIGN:
            free(stmt->as.assign.name);
            expr_free(stmt->as.assign.value);
            break;
        case STMT_SAY:
            expr_free(stmt->as.say.expression);
            break;
        case STMT_WHEN:
            for (size_t i = 0; i < stmt->as.when.branch_count; i++) {
                expr_free(stmt->as.when.branches[i].condition);
                block_free(stmt->as.when.branches[i].block);
            }
            free(stmt->as.when.branches);
            block_free(stmt->as.when.otherwise_branch);
            break;
        case STMT_REPEAT_COUNT:
            expr_free(stmt->as.repeat_count.count_expr);
            block_free(stmt->as.repeat_count.body);
            break;
        case STMT_REPEAT_RANGE:
            free(stmt->as.repeat_range.var_name);
            expr_free(stmt->as.repeat_range.start);
            expr_free(stmt->as.repeat_range.end);
            block_free(stmt->as.repeat_range.body);
            break;
        case STMT_REPEAT_EACH:
            free(stmt->as.repeat_each.var_name);
            expr_free(stmt->as.repeat_each.collection);
            block_free(stmt->as.repeat_each.body);
            break;
        case STMT_DURING:
            expr_free(stmt->as.during.condition);
            block_free(stmt->as.during.body);
            break;
        case STMT_LEAVE:
        case STMT_SKIP:
            break;
        case STMT_TASK:
            free(stmt->as.task.name);
            for (size_t i = 0; i < stmt->as.task.param_count; i++) {
                free(stmt->as.task.params[i]);
            }
            free(stmt->as.task.params);
            block_free(stmt->as.task.body);
            break;
        case STMT_SEND:
            expr_free(stmt->as.send.expression);
            break;
        case STMT_GRAB:
            free(stmt->as.grab.module_name);
            break;
        case STMT_ATTEMPT:
            block_free(stmt->as.attempt.try_body);
            free(stmt->as.attempt.problem_var);
            block_free(stmt->as.attempt.rescue_body);
            break;
        case STMT_CHOOSE:
            expr_free(stmt->as.choose.expr);
            for (size_t i = 0; i < stmt->as.choose.case_count; i++) {
                expr_free(stmt->as.choose.cases[i].expr);
                block_free(stmt->as.choose.cases[i].block);
            }
            free(stmt->as.choose.cases);
            block_free(stmt->as.choose.otherwise_branch);
            break;
        case STMT_PUT:
            expr_free(stmt->as.put.value);
            expr_free(stmt->as.put.list);
            break;
        case STMT_SET_FIELD:
            expr_free(stmt->as.set_field.map);
            expr_free(stmt->as.set_field.field);
            expr_free(stmt->as.set_field.value);
            break;
        case STMT_HI_HTMVSS:
        case STMT_BYE_HTMVSS:
            break;
        case STMT_EXPR:
            expr_free(stmt->as.expr_stmt.expression);
            break;
    }
    free(stmt);
}
