#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "interpreter.h"
#include "parser.h"

// Helper to duplicate string safely
static char *safe_strdup(const char *s) {
    if (!s) return NULL;
    char *dup = malloc(strlen(s) + 1);
    if (dup) {
        strcpy(dup, s);
    }
    return dup;
}

static FlowResult flow_normal(void) {
    FlowResult res;
    res.type = FLOW_NORMAL;
    res.value = value_new_empty();
    res.error_msg = NULL;
    res.line = 0;
    res.column = 0;
    return res;
}

static FlowResult flow_error(int line, int col, const char *format, ...) {
    FlowResult res;
    res.type = FLOW_ERROR;
    res.value = value_new_empty();
    res.line = line;
    res.column = col;

    va_list args;
    va_start(args, format);
    // Determine size
    int len = vsnprintf(NULL, 0, format, args);
    va_end(args);

    res.error_msg = malloc(len + 1);
    va_start(args, format);
    vsnprintf(res.error_msg, len + 1, format, args);
    va_end(args);

    return res;
}

// Forward declarations of evaluation helpers
static FlowResult eval_expr(Expr *expr, Env *env, Value *out_val);

// Built-in Native Functions
static Value builtin_size(size_t arg_count, Value *args, bool *out_error, char **out_error_msg) {
    if (arg_count != 1) {
        *out_error = true;
        *out_error_msg = safe_strdup("size of expects exactly 1 argument");
        return value_new_empty();
    }
    Value val = args[0];
    if (val.type == VAL_LIST) {
        return value_new_number(val.as.list->count);
    } else if (val.type == VAL_MAP) {
        return value_new_number(val.as.map->count);
    } else if (val.type == VAL_STRING) {
        return value_new_number(strlen(val.as.string->chars));
    } else {
        *out_error = true;
        *out_error_msg = safe_strdup("size of expects a list, map, or string");
        return value_new_empty();
    }
}

static Value builtin_exists(size_t arg_count, Value *args, bool *out_error, char **out_error_msg) {
    if (arg_count != 1) {
        *out_error = true;
        *out_error_msg = safe_strdup("exists expects exactly 1 argument");
        return value_new_empty();
    }
    if (args[0].type != VAL_STRING) {
        *out_error = true;
        *out_error_msg = safe_strdup("exists expects a string file path");
        return value_new_empty();
    }
    const char *path = args[0].as.string->chars;
    FILE *f = fopen(path, "rb");
    if (f) {
        fclose(f);
        return value_new_bool(true);
    }
    return value_new_bool(false);
}

static Value builtin_read(size_t arg_count, Value *args, bool *out_error, char **out_error_msg) {
    if (arg_count != 1) {
        *out_error = true;
        *out_error_msg = safe_strdup("read expects exactly 1 argument");
        return value_new_empty();
    }
    if (args[0].type != VAL_STRING) {
        *out_error = true;
        *out_error_msg = safe_strdup("read expects a string file path");
        return value_new_empty();
    }
    const char *path = args[0].as.string->chars;
    FILE *file = fopen(path, "rb");
    if (!file) {
        *out_error = true;
        *out_error_msg = malloc(strlen(path) + 32);
        sprintf(*out_error_msg, "Could not open file for reading: %s", path);
        return value_new_empty();
    }

    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    rewind(file);

    char *buffer = malloc(size + 1);
    size_t read_bytes = fread(buffer, 1, size, file);
    fclose(file);
    buffer[read_bytes] = '\0';

    Value res = value_new_string(buffer);
    free(buffer);
    return res;
}

static Value builtin_write(size_t arg_count, Value *args, bool *out_error, char **out_error_msg) {
    if (arg_count != 2) {
        *out_error = true;
        *out_error_msg = safe_strdup("write expects content and path");
        return value_new_empty();
    }
    if (args[0].type != VAL_STRING || args[1].type != VAL_STRING) {
        *out_error = true;
        *out_error_msg = safe_strdup("write expects string content and path");
        return value_new_empty();
    }
    const char *content = args[0].as.string->chars;
    const char *path = args[1].as.string->chars;
    FILE *file = fopen(path, "wb");
    if (!file) {
        *out_error = true;
        *out_error_msg = malloc(strlen(path) + 32);
        sprintf(*out_error_msg, "Could not open file for writing: %s", path);
        return value_new_empty();
    }
    fwrite(content, 1, strlen(content), file);
    fclose(file);
    return value_new_empty();
}

static Value builtin_add(size_t arg_count, Value *args, bool *out_error, char **out_error_msg) {
    if (arg_count != 2) {
        *out_error = true;
        *out_error_msg = safe_strdup("add expects content and path");
        return value_new_empty();
    }
    if (args[0].type != VAL_STRING || args[1].type != VAL_STRING) {
        *out_error = true;
        *out_error_msg = safe_strdup("add expects string content and path");
        return value_new_empty();
    }
    const char *content = args[0].as.string->chars;
    const char *path = args[1].as.string->chars;
    FILE *file = fopen(path, "ab");
    if (!file) {
        *out_error = true;
        *out_error_msg = malloc(strlen(path) + 32);
        sprintf(*out_error_msg, "Could not open file for appending: %s", path);
        return value_new_empty();
    }
    fwrite(content, 1, strlen(content), file);
    fclose(file);
    return value_new_empty();
}

static Value builtin_erase(size_t arg_count, Value *args, bool *out_error, char **out_error_msg) {
    if (arg_count != 1) {
        *out_error = true;
        *out_error_msg = safe_strdup("erase expects exactly 1 argument");
        return value_new_empty();
    }
    if (args[0].type != VAL_STRING) {
        *out_error = true;
        *out_error_msg = safe_strdup("erase expects a string file path");
        return value_new_empty();
    }
    const char *path = args[0].as.string->chars;
    if (remove(path) != 0) {
        *out_error = true;
        *out_error_msg = malloc(strlen(path) + 32);
        sprintf(*out_error_msg, "Could not delete file: %s", path);
        return value_new_empty();
    }
    return value_new_empty();
}

void register_builtins(Env *env) {
    env_define(env, "__size", value_new_native(builtin_size));
    env_define(env, "__exists", value_new_native(builtin_exists));
    env_define(env, "__read", value_new_native(builtin_read));
    env_define(env, "__write", value_new_native(builtin_write));
    env_define(env, "__add", value_new_native(builtin_add));
    env_define(env, "__erase", value_new_native(builtin_erase));
}

// Expression evaluation
static FlowResult eval_expr(Expr *expr, Env *env, Value *out_val) {
    if (!expr) {
        *out_val = value_new_empty();
        return flow_normal();
    }

    switch (expr->kind) {
        case EXPR_NUMBER:
            *out_val = value_new_number(expr->as.number);
            return flow_normal();
        case EXPR_STRING:
            *out_val = value_new_string(expr->as.string);
            return flow_normal();
        case EXPR_BOOL:
            *out_val = value_new_bool(expr->as.boolean);
            return flow_normal();
        case EXPR_EMPTY:
            *out_val = value_new_empty();
            return flow_normal();
        case EXPR_NAME: {
            if (!env_get(env, expr->as.name, out_val)) {
                return flow_error(expr->line, expr->column, "Undefined variable '%s'.", expr->as.name);
            }
            return flow_normal();
        }
        case EXPR_UNARY: {
            Value operand;
            FlowResult res = eval_expr(expr->as.unary.operand, env, &operand);
            if (res.type != FLOW_NORMAL) return res;

            if (expr->as.unary.op == TOKEN_MINUS) {
                if (operand.type != VAL_NUMBER) {
                    value_release(operand);
                    return flow_error(expr->line, expr->column, "Operand to '-' must be a number.");
                }
                *out_val = value_new_number(-operand.as.number);
                value_release(operand);
                return flow_normal();
            } else if (expr->as.unary.op == TOKEN_NOT) {
                *out_val = value_new_bool(!value_truthy(operand));
                value_release(operand);
                return flow_normal();
            }
            value_release(operand);
            return flow_error(expr->line, expr->column, "Unknown unary operator.");
        }
        case EXPR_BINARY: {
            // Short-circuit logical operators
            if (expr->as.binary.op == TOKEN_OR) {
                Value left;
                FlowResult res = eval_expr(expr->as.binary.left, env, &left);
                if (res.type != FLOW_NORMAL) return res;
                if (value_truthy(left)) {
                    *out_val = left;
                    return flow_normal();
                }
                value_release(left);
                return eval_expr(expr->as.binary.right, env, out_val);
            }
            if (expr->as.binary.op == TOKEN_AND) {
                Value left;
                FlowResult res = eval_expr(expr->as.binary.left, env, &left);
                if (res.type != FLOW_NORMAL) return res;
                if (!value_truthy(left)) {
                    *out_val = left;
                    return flow_normal();
                }
                value_release(left);
                return eval_expr(expr->as.binary.right, env, out_val);
            }

            Value left, right;
            FlowResult res = eval_expr(expr->as.binary.left, env, &left);
            if (res.type != FLOW_NORMAL) return res;
            res = eval_expr(expr->as.binary.right, env, &right);
            if (res.type != FLOW_NORMAL) {
                value_release(left);
                return res;
            }

            TokenType op = expr->as.binary.op;
            if (op == TOKEN_PLUS) {
                if (left.type == VAL_NUMBER && right.type == VAL_NUMBER) {
                    *out_val = value_new_number(left.as.number + right.as.number);
                    value_release(left); value_release(right);
                    return flow_normal();
                } else if (left.type == VAL_STRING && right.type == VAL_STRING) {
                    char *joined = malloc(strlen(left.as.string->chars) + strlen(right.as.string->chars) + 1);
                    strcpy(joined, left.as.string->chars);
                    strcat(joined, right.as.string->chars);
                    *out_val = value_new_string(joined);
                    free(joined);
                    value_release(left); value_release(right);
                    return flow_normal();
                } else {
                    value_release(left); value_release(right);
                    return flow_error(expr->line, expr->column, "Can only add numbers or join strings.");
                }
            }

            // Other arithmetic operators
            if (op == TOKEN_MINUS || op == TOKEN_STAR || op == TOKEN_SLASH || op == TOKEN_PERCENT) {
                if (left.type != VAL_NUMBER || right.type != VAL_NUMBER) {
                    value_release(left); value_release(right);
                    return flow_error(expr->line, expr->column, "Arithmetic operands must be numbers.");
                }
                double l = left.as.number;
                double r = right.as.number;
                value_release(left); value_release(right);

                if (op == TOKEN_MINUS) *out_val = value_new_number(l - r);
                else if (op == TOKEN_STAR) *out_val = value_new_number(l * r);
                else if (op == TOKEN_SLASH) {
                    if (r == 0.0) return flow_error(expr->line, expr->column, "Division by zero.");
                    *out_val = value_new_number(l / r);
                } else {
                    if (r == 0.0) return flow_error(expr->line, expr->column, "Modulo by zero.");
                    // Standard C Modulo on doubles using fmod? Let's just cast to long long, or use standard double mod.
                    // Casting to long is standard for small % operators.
                    *out_val = value_new_number((long long)l % (long long)r);
                }
                return flow_normal();
            }

            // Numeric comparisons
            if (op == TOKEN_ABOVE || op == TOKEN_BELOW || op == TOKEN_AT_LEAST || op == TOKEN_AT_MOST) {
                if (left.type != VAL_NUMBER || right.type != VAL_NUMBER) {
                    value_release(left); value_release(right);
                    return flow_error(expr->line, expr->column, "Comparison operands must be numbers.");
                }
                double l = left.as.number;
                double r = right.as.number;
                value_release(left); value_release(right);

                if (op == TOKEN_ABOVE) *out_val = value_new_bool(l > r);
                else if (op == TOKEN_BELOW) *out_val = value_new_bool(l < r);
                else if (op == TOKEN_AT_LEAST) *out_val = value_new_bool(l >= r);
                else *out_val = value_new_bool(l <= r);
                return flow_normal();
            }

            // Equality comparisons
            if (op == TOKEN_SAME_AS || op == TOKEN_NOT_SAME_AS) {
                bool same = value_same_as(left, right);
                *out_val = value_new_bool(op == TOKEN_SAME_AS ? same : !same);
                value_release(left); value_release(right);
                return flow_normal();
            }

            value_release(left); value_release(right);
            return flow_error(expr->line, expr->column, "Unknown binary operator.");
        }
        case EXPR_LIST: {
            Value list_val = value_new_list();
            for (size_t i = 0; i < expr->as.list.count; i++) {
                Value elem;
                FlowResult res = eval_expr(expr->as.list.elements[i], env, &elem);
                if (res.type != FLOW_NORMAL) {
                    value_release(list_val);
                    return res;
                }
                // Append elem to list_val
                ValList *l = list_val.as.list;
                if (l->count >= l->capacity) {
                    l->capacity = l->capacity == 0 ? 8 : l->capacity * 2;
                    l->items = realloc(l->items, sizeof(Value) * l->capacity);
                }
                l->items[l->count++] = elem; // Holds reference
            }
            *out_val = list_val;
            return flow_normal();
        }
        case EXPR_MAP: {
            Value map_val = value_new_map();
            for (size_t i = 0; i < expr->as.map.count; i++) {
                Value entry_val;
                FlowResult res = eval_expr(expr->as.map.values[i], env, &entry_val);
                if (res.type != FLOW_NORMAL) {
                    value_release(map_val);
                    return res;
                }
                ValMap *m = map_val.as.map;
                if (m->count >= m->capacity) {
                    m->capacity = m->capacity == 0 ? 8 : m->capacity * 2;
                    m->entries = realloc(m->entries, sizeof(ValMapEntry) * m->capacity);
                }
                m->entries[m->count].key = safe_strdup(expr->as.map.keys[i]);
                m->entries[m->count].value = entry_val;
                m->count++;
            }
            *out_val = map_val;
            return flow_normal();
        }
        case EXPR_ITEM_ACCESS: {
            Value col;
            FlowResult res = eval_expr(expr->as.item_access.list, env, &col);
            if (res.type != FLOW_NORMAL) return res;
            if (col.type != VAL_LIST) {
                value_release(col);
                return flow_error(expr->line, expr->column, "Item access expects list.");
            }
            Value idx;
            res = eval_expr(expr->as.item_access.index, env, &idx);
            if (res.type != FLOW_NORMAL) {
                value_release(col);
                return res;
            }
            if (idx.type != VAL_NUMBER) {
                value_release(col); value_release(idx);
                return flow_error(expr->line, expr->column, "List index must be a number.");
            }
            long long index = (long long)idx.as.number;
            value_release(idx);

            ValList *l = col.as.list;
            if (index < 0 || (size_t)index >= l->count) {
                value_release(col);
                return flow_error(expr->line, expr->column, "List index out of range: got %lld but size is %zu.", index, l->count);
            }
            *out_val = l->items[index];
            value_retain(*out_val);
            value_release(col);
            return flow_normal();
        }
        case EXPR_FIELD_ACCESS: {
            Value col;
            FlowResult res = eval_expr(expr->as.field_access.map, env, &col);
            if (res.type != FLOW_NORMAL) return res;
            if (col.type != VAL_MAP) {
                value_release(col);
                return flow_error(expr->line, expr->column, "Field access expects a map.");
            }
            Value field_val;
            res = eval_expr(expr->as.field_access.field, env, &field_val);
            if (res.type != FLOW_NORMAL) {
                value_release(col);
                return res;
            }
            if (field_val.type != VAL_STRING) {
                value_release(col); value_release(field_val);
                return flow_error(expr->line, expr->column, "Map field key must be a string.");
            }
            const char *key = field_val.as.string->chars;

            ValMap *m = col.as.map;
            bool found = false;
            for (size_t i = 0; i < m->count; i++) {
                if (strcmp(m->entries[i].key, key) == 0) {
                    *out_val = m->entries[i].value;
                    value_retain(*out_val);
                    found = true;
                    break;
                }
            }
            value_release(field_val);
            value_release(col);

            if (!found) {
                return flow_error(expr->line, expr->column, "Map key '%s' not found.", key);
            }
            return flow_normal();
        }
        case EXPR_CALL: {
            Value callee;
            FlowResult res = eval_expr(expr->as.call.callee, env, &callee);
            if (res.type != FLOW_NORMAL) return res;

            if (callee.type != VAL_TASK && callee.type != VAL_NATIVE) {
                value_release(callee);
                return flow_error(expr->line, expr->column, "Value is not callable.");
            }

            // Evaluate arguments
            Value *args = malloc(sizeof(Value) * expr->as.call.count);
            for (size_t i = 0; i < expr->as.call.count; i++) {
                res = eval_expr(expr->as.call.args[i], env, &args[i]);
                if (res.type != FLOW_NORMAL) {
                    value_release(callee);
                    for (size_t j = 0; j < i; j++) value_release(args[j]);
                    free(args);
                    return res;
                }
            }

            if (callee.type == VAL_TASK) {
                ValTask *task = callee.as.task;
                if (expr->as.call.count != task->param_count) {
                    value_release(callee);
                    for (size_t i = 0; i < expr->as.call.count; i++) value_release(args[i]);
                    free(args);
                    return flow_error(expr->line, expr->column, "Expected %zu arguments but got %zu.", task->param_count, expr->as.call.count);
                }

                // Create environment for call using closure environment
                Env *call_env = env_new(task->closure);
                for (size_t i = 0; i < task->param_count; i++) {
                    env_define(call_env, task->params[i], args[i]);
                }

                Block body;
                body.statements = task->body;
                body.count = task->body_count;

                FlowResult call_res = interpret(body, call_env);
                env_release(call_env);

                // Release args (they were retained during eval, and call_env also retained them. But call_env is released now).
                for (size_t i = 0; i < expr->as.call.count; i++) value_release(args[i]);
                free(args);
                value_release(callee);

                if (call_res.type == FLOW_SEND) {
                    *out_val = call_res.value; // It is already retained in send statement
                    // Convert FLOW_SEND to FLOW_NORMAL for expression result
                    call_res.type = FLOW_NORMAL;
                    call_res.value = value_new_empty(); // Clear so it isn't released twice
                    return call_res;
                }

                if (call_res.type == FLOW_LEAVE || call_res.type == FLOW_SKIP) {
                    FlowResult err = flow_error(call_res.line, call_res.column, "leave or skip outside loop.");
                    return err;
                }

                if (call_res.type == FLOW_ERROR) {
                    return call_res;
                }

                *out_val = value_new_empty();
                return flow_normal();
            } else {
                // Native task
                bool err = false;
                char *err_msg = NULL;
                Value ret_val = callee.as.native(expr->as.call.count, args, &err, &err_msg);

                // Release args
                for (size_t i = 0; i < expr->as.call.count; i++) value_release(args[i]);
                free(args);
                value_release(callee);

                if (err) {
                    FlowResult flow_err = flow_error(expr->line, expr->column, "%s", err_msg);
                    free(err_msg);
                    return flow_err;
                }
                *out_val = ret_val;
                return flow_normal();
            }
        }
    }

    return flow_error(expr->line, expr->column, "Unknown expression kind.");
}

// Statement execution
static FlowResult exec_stmt(Stmt *stmt, Env *env) {
    if (!stmt) return flow_normal();

    switch (stmt->kind) {
        case STMT_MAKE: {
            Value val;
            FlowResult res = eval_expr(stmt->as.make.initializer, env, &val);
            if (res.type != FLOW_NORMAL) return res;
            if (!env_define(env, stmt->as.make.name, val)) {
                value_release(val);
                return flow_error(stmt->line, stmt->column, "Variable '%s' is already defined in this scope.", stmt->as.make.name);
            }
            value_release(val);
            return flow_normal();
        }
        case STMT_KEEP: {
            Value val;
            FlowResult res = eval_expr(stmt->as.keep.initializer, env, &val);
            if (res.type != FLOW_NORMAL) return res;
            if (!env_define_const(env, stmt->as.keep.name, val)) {
                value_release(val);
                return flow_error(stmt->line, stmt->column, "Constant '%s' is already defined in this scope.", stmt->as.keep.name);
            }
            value_release(val);
            return flow_normal();
        }
        case STMT_ASSIGN: {
            Value val;
            FlowResult res = eval_expr(stmt->as.assign.value, env, &val);
            if (res.type != FLOW_NORMAL) return res;
            if (!env_assign(env, stmt->as.assign.name, val)) {
                value_release(val);
                return flow_error(stmt->line, stmt->column, "Cannot reassign to '%s' (either constant or undefined variable).", stmt->as.assign.name);
            }
            value_release(val);
            return flow_normal();
        }
        case STMT_SAY: {
            Value val;
            FlowResult res = eval_expr(stmt->as.say.expression, env, &val);
            if (res.type != FLOW_NORMAL) return res;
            value_say(val);
            value_release(val);
            return flow_normal();
        }
        case STMT_SEND: {
            Value val;
            FlowResult res = eval_expr(stmt->as.send.expression, env, &val);
            if (res.type != FLOW_NORMAL) return res;
            FlowResult send_res;
            send_res.type = FLOW_SEND;
            send_res.value = val; // ownership transferred to send result
            send_res.error_msg = NULL;
            send_res.line = stmt->line;
            send_res.column = stmt->column;
            return send_res;
        }
        case STMT_LEAVE: {
            FlowResult leave_res;
            leave_res.type = FLOW_LEAVE;
            leave_res.value = value_new_empty();
            leave_res.error_msg = NULL;
            leave_res.line = stmt->line;
            leave_res.column = stmt->column;
            return leave_res;
        }
        case STMT_SKIP: {
            FlowResult skip_res;
            skip_res.type = FLOW_SKIP;
            skip_res.value = value_new_empty();
            skip_res.error_msg = NULL;
            skip_res.line = stmt->line;
            skip_res.column = stmt->column;
            return skip_res;
        }
        case STMT_TASK: {
            // Define task
            Value task_val = value_new_task(
                stmt->as.task.params, stmt->as.task.param_count,
                stmt->as.task.body.statements, stmt->as.task.body.count,
                env
            );
            if (!env_define(env, stmt->as.task.name, task_val)) {
                value_release(task_val);
                return flow_error(stmt->line, stmt->column, "Task '%s' redefines existing name in this scope.", stmt->as.task.name);
            }
            value_release(task_val);
            return flow_normal();
        }
        case STMT_WHEN: {
            for (size_t i = 0; i < stmt->as.when.branch_count; i++) {
                Value cond;
                FlowResult res = eval_expr(stmt->as.when.branches[i].condition, env, &cond);
                if (res.type != FLOW_NORMAL) return res;
                bool is_true = value_truthy(cond);
                value_release(cond);

                if (is_true) {
                    Env *branch_env = env_new(env);
                    FlowResult branch_res = interpret(stmt->as.when.branches[i].block, branch_env);
                    env_release(branch_env);
                    return branch_res;
                }
            }

            if (stmt->as.when.otherwise_branch.count > 0) {
                Env *branch_env = env_new(env);
                FlowResult branch_res = interpret(stmt->as.when.otherwise_branch, branch_env);
                env_release(branch_env);
                return branch_res;
            }

            return flow_normal();
        }
        case STMT_REPEAT_COUNT: {
            Value count_val;
            FlowResult res = eval_expr(stmt->as.repeat_count.count_expr, env, &count_val);
            if (res.type != FLOW_NORMAL) return res;
            if (count_val.type != VAL_NUMBER) {
                value_release(count_val);
                return flow_error(stmt->line, stmt->column, "Repeat count must be a number.");
            }
            double c = count_val.as.number;
            value_release(count_val);

            for (double i = 0; i < c; i++) {
                Env *loop_env = env_new(env);
                FlowResult loop_res = interpret(stmt->as.repeat_count.body, loop_env);
                env_release(loop_env);

                if (loop_res.type == FLOW_LEAVE) break;
                if (loop_res.type == FLOW_SKIP) continue;
                if (loop_res.type != FLOW_NORMAL) return loop_res;
            }
            return flow_normal();
        }
        case STMT_REPEAT_RANGE: {
            Value start_val, end_val;
            FlowResult res = eval_expr(stmt->as.repeat_range.start, env, &start_val);
            if (res.type != FLOW_NORMAL) return res;
            res = eval_expr(stmt->as.repeat_range.end, env, &end_val);
            if (res.type != FLOW_NORMAL) {
                value_release(start_val);
                return res;
            }
            if (start_val.type != VAL_NUMBER || end_val.type != VAL_NUMBER) {
                value_release(start_val); value_release(end_val);
                return flow_error(stmt->line, stmt->column, "Range bounds must be numbers.");
            }
            double start = start_val.as.number;
            double end = end_val.as.number;
            value_release(start_val); value_release(end_val);

            if (start <= end) {
                for (double i = start; i <= end; i++) {
                    Env *loop_env = env_new(env);
                    env_define(loop_env, stmt->as.repeat_range.var_name, value_new_number(i));
                    FlowResult loop_res = interpret(stmt->as.repeat_range.body, loop_env);
                    env_release(loop_env);

                    if (loop_res.type == FLOW_LEAVE) break;
                    if (loop_res.type == FLOW_SKIP) continue;
                    if (loop_res.type != FLOW_NORMAL) return loop_res;
                }
            } else {
                for (double i = start; i >= end; i--) {
                    Env *loop_env = env_new(env);
                    env_define(loop_env, stmt->as.repeat_range.var_name, value_new_number(i));
                    FlowResult loop_res = interpret(stmt->as.repeat_range.body, loop_env);
                    env_release(loop_env);

                    if (loop_res.type == FLOW_LEAVE) break;
                    if (loop_res.type == FLOW_SKIP) continue;
                    if (loop_res.type != FLOW_NORMAL) return loop_res;
                }
            }
            return flow_normal();
        }
        case STMT_REPEAT_EACH: {
            Value col;
            FlowResult res = eval_expr(stmt->as.repeat_each.collection, env, &col);
            if (res.type != FLOW_NORMAL) return res;
            if (col.type != VAL_LIST) {
                value_release(col);
                return flow_error(stmt->line, stmt->column, "repeat each expects list.");
            }
            ValList *l = col.as.list;
            for (size_t i = 0; i < l->count; i++) {
                Env *loop_env = env_new(env);
                env_define(loop_env, stmt->as.repeat_each.var_name, l->items[i]);
                FlowResult loop_res = interpret(stmt->as.repeat_each.body, loop_env);
                env_release(loop_env);

                if (loop_res.type == FLOW_LEAVE) break;
                if (loop_res.type == FLOW_SKIP) continue;
                if (loop_res.type != FLOW_NORMAL) {
                    value_release(col);
                    return loop_res;
                }
            }
            value_release(col);
            return flow_normal();
        }
        case STMT_DURING: {
            for (;;) {
                Value cond;
                FlowResult res = eval_expr(stmt->as.during.condition, env, &cond);
                if (res.type != FLOW_NORMAL) return res;
                bool is_true = value_truthy(cond);
                value_release(cond);

                if (!is_true) break;

                Env *loop_env = env_new(env);
                FlowResult loop_res = interpret(stmt->as.during.body, loop_env);
                env_release(loop_env);

                if (loop_res.type == FLOW_LEAVE) break;
                if (loop_res.type == FLOW_SKIP) continue;
                if (loop_res.type != FLOW_NORMAL) return loop_res;
            }
            return flow_normal();
        }
        case STMT_ATTEMPT: {
            Env *try_env = env_new(env);
            FlowResult res = interpret(stmt->as.attempt.try_body, try_env);
            env_release(try_env);

            if (res.type == FLOW_ERROR) {
                Value err_str = value_new_string(res.error_msg);
                free(res.error_msg); // Free old error message

                Env *rescue_env = env_new(env);
                env_define(rescue_env, stmt->as.attempt.problem_var, err_str);
                value_release(err_str);

                FlowResult rescue_res = interpret(stmt->as.attempt.rescue_body, rescue_env);
                env_release(rescue_env);
                return rescue_res;
            }

            return res;
        }
        case STMT_CHOOSE: {
            Value val;
            FlowResult res = eval_expr(stmt->as.choose.expr, env, &val);
            if (res.type != FLOW_NORMAL) return res;

            bool matched = false;
            for (size_t i = 0; i < stmt->as.choose.case_count; i++) {
                Value cval;
                res = eval_expr(stmt->as.choose.cases[i].expr, env, &cval);
                if (res.type != FLOW_NORMAL) {
                    value_release(val);
                    return res;
                }
                bool is_same = value_same_as(val, cval);
                value_release(cval);

                if (is_same) {
                    matched = true;
                    value_release(val);
                    Env *case_env = env_new(env);
                    FlowResult case_res = interpret(stmt->as.choose.cases[i].block, case_env);
                    env_release(case_env);
                    return case_res;
                }
            }

            value_release(val);
            if (!matched && stmt->as.choose.otherwise_branch.count > 0) {
                Env *case_env = env_new(env);
                FlowResult case_res = interpret(stmt->as.choose.otherwise_branch, case_env);
                env_release(case_env);
                return case_res;
            }
            return flow_normal();
        }
        case STMT_PUT: {
            Value val, col;
            FlowResult res = eval_expr(stmt->as.put.value, env, &val);
            if (res.type != FLOW_NORMAL) return res;
            res = eval_expr(stmt->as.put.list, env, &col);
            if (res.type != FLOW_NORMAL) {
                value_release(val);
                return res;
            }
            if (col.type != VAL_LIST) {
                value_release(val); value_release(col);
                return flow_error(stmt->line, stmt->column, "put statement expects a list.");
            }
            ValList *l = col.as.list;
            if (l->count >= l->capacity) {
                l->capacity = l->capacity == 0 ? 8 : l->capacity * 2;
                l->items = realloc(l->items, sizeof(Value) * l->capacity);
            }
            l->items[l->count++] = val; // Transfers reference (already retained by eval_expr)
            value_release(col);
            return flow_normal();
        }
        case STMT_SET_FIELD: {
            Value col, field_val, val;
            FlowResult res = eval_expr(stmt->as.set_field.map, env, &col);
            if (res.type != FLOW_NORMAL) return res;
            res = eval_expr(stmt->as.set_field.field, env, &field_val);
            if (res.type != FLOW_NORMAL) {
                value_release(col);
                return res;
            }
            res = eval_expr(stmt->as.set_field.value, env, &val);
            if (res.type != FLOW_NORMAL) {
                value_release(col); value_release(field_val);
                return res;
            }
            if (col.type != VAL_MAP) {
                value_release(col); value_release(field_val); value_release(val);
                return flow_error(stmt->line, stmt->column, "set statement expects a map.");
            }
            if (field_val.type != VAL_STRING) {
                value_release(col); value_release(field_val); value_release(val);
                return flow_error(stmt->line, stmt->column, "map field key must be a string.");
            }

            ValMap *m = col.as.map;
            const char *key = field_val.as.string->chars;
            bool found = false;
            for (size_t i = 0; i < m->count; i++) {
                if (strcmp(m->entries[i].key, key) == 0) {
                    value_release(m->entries[i].value);
                    m->entries[i].value = val; // transfers reference (already retained)
                    found = true;
                    break;
                }
            }

            if (!found) {
                if (m->count >= m->capacity) {
                    m->capacity = m->capacity == 0 ? 8 : m->capacity * 2;
                    m->entries = realloc(m->entries, sizeof(ValMapEntry) * m->capacity);
                }
                m->entries[m->count].key = safe_strdup(key);
                m->entries[m->count].value = val; // transfers reference
                m->count++;
            }

            value_release(col); value_release(field_val);
            return flow_normal();
        }
        case STMT_HI_HTMVSS: {
            printf("<!DOCTYPE html>\n<html>\n<head>\n<script src=\"https://cdn.tailwindcss.com\"></script>\n</head>\n<body class=\"bg-slate-900 text-white font-sans flex flex-col items-center justify-center min-h-screen\">\n");
            return flow_normal();
        }
        case STMT_BYE_HTMVSS: {
            printf("</body>\n</html>\n");
            return flow_normal();
        }
        case STMT_EXPR: {
            Value val;
            FlowResult res = eval_expr(stmt->as.expr_stmt.expression, env, &val);
            if (res.type != FLOW_NORMAL) return res;
            value_release(val);
            return flow_normal();
        }
        case STMT_GRAB: {
            // Module Import
            char filepath[256];
            snprintf(filepath, sizeof(filepath), "%s.vss", stmt->as.grab.module_name);
            FILE *f = fopen(filepath, "rb");
            if (!f) {
                // Try examples/ directory or same dir
                snprintf(filepath, sizeof(filepath), "examples/%s.vss", stmt->as.grab.module_name);
                f = fopen(filepath, "rb");
            }
            if (!f) {
                return flow_error(stmt->line, stmt->column, "Grab module '%s' not found.", stmt->as.grab.module_name);
            }
            fclose(f);

            // Read the file content
            FILE *file = fopen(filepath, "rb");
            fseek(file, 0, SEEK_END);
            long size = ftell(file);
            rewind(file);
            char *source = malloc(size + 1);
            size_t read_bytes = fread(source, 1, size, file);
            fclose(file);
            source[read_bytes] = '\0';

            Lexer mod_lexer;
            lexer_init(&mod_lexer, source);
            Parser mod_parser;
            parser_init(&mod_parser, &mod_lexer);
            Block mod_ast = parse_program(&mod_parser);
            free(source);

            if (mod_parser.had_error) {
                block_free(mod_ast);
                return flow_error(stmt->line, stmt->column, "Syntax error in module '%s'.", stmt->as.grab.module_name);
            }

            Env *mod_env = env_new(NULL);
            register_builtins(mod_env);
            
            FlowResult mod_res = interpret(mod_ast, mod_env);
            block_free(mod_ast);

            if (mod_res.type == FLOW_ERROR) {
                env_release(mod_env);
                return mod_res;
            }

            // Copy all bindings from module_env to current env
            for (size_t i = 0; i < mod_env->count; i++) {
                // Skip built-ins starting with double underscore
                if (strncmp(mod_env->items[i].name, "__", 2) == 0) continue;
                
                if (mod_env->items[i].is_constant) {
                    env_define_const(env, mod_env->items[i].name, mod_env->items[i].value);
                } else {
                    env_define(env, mod_env->items[i].name, mod_env->items[i].value);
                }
            }

            env_release(mod_env);
            return flow_normal();
        }
    }

    return flow_error(stmt->line, stmt->column, "Unknown statement kind.");
}

FlowResult interpret(Block block, Env *env) {
    for (size_t i = 0; i < block.count; i++) {
        FlowResult res = exec_stmt(block.statements[i], env);
        if (res.type != FLOW_NORMAL) {
            return res;
        }
    }
    return flow_normal();
}
