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

VSS_Expr *vss_expr_new_number(double value, int line, int column) {
    VSS_Expr *expr = malloc(sizeof(VSS_Expr));
    if (expr) {
        expr->kind = VSS_EXPR_NUMBER;
        expr->line = line;
        expr->column = column;
        expr->as.number = value;
    }
    return expr;
}

VSS_Expr *vss_expr_new_string(const char *value, int line, int column) {
    VSS_Expr *expr = malloc(sizeof(VSS_Expr));
    if (expr) {
        expr->kind = VSS_EXPR_STRING;
        expr->line = line;
        expr->column = column;
        expr->as.string = safe_strdup(value);
    }
    return expr;
}

VSS_Expr *vss_expr_new_bool(bool value, int line, int column) {
    VSS_Expr *expr = malloc(sizeof(VSS_Expr));
    if (expr) {
        expr->kind = VSS_EXPR_BOOL;
        expr->line = line;
        expr->column = column;
        expr->as.boolean = value;
    }
    return expr;
}

VSS_Expr *vss_expr_new_empty(int line, int column) {
    VSS_Expr *expr = malloc(sizeof(VSS_Expr));
    if (expr) {
        expr->kind = VSS_EXPR_EMPTY;
        expr->line = line;
        expr->column = column;
    }
    return expr;
}

VSS_Expr *vss_expr_new_name(const char *name, int line, int column) {
    VSS_Expr *expr = malloc(sizeof(VSS_Expr));
    if (expr) {
        expr->kind = VSS_EXPR_NAME;
        expr->line = line;
        expr->column = column;
        expr->as.name = safe_strdup(name);
    }
    return expr;
}

VSS_Expr *vss_expr_new_binary(VSS_TokenType op, VSS_Expr *left, VSS_Expr *right, int line, int column) {
    VSS_Expr *expr = malloc(sizeof(VSS_Expr));
    if (expr) {
        expr->kind = VSS_EXPR_BINARY;
        expr->line = line;
        expr->column = column;
        expr->as.binary.op = op;
        expr->as.binary.left = left;
        expr->as.binary.right = right;
    }
    return expr;
}

VSS_Expr *vss_expr_new_unary(VSS_TokenType op, VSS_Expr *operand, int line, int column) {
    VSS_Expr *expr = malloc(sizeof(VSS_Expr));
    if (expr) {
        expr->kind = VSS_EXPR_UNARY;
        expr->line = line;
        expr->column = column;
        expr->as.unary.op = op;
        expr->as.unary.operand = operand;
    }
    return expr;
}

VSS_Expr *vss_expr_new_list(VSS_Expr **elements, size_t count, int line, int column) {
    VSS_Expr *expr = malloc(sizeof(VSS_Expr));
    if (expr) {
        expr->kind = VSS_EXPR_LIST;
        expr->line = line;
        expr->column = column;
        expr->as.list.elements = elements;
        expr->as.list.count = count;
    }
    return expr;
}

VSS_Expr *vss_expr_new_map(char **keys, VSS_Expr **values, size_t count, int line, int column) {
    VSS_Expr *expr = malloc(sizeof(VSS_Expr));
    if (expr) {
        expr->kind = VSS_EXPR_MAP;
        expr->line = line;
        expr->column = column;
        expr->as.map.keys = keys;
        expr->as.map.values = values;
        expr->as.map.count = count;
    }
    return expr;
}

VSS_Expr *vss_expr_new_item_access(VSS_Expr *list, VSS_Expr *index, int line, int column) {
    VSS_Expr *expr = malloc(sizeof(VSS_Expr));
    if (expr) {
        expr->kind = VSS_EXPR_ITEM_ACCESS;
        expr->line = line;
        expr->column = column;
        expr->as.item_access.list = list;
        expr->as.item_access.index = index;
    }
    return expr;
}

VSS_Expr *vss_expr_new_field_access(VSS_Expr *map, VSS_Expr *field, int line, int column) {
    VSS_Expr *expr = malloc(sizeof(VSS_Expr));
    if (expr) {
        expr->kind = VSS_EXPR_FIELD_ACCESS;
        expr->line = line;
        expr->column = column;
        expr->as.field_access.map = map;
        expr->as.field_access.field = field;
    }
    return expr;
}

VSS_Expr *vss_expr_new_call(VSS_Expr *callee, VSS_Expr **args, size_t count, int line, int column) {
    VSS_Expr *expr = malloc(sizeof(VSS_Expr));
    if (expr) {
        expr->kind = VSS_EXPR_CALL;
        expr->line = line;
        expr->column = column;
        expr->as.call.callee = callee;
        expr->as.call.args = args;
        expr->as.call.count = count;
    }
    return expr;
}

void vss_expr_free(VSS_Expr *expr) {
    if (!expr) return;
    switch (expr->kind) {
        case VSS_EXPR_NUMBER:
        case VSS_EXPR_BOOL:
        case VSS_EXPR_EMPTY:
            break;
        case VSS_EXPR_STRING:
            free(expr->as.string);
            break;
        case VSS_EXPR_NAME:
            free(expr->as.name);
            break;
        case VSS_EXPR_BINARY:
            vss_expr_free(expr->as.binary.left);
            vss_expr_free(expr->as.binary.right);
            break;
        case VSS_EXPR_UNARY:
            vss_expr_free(expr->as.unary.operand);
            break;
        case VSS_EXPR_LIST:
            for (size_t i = 0; i < expr->as.list.count; i++) {
                vss_expr_free(expr->as.list.elements[i]);
            }
            free(expr->as.list.elements);
            break;
        case VSS_EXPR_MAP:
            for (size_t i = 0; i < expr->as.map.count; i++) {
                free(expr->as.map.keys[i]);
                vss_expr_free(expr->as.map.values[i]);
            }
            free(expr->as.map.keys);
            free(expr->as.map.values);
            break;
        case VSS_EXPR_ITEM_ACCESS:
            vss_expr_free(expr->as.item_access.list);
            vss_expr_free(expr->as.item_access.index);
            break;
        case VSS_EXPR_FIELD_ACCESS:
            vss_expr_free(expr->as.field_access.map);
            vss_expr_free(expr->as.field_access.field);
            break;
        case VSS_EXPR_CALL:
            vss_expr_free(expr->as.call.callee);
            for (size_t i = 0; i < expr->as.call.count; i++) {
                vss_expr_free(expr->as.call.args[i]);
            }
            free(expr->as.call.args);
            break;
    }
    free(expr);
}

VSS_Stmt *vss_stmt_new_make(const char *name, VSS_Expr *initializer, int line, int column) {
    VSS_Stmt *stmt = malloc(sizeof(VSS_Stmt));
    if (stmt) {
        stmt->kind = VSS_STMT_MAKE;
        stmt->line = line;
        stmt->column = column;
        stmt->as.make.name = safe_strdup(name);
        stmt->as.make.initializer = initializer;
    }
    return stmt;
}

VSS_Stmt *vss_stmt_new_keep(const char *name, VSS_Expr *initializer, int line, int column) {
    VSS_Stmt *stmt = malloc(sizeof(VSS_Stmt));
    if (stmt) {
        stmt->kind = VSS_STMT_KEEP;
        stmt->line = line;
        stmt->column = column;
        stmt->as.keep.name = safe_strdup(name);
        stmt->as.keep.initializer = initializer;
    }
    return stmt;
}

VSS_Stmt *vss_stmt_new_assign(const char *name, VSS_Expr *value, int line, int column) {
    VSS_Stmt *stmt = malloc(sizeof(VSS_Stmt));
    if (stmt) {
        stmt->kind = VSS_STMT_ASSIGN;
        stmt->line = line;
        stmt->column = column;
        stmt->as.assign.name = safe_strdup(name);
        stmt->as.assign.value = value;
    }
    return stmt;
}

VSS_Stmt *vss_stmt_new_say(VSS_Expr *expression, int line, int column) {
    VSS_Stmt *stmt = malloc(sizeof(VSS_Stmt));
    if (stmt) {
        stmt->kind = VSS_STMT_SAY;
        stmt->line = line;
        stmt->column = column;
        stmt->as.say.expression = expression;
    }
    return stmt;
}

VSS_Stmt *vss_stmt_new_when(VSS_WhenBranch *branches, size_t branch_count, VSS_Block otherwise_branch, int line, int column) {
    VSS_Stmt *stmt = malloc(sizeof(VSS_Stmt));
    if (stmt) {
        stmt->kind = VSS_STMT_WHEN;
        stmt->line = line;
        stmt->column = column;
        stmt->as.when.branches = branches;
        stmt->as.when.branch_count = branch_count;
        stmt->as.when.otherwise_branch = otherwise_branch;
    }
    return stmt;
}

VSS_Stmt *vss_stmt_new_repeat_count(VSS_Expr *count_expr, VSS_Block body, int line, int column) {
    VSS_Stmt *stmt = malloc(sizeof(VSS_Stmt));
    if (stmt) {
        stmt->kind = VSS_STMT_REPEAT_COUNT;
        stmt->line = line;
        stmt->column = column;
        stmt->as.repeat_count.count_expr = count_expr;
        stmt->as.repeat_count.body = body;
    }
    return stmt;
}

VSS_Stmt *vss_stmt_new_repeat_range(const char *var_name, VSS_Expr *start, VSS_Expr *end, VSS_Block body, int line, int column) {
    VSS_Stmt *stmt = malloc(sizeof(VSS_Stmt));
    if (stmt) {
        stmt->kind = VSS_STMT_REPEAT_RANGE;
        stmt->line = line;
        stmt->column = column;
        stmt->as.repeat_range.var_name = safe_strdup(var_name);
        stmt->as.repeat_range.start = start;
        stmt->as.repeat_range.end = end;
        stmt->as.repeat_range.body = body;
    }
    return stmt;
}

VSS_Stmt *vss_stmt_new_repeat_each(const char *var_name, VSS_Expr *collection, VSS_Block body, int line, int column) {
    VSS_Stmt *stmt = malloc(sizeof(VSS_Stmt));
    if (stmt) {
        stmt->kind = VSS_STMT_REPEAT_EACH;
        stmt->line = line;
        stmt->column = column;
        stmt->as.repeat_each.var_name = safe_strdup(var_name);
        stmt->as.repeat_each.collection = collection;
        stmt->as.repeat_each.body = body;
    }
    return stmt;
}

VSS_Stmt *vss_stmt_new_during(VSS_Expr *condition, VSS_Block body, int line, int column) {
    VSS_Stmt *stmt = malloc(sizeof(VSS_Stmt));
    if (stmt) {
        stmt->kind = VSS_STMT_DURING;
        stmt->line = line;
        stmt->column = column;
        stmt->as.during.condition = condition;
        stmt->as.during.body = body;
    }
    return stmt;
}

VSS_Stmt *vss_stmt_new_leave(int line, int column) {
    VSS_Stmt *stmt = malloc(sizeof(VSS_Stmt));
    if (stmt) {
        stmt->kind = VSS_STMT_LEAVE;
        stmt->line = line;
        stmt->column = column;
    }
    return stmt;
}

VSS_Stmt *vss_stmt_new_skip(int line, int column) {
    VSS_Stmt *stmt = malloc(sizeof(VSS_Stmt));
    if (stmt) {
        stmt->kind = VSS_STMT_SKIP;
        stmt->line = line;
        stmt->column = column;
    }
    return stmt;
}

VSS_Stmt *vss_stmt_new_task(const char *name, char **params, size_t param_count, VSS_Block body, int line, int column) {
    VSS_Stmt *stmt = malloc(sizeof(VSS_Stmt));
    if (stmt) {
        stmt->kind = VSS_STMT_TASK;
        stmt->line = line;
        stmt->column = column;
        stmt->as.task.name = safe_strdup(name);
        stmt->as.task.params = params;
        stmt->as.task.param_count = param_count;
        stmt->as.task.body = body;
    }
    return stmt;
}

VSS_Stmt *vss_stmt_new_send(VSS_Expr *expression, int line, int column) {
    VSS_Stmt *stmt = malloc(sizeof(VSS_Stmt));
    if (stmt) {
        stmt->kind = VSS_STMT_SEND;
        stmt->line = line;
        stmt->column = column;
        stmt->as.send.expression = expression;
    }
    return stmt;
}

VSS_Stmt *vss_stmt_new_grab(const char *module_name, int line, int column) {
    VSS_Stmt *stmt = malloc(sizeof(VSS_Stmt));
    if (stmt) {
        stmt->kind = VSS_STMT_GRAB;
        stmt->line = line;
        stmt->column = column;
        stmt->as.grab.module_name = safe_strdup(module_name);
    }
    return stmt;
}

VSS_Stmt *vss_stmt_new_attempt(VSS_Block try_body, const char *problem_var, VSS_Block rescue_body, int line, int column) {
    VSS_Stmt *stmt = malloc(sizeof(VSS_Stmt));
    if (stmt) {
        stmt->kind = VSS_STMT_ATTEMPT;
        stmt->line = line;
        stmt->column = column;
        stmt->as.attempt.try_body = try_body;
        stmt->as.attempt.problem_var = safe_strdup(problem_var);
        stmt->as.attempt.rescue_body = rescue_body;
    }
    return stmt;
}

VSS_Stmt *vss_stmt_new_choose(VSS_Expr *expr, VSS_ChooseCase *cases, size_t case_count, VSS_Block otherwise_branch, int line, int column) {
    VSS_Stmt *stmt = malloc(sizeof(VSS_Stmt));
    if (stmt) {
        stmt->kind = VSS_STMT_CHOOSE;
        stmt->line = line;
        stmt->column = column;
        stmt->as.choose.expr = expr;
        stmt->as.choose.cases = cases;
        stmt->as.choose.case_count = case_count;
        stmt->as.choose.otherwise_branch = otherwise_branch;
    }
    return stmt;
}

VSS_Stmt *vss_stmt_new_put(VSS_Expr *value, VSS_Expr *list, int line, int column) {
    VSS_Stmt *stmt = malloc(sizeof(VSS_Stmt));
    if (stmt) {
        stmt->kind = VSS_STMT_PUT;
        stmt->line = line;
        stmt->column = column;
        stmt->as.put.value = value;
        stmt->as.put.list = list;
    }
    return stmt;
}

VSS_Stmt *vss_stmt_new_set_field(VSS_Expr *map, VSS_Expr *field, VSS_Expr *value, int line, int column) {
    VSS_Stmt *stmt = malloc(sizeof(VSS_Stmt));
    if (stmt) {
        stmt->kind = VSS_STMT_SET_FIELD;
        stmt->line = line;
        stmt->column = column;
        stmt->as.set_field.map = map;
        stmt->as.set_field.field = field;
        stmt->as.set_field.value = value;
    }
    return stmt;
}

VSS_Stmt *vss_stmt_new_hi_htmvss(int line, int column) {
    VSS_Stmt *stmt = malloc(sizeof(VSS_Stmt));
    if (stmt) {
        stmt->kind = VSS_STMT_HI_HTMVSS;
        stmt->line = line;
        stmt->column = column;
    }
    return stmt;
}

VSS_Stmt *vss_stmt_new_bye_htmvss(int line, int column) {
    VSS_Stmt *stmt = malloc(sizeof(VSS_Stmt));
    if (stmt) {
        stmt->kind = VSS_STMT_BYE_HTMVSS;
        stmt->line = line;
        stmt->column = column;
    }
    return stmt;
}

VSS_Stmt *vss_stmt_new_expr(VSS_Expr *expression, int line, int column) {
    VSS_Stmt *stmt = malloc(sizeof(VSS_Stmt));
    if (stmt) {
        stmt->kind = VSS_STMT_EXPR;
        stmt->line = line;
        stmt->column = column;
        stmt->as.expr_stmt.expression = expression;
    }
    return stmt;
}

void vss_block_free(VSS_Block block) {
    if (block.statements) {
        for (size_t i = 0; i < block.count; i++) {
            vss_stmt_free(block.statements[i]);
        }
        free(block.statements);
    }
}

void vss_stmt_free(VSS_Stmt *stmt) {
    if (!stmt) return;
    switch (stmt->kind) {
        case VSS_STMT_MAKE:
            free(stmt->as.make.name);
            vss_expr_free(stmt->as.make.initializer);
            break;
        case VSS_STMT_KEEP:
            free(stmt->as.keep.name);
            vss_expr_free(stmt->as.keep.initializer);
            break;
        case VSS_STMT_ASSIGN:
            free(stmt->as.assign.name);
            vss_expr_free(stmt->as.assign.value);
            break;
        case VSS_STMT_SAY:
            vss_expr_free(stmt->as.say.expression);
            break;
        case VSS_STMT_WHEN:
            for (size_t i = 0; i < stmt->as.when.branch_count; i++) {
                vss_expr_free(stmt->as.when.branches[i].condition);
                vss_block_free(stmt->as.when.branches[i].block);
            }
            free(stmt->as.when.branches);
            vss_block_free(stmt->as.when.otherwise_branch);
            break;
        case VSS_STMT_REPEAT_COUNT:
            vss_expr_free(stmt->as.repeat_count.count_expr);
            vss_block_free(stmt->as.repeat_count.body);
            break;
        case VSS_STMT_REPEAT_RANGE:
            free(stmt->as.repeat_range.var_name);
            vss_expr_free(stmt->as.repeat_range.start);
            vss_expr_free(stmt->as.repeat_range.end);
            vss_block_free(stmt->as.repeat_range.body);
            break;
        case VSS_STMT_REPEAT_EACH:
            free(stmt->as.repeat_each.var_name);
            vss_expr_free(stmt->as.repeat_each.collection);
            vss_block_free(stmt->as.repeat_each.body);
            break;
        case VSS_STMT_DURING:
            vss_expr_free(stmt->as.during.condition);
            vss_block_free(stmt->as.during.body);
            break;
        case VSS_STMT_LEAVE:
        case VSS_STMT_SKIP:
            break;
        case VSS_STMT_TASK:
            free(stmt->as.task.name);
            for (size_t i = 0; i < stmt->as.task.param_count; i++) {
                free(stmt->as.task.params[i]);
            }
            free(stmt->as.task.params);
            vss_block_free(stmt->as.task.body);
            break;
        case VSS_STMT_SEND:
            vss_expr_free(stmt->as.send.expression);
            break;
        case VSS_STMT_GRAB:
            free(stmt->as.grab.module_name);
            break;
        case VSS_STMT_ATTEMPT:
            vss_block_free(stmt->as.attempt.try_body);
            free(stmt->as.attempt.problem_var);
            vss_block_free(stmt->as.attempt.rescue_body);
            break;
        case VSS_STMT_CHOOSE:
            vss_expr_free(stmt->as.choose.expr);
            for (size_t i = 0; i < stmt->as.choose.case_count; i++) {
                vss_expr_free(stmt->as.choose.cases[i].expr);
                vss_block_free(stmt->as.choose.cases[i].block);
            }
            free(stmt->as.choose.cases);
            vss_block_free(stmt->as.choose.otherwise_branch);
            break;
        case VSS_STMT_PUT:
            vss_expr_free(stmt->as.put.value);
            vss_expr_free(stmt->as.put.list);
            break;
        case VSS_STMT_SET_FIELD:
            vss_expr_free(stmt->as.set_field.map);
            vss_expr_free(stmt->as.set_field.field);
            vss_expr_free(stmt->as.set_field.value);
            break;
        case VSS_STMT_HI_HTMVSS:
        case VSS_STMT_BYE_HTMVSS:
            break;
        case VSS_STMT_EXPR:
            vss_expr_free(stmt->as.expr_stmt.expression);
            break;
    }
    free(stmt);
}
