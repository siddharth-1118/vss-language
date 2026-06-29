#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "compiler.h"
#include "chunk.h"

// Scope & Local tracking
typedef struct {
    char *name;
    int depth;
    bool is_constant;
    bool is_captured;
} Local;

typedef struct {
    int index;
    bool is_local;
} UpvalueInfo;

typedef enum {
    TYPE_MAIN,
    TYPE_TASK
} FunctionType;

typedef struct Compiler Compiler;
struct Compiler {
    Compiler *enclosing;
    VSS_ObjFunction *function;
    FunctionType type;
    
    Local locals[256];
    int local_count;
    UpvalueInfo upvalues[256];
    
    int scope_depth;
};

// Loop tracking
typedef struct LoopCompiler LoopCompiler;
struct LoopCompiler {
    LoopCompiler *enclosing;
    bool is_during;
    int start_offset;
    int *leave_jumps;
    int leave_count;
    int leave_capacity;
    int *skip_jumps;
    int skip_count;
    int skip_capacity;
};

// Global compile state
static Compiler *current_compiler = NULL;
static LoopCompiler *current_loop = NULL;

static void compiler_init(Compiler *compiler, const char *name, FunctionType type) {
    compiler->enclosing = current_compiler;
    compiler->function = vss_function_new(name, 0);
    compiler->type = type;
    compiler->local_count = 0;
    compiler->scope_depth = 0;
    
    current_compiler = compiler;
    
    // Claim local slot 0 for function context/self (or closure)
    Local *local = &compiler->locals[compiler->local_count++];
    local->depth = 0;
    local->is_constant = true;
    local->is_captured = false;
    if (type == TYPE_MAIN) {
        local->name = malloc(1);
        local->name[0] = '\0';
    } else {
        local->name = malloc(5);
        strcpy(local->name, "self");
    }
}

static VSS_ObjFunction *compiler_end(void) {
    VSS_ObjFunction *func = current_compiler->function;
    
    // Free the local 0 name
    if (current_compiler->local_count > 0) {
        free(current_compiler->locals[0].name);
    }
    
    current_compiler = current_compiler->enclosing;
    return func;
}

static VSS_Chunk *current_chunk(void) {
    return &current_compiler->function->chunk;
}

static void emit_byte(uint8_t byte, int line) {
    vss_chunk_write(current_chunk(), byte, line);
}

static void emit_bytes(uint8_t byte1, uint8_t byte2, int line) {
    emit_byte(byte1, line);
    emit_byte(byte2, line);
}

static void emit_return(int line) {
    emit_byte(VSS_OP_EMPTY, line);
    emit_byte(VSS_OP_RETURN, line);
}

static int make_constant(VSS_Value value, int line) {
    (void)line;
    int constant = vss_chunk_add_constant(current_chunk(), value);
    return constant;
}

static void emit_constant(VSS_Value value, int line) {
    int constant = make_constant(value, line);
    if (constant <= 255) {
        emit_bytes(VSS_OP_CONSTANT, (uint8_t)constant, line);
    } else {
        emit_byte(VSS_OP_CONSTANT_LONG, line);
        emit_byte(constant & 0xff, line);
        emit_byte((constant >> 8) & 0xff, line);
        emit_byte((constant >> 16) & 0xff, line);
    }
}

static int emit_jump(uint8_t instruction, int line) {
    emit_byte(instruction, line);
    emit_byte(0xff, line);
    emit_byte(0xff, line);
    return current_chunk()->count - 2;
}

static void patch_jump(int offset) {
    int jump = current_chunk()->count - offset - 2;
    if (jump > 65535) {
        fprintf(stderr, "Too much code to jump over.\n");
        exit(1);
    }
    current_chunk()->code[offset] = (jump >> 8) & 0xff;
    current_chunk()->code[offset + 1] = jump & 0xff;
}

static void emit_loop(int loop_start, int line) {
    emit_byte(VSS_OP_LOOP, line);
    
    int offset = current_chunk()->count + 2 - loop_start;
    if (offset > 65535) {
        fprintf(stderr, "Loop body too large.\n");
        exit(1);
    }
    emit_byte((offset >> 8) & 0xff, line);
    emit_byte(offset & 0xff, line);
}

static void begin_scope(void) {
    current_compiler->scope_depth++;
}

static void end_scope(int line) {
    current_compiler->scope_depth--;
    
    while (current_compiler->local_count > 0 &&
           current_compiler->locals[current_compiler->local_count - 1].depth > current_compiler->scope_depth) {
        emit_byte(VSS_OP_POP, line);
        free(current_compiler->locals[current_compiler->local_count - 1].name);
        current_compiler->local_count--;
    }
}

static void add_local(const char *name, bool is_const, int line) {
    (void)line;
    if (current_compiler->local_count >= 256) {
        fprintf(stderr, "Too many local variables in function.\n");
        return;
    }
    
    for (int i = current_compiler->local_count - 1; i >= 0; i--) {
        Local *local = &current_compiler->locals[i];
        if (local->depth < current_compiler->scope_depth) break;
        if (strcmp(local->name, name) == 0) {
            fprintf(stderr, "Variable '%s' already defined in this scope.\n", name);
            return;
        }
    }
    
    Local *local = &current_compiler->locals[current_compiler->local_count++];
    local->name = malloc(strlen(name) + 1);
    strcpy(local->name, name);
    local->depth = current_compiler->scope_depth;
    local->is_constant = is_const;
    local->is_captured = false;
}

static int resolve_local(Compiler *compiler, const char *name) {
    for (int i = compiler->local_count - 1; i >= 0; i--) {
        Local *local = &compiler->locals[i];
        if (strcmp(local->name, name) == 0) {
            return i;
        }
    }
    return -1;
}

static int add_upvalue(Compiler *compiler, uint8_t index, bool is_local) {
    int count = compiler->function->upvalue_count;
    for (int i = 0; i < count; i++) {
        UpvalueInfo *upvalue = &compiler->upvalues[i];
        if (upvalue->index == index && upvalue->is_local == is_local) {
            return i;
        }
    }
    
    if (count >= 256) {
        fprintf(stderr, "Too many closure variables in function.\n");
        return 0;
    }
    
    compiler->upvalues[count].is_local = is_local;
    compiler->upvalues[count].index = index;
    compiler->function->upvalue_count++;
    return count;
}

static int resolve_upvalue(Compiler *compiler, const char *name) {
    if (compiler->enclosing == NULL) return -1;
    
    int local = resolve_local(compiler->enclosing, name);
    if (local != -1) {
        compiler->enclosing->locals[local].is_captured = true;
        return add_upvalue(compiler, (uint8_t)local, true);
    }
    
    int upvalue = resolve_upvalue(compiler->enclosing, name);
    if (upvalue != -1) {
        return add_upvalue(compiler, (uint8_t)upvalue, false);
    }
    
    return -1;
}

// Forward declarations
static void compile_stmt(VSS_Stmt *stmt);
static void compile_expr(VSS_Expr *expr);
static void compile_block(VSS_Block block, bool new_scope, int line);

static void compile_expr(VSS_Expr *expr) {
    if (!expr) {
        emit_byte(VSS_OP_EMPTY, 0);
        return;
    }
    
    switch (expr->kind) {
        case VSS_EXPR_NUMBER:
            emit_constant(vss_value_new_number(expr->as.number), expr->line);
            break;
        case VSS_EXPR_STRING:
            emit_constant(vss_value_new_string(expr->as.string), expr->line);
            break;
        case VSS_EXPR_BOOL:
            emit_byte(expr->as.boolean ? VSS_OP_TRUE : VSS_OP_FALSE, expr->line);
            break;
        case VSS_EXPR_EMPTY:
            emit_byte(VSS_OP_EMPTY, expr->line);
            break;
        case VSS_EXPR_NAME: {
            int arg = resolve_local(current_compiler, expr->as.name);
            if (arg != -1) {
                emit_bytes(VSS_OP_GET_LOCAL, (uint8_t)arg, expr->line);
            } else if ((arg = resolve_upvalue(current_compiler, expr->as.name)) != -1) {
                emit_bytes(VSS_OP_GET_UPVALUE, (uint8_t)arg, expr->line);
            } else {
                int name_const = make_constant(vss_value_new_string(expr->as.name), expr->line);
                emit_bytes(VSS_OP_GET_GLOBAL, (uint8_t)name_const, expr->line);
            }
            break;
        }
        case VSS_EXPR_UNARY: {
            compile_expr(expr->as.unary.operand);
            if (expr->as.unary.op == VSS_TOKEN_MINUS) {
                emit_byte(VSS_OP_NEGATE, expr->line);
            } else if (expr->as.unary.op == VSS_TOKEN_NOT) {
                emit_byte(VSS_OP_NOT, expr->line);
            }
            break;
        }
        case VSS_EXPR_BINARY: {
            if (expr->as.binary.op == VSS_TOKEN_AND) {
                compile_expr(expr->as.binary.left);
                int end_jump = emit_jump(VSS_OP_JUMP_IF_FALSE, expr->line);
                emit_byte(VSS_OP_POP, expr->line);
                compile_expr(expr->as.binary.right);
                patch_jump(end_jump);
                break;
            }
            if (expr->as.binary.op == VSS_TOKEN_OR) {
                compile_expr(expr->as.binary.left);
                int next_branch = emit_jump(VSS_OP_JUMP_IF_FALSE, expr->line);
                int end_jump = emit_jump(VSS_OP_JUMP, expr->line);
                patch_jump(next_branch);
                emit_byte(VSS_OP_POP, expr->line);
                compile_expr(expr->as.binary.right);
                patch_jump(end_jump);
                break;
            }
            
            compile_expr(expr->as.binary.left);
            compile_expr(expr->as.binary.right);
            switch (expr->as.binary.op) {
                case VSS_TOKEN_PLUS:         emit_byte(VSS_OP_ADD, expr->line); break;
                case VSS_TOKEN_MINUS:        emit_byte(VSS_OP_SUB, expr->line); break;
                case VSS_TOKEN_STAR:         emit_byte(VSS_OP_MUL, expr->line); break;
                case VSS_TOKEN_SLASH:        emit_byte(VSS_OP_DIV, expr->line); break;
                case VSS_TOKEN_PERCENT:      emit_byte(VSS_OP_MOD, expr->line); break;
                case VSS_TOKEN_ABOVE:        emit_byte(VSS_OP_ABOVE, expr->line); break;
                case VSS_TOKEN_BELOW:        emit_byte(VSS_OP_BELOW, expr->line); break;
                case VSS_TOKEN_AT_LEAST:     emit_byte(VSS_OP_AT_LEAST, expr->line); break;
                case VSS_TOKEN_AT_MOST:      emit_byte(VSS_OP_AT_MOST, expr->line); break;
                case VSS_TOKEN_SAME_AS:      emit_byte(VSS_OP_SAME_AS, expr->line); break;
                case VSS_TOKEN_NOT_SAME_AS:  emit_byte(VSS_OP_NOT_SAME_AS, expr->line); break;
                default: break;
            }
            break;
        }
        case VSS_EXPR_LIST: {
            for (size_t i = 0; i < expr->as.list.count; i++) {
                compile_expr(expr->as.list.elements[i]);
            }
            emit_bytes(VSS_OP_BUILD_LIST, (uint8_t)expr->as.list.count, expr->line);
            break;
        }
        case VSS_EXPR_MAP: {
            for (size_t i = 0; i < expr->as.map.count; i++) {
                emit_constant(vss_value_new_string(expr->as.map.keys[i]), expr->line);
                compile_expr(expr->as.map.values[i]);
            }
            emit_bytes(VSS_OP_BUILD_MAP, (uint8_t)expr->as.map.count, expr->line);
            break;
        }
        case VSS_EXPR_ITEM_ACCESS: {
            compile_expr(expr->as.item_access.list);
            compile_expr(expr->as.item_access.index);
            emit_byte(VSS_OP_GET_ITEM, expr->line);
            break;
        }
        case VSS_EXPR_FIELD_ACCESS: {
            compile_expr(expr->as.field_access.map);
            compile_expr(expr->as.field_access.field);
            emit_byte(VSS_OP_GET_FIELD, expr->line);
            break;
        }
        case VSS_EXPR_CALL: {
            compile_expr(expr->as.call.callee);
            for (size_t i = 0; i < expr->as.call.count; i++) {
                compile_expr(expr->as.call.args[i]);
            }
            emit_bytes(VSS_OP_CALL, (uint8_t)expr->as.call.count, expr->line);
            break;
        }
    }
}

static void compile_block(VSS_Block block, bool new_scope, int line) {
    if (new_scope) begin_scope();
    for (size_t i = 0; i < block.count; i++) {
        compile_stmt(block.statements[i]);
    }
    if (new_scope) end_scope(line);
}

static void compile_stmt(VSS_Stmt *stmt) {
    if (!stmt) return;
    
    switch (stmt->kind) {
        case VSS_STMT_MAKE:
        case VSS_STMT_KEEP: {
            bool is_const = (stmt->kind == VSS_STMT_KEEP);
            compile_expr(stmt->as.make.initializer);
            
            if (current_compiler->scope_depth == 0) {
                int name_const = make_constant(vss_value_new_string(stmt->as.make.name), stmt->line);
                emit_byte(VSS_OP_DEFINE_GLOBAL, stmt->line);
                emit_byte((uint8_t)name_const, stmt->line);
                emit_byte(is_const ? 1 : 0, stmt->line);
            } else {
                add_local(stmt->as.make.name, is_const, stmt->line);
            }
            break;
        }
        case VSS_STMT_ASSIGN: {
            compile_expr(stmt->as.assign.value);
            int arg = resolve_local(current_compiler, stmt->as.assign.name);
            if (arg != -1) {
                emit_bytes(VSS_OP_SET_LOCAL, (uint8_t)arg, stmt->line);
            } else if ((arg = resolve_upvalue(current_compiler, stmt->as.assign.name)) != -1) {
                emit_bytes(VSS_OP_SET_UPVALUE, (uint8_t)arg, stmt->line);
            } else {
                int name_const = make_constant(vss_value_new_string(stmt->as.assign.name), stmt->line);
                emit_bytes(VSS_OP_SET_GLOBAL, (uint8_t)name_const, stmt->line);
            }
            emit_byte(VSS_OP_POP, stmt->line);
            break;
        }
        case VSS_STMT_SAY: {
            compile_expr(stmt->as.say.expression);
            emit_byte(VSS_OP_SAY, stmt->line);
            break;
        }
        case VSS_STMT_SEND: {
            compile_expr(stmt->as.send.expression);
            emit_byte(VSS_OP_RETURN, stmt->line);
            break;
        }
        case VSS_STMT_WHEN: {
            int *end_jumps = malloc(sizeof(int) * stmt->as.when.branch_count);
            
            for (size_t i = 0; i < stmt->as.when.branch_count; i++) {
                compile_expr(stmt->as.when.branches[i].condition);
                int false_jump = emit_jump(VSS_OP_JUMP_IF_FALSE, stmt->line);
                emit_byte(VSS_OP_POP, stmt->line);
                
                compile_block(stmt->as.when.branches[i].block, true, stmt->line);
                end_jumps[i] = emit_jump(VSS_OP_JUMP, stmt->line);
                
                patch_jump(false_jump);
                emit_byte(VSS_OP_POP, stmt->line);
            }
            
            if (stmt->as.when.otherwise_branch.count > 0) {
                compile_block(stmt->as.when.otherwise_branch, true, stmt->line);
            }
            
            for (size_t i = 0; i < stmt->as.when.branch_count; i++) {
                patch_jump(end_jumps[i]);
            }
            free(end_jumps);
            break;
        }
        case VSS_STMT_REPEAT_COUNT: {
            compile_expr(stmt->as.repeat_count.count_expr);
            
            begin_scope();
            add_local("$limit", true, stmt->line);
            
            emit_constant(vss_value_new_number(0.0), stmt->line);
            add_local("$counter", false, stmt->line);
            
            int loop_start = current_chunk()->count;
            int counter_slot = resolve_local(current_compiler, "$counter");
            int limit_slot = resolve_local(current_compiler, "$limit");
            
            emit_bytes(VSS_OP_GET_LOCAL, (uint8_t)counter_slot, stmt->line);
            emit_bytes(VSS_OP_GET_LOCAL, (uint8_t)limit_slot, stmt->line);
            emit_byte(VSS_OP_BELOW, stmt->line);
            
            int exit_jump = emit_jump(VSS_OP_JUMP_IF_FALSE, stmt->line);
            emit_byte(VSS_OP_POP, stmt->line);
            
            LoopCompiler loop;
            loop.enclosing = current_loop;
            loop.is_during = false;
            loop.start_offset = loop_start;
            loop.leave_jumps = NULL;
            loop.leave_count = 0;
            loop.leave_capacity = 0;
            loop.skip_jumps = NULL;
            loop.skip_count = 0;
            loop.skip_capacity = 0;
            current_loop = &loop;
            
            compile_block(stmt->as.repeat_count.body, true, stmt->line);
            
            for (int i = 0; i < loop.skip_count; i++) {
                patch_jump(loop.skip_jumps[i]);
            }
            free(loop.skip_jumps);
            
            emit_bytes(VSS_OP_GET_LOCAL, (uint8_t)counter_slot, stmt->line);
            emit_constant(vss_value_new_number(1.0), stmt->line);
            emit_byte(VSS_OP_ADD, stmt->line);
            emit_bytes(VSS_OP_SET_LOCAL, (uint8_t)counter_slot, stmt->line);
            emit_byte(VSS_OP_POP, stmt->line);
            
            emit_loop(loop_start, stmt->line);
            
            patch_jump(exit_jump);
            emit_byte(VSS_OP_POP, stmt->line);
            
            for (int i = 0; i < loop.leave_count; i++) {
                patch_jump(loop.leave_jumps[i]);
            }
            free(loop.leave_jumps);
            current_loop = loop.enclosing;
            
            end_scope(stmt->line);
            break;
        }
        case VSS_STMT_REPEAT_RANGE: {
            compile_expr(stmt->as.repeat_range.start);
            compile_expr(stmt->as.repeat_range.end);
            
            begin_scope();
            add_local("$start", true, stmt->line);
            add_local("$end", true, stmt->line);
            
            int start_slot = resolve_local(current_compiler, "$start");
            int end_slot = resolve_local(current_compiler, "$end");
            
            emit_bytes(VSS_OP_GET_LOCAL, (uint8_t)start_slot, stmt->line);
            emit_bytes(VSS_OP_GET_LOCAL, (uint8_t)end_slot, stmt->line);
            emit_byte(VSS_OP_AT_MOST, stmt->line);
            
            int false_branch = emit_jump(VSS_OP_JUMP_IF_FALSE, stmt->line);
            emit_byte(VSS_OP_POP, stmt->line);
            
            // Upward
            begin_scope();
            emit_bytes(VSS_OP_GET_LOCAL, (uint8_t)start_slot, stmt->line);
            add_local(stmt->as.repeat_range.var_name, false, stmt->line);
            
            int up_start = current_chunk()->count;
            int i_slot = resolve_local(current_compiler, stmt->as.repeat_range.var_name);
            emit_bytes(VSS_OP_GET_LOCAL, (uint8_t)i_slot, stmt->line);
            emit_bytes(VSS_OP_GET_LOCAL, (uint8_t)end_slot, stmt->line);
            emit_byte(VSS_OP_AT_MOST, stmt->line);
            
            int up_exit = emit_jump(VSS_OP_JUMP_IF_FALSE, stmt->line);
            emit_byte(VSS_OP_POP, stmt->line);
            
            LoopCompiler up_loop;
            up_loop.enclosing = current_loop;
            up_loop.is_during = false;
            up_loop.start_offset = up_start;
            up_loop.leave_jumps = NULL;
            up_loop.leave_count = 0;
            up_loop.leave_capacity = 0;
            up_loop.skip_jumps = NULL;
            up_loop.skip_count = 0;
            up_loop.skip_capacity = 0;
            current_loop = &up_loop;
            
            compile_block(stmt->as.repeat_range.body, true, stmt->line);
            
            for (int i = 0; i < up_loop.skip_count; i++) {
                patch_jump(up_loop.skip_jumps[i]);
            }
            free(up_loop.skip_jumps);
            
            emit_bytes(VSS_OP_GET_LOCAL, (uint8_t)i_slot, stmt->line);
            emit_constant(vss_value_new_number(1.0), stmt->line);
            emit_byte(VSS_OP_ADD, stmt->line);
            emit_bytes(VSS_OP_SET_LOCAL, (uint8_t)i_slot, stmt->line);
            emit_byte(VSS_OP_POP, stmt->line);
            
            emit_loop(up_start, stmt->line);
            
            patch_jump(up_exit);
            emit_byte(VSS_OP_POP, stmt->line);
            
            for (int k = 0; k < up_loop.leave_count; k++) {
                patch_jump(up_loop.leave_jumps[k]);
            }
            free(up_loop.leave_jumps);
            current_loop = up_loop.enclosing;
            
            end_scope(stmt->line);
            
            int end_jump = emit_jump(VSS_OP_JUMP, stmt->line);
            
            // Downward
            patch_jump(false_branch);
            emit_byte(VSS_OP_POP, stmt->line);
            
            begin_scope();
            emit_bytes(VSS_OP_GET_LOCAL, (uint8_t)start_slot, stmt->line);
            add_local(stmt->as.repeat_range.var_name, false, stmt->line);
            
            int down_start = current_chunk()->count;
            i_slot = resolve_local(current_compiler, stmt->as.repeat_range.var_name);
            emit_bytes(VSS_OP_GET_LOCAL, (uint8_t)i_slot, stmt->line);
            emit_bytes(VSS_OP_GET_LOCAL, (uint8_t)end_slot, stmt->line);
            emit_byte(VSS_OP_AT_LEAST, stmt->line);
            
            int down_exit = emit_jump(VSS_OP_JUMP_IF_FALSE, stmt->line);
            emit_byte(VSS_OP_POP, stmt->line);
            
            LoopCompiler down_loop;
            down_loop.enclosing = current_loop;
            down_loop.is_during = false;
            down_loop.start_offset = down_start;
            down_loop.leave_jumps = NULL;
            down_loop.leave_count = 0;
            down_loop.leave_capacity = 0;
            down_loop.skip_jumps = NULL;
            down_loop.skip_count = 0;
            down_loop.skip_capacity = 0;
            current_loop = &down_loop;
            
            compile_block(stmt->as.repeat_range.body, true, stmt->line);
            
            for (int i = 0; i < down_loop.skip_count; i++) {
                patch_jump(down_loop.skip_jumps[i]);
            }
            free(down_loop.skip_jumps);
            
            emit_bytes(VSS_OP_GET_LOCAL, (uint8_t)i_slot, stmt->line);
            emit_constant(vss_value_new_number(1.0), stmt->line);
            emit_byte(VSS_OP_SUB, stmt->line);
            emit_bytes(VSS_OP_SET_LOCAL, (uint8_t)i_slot, stmt->line);
            emit_byte(VSS_OP_POP, stmt->line);
            
            emit_loop(down_start, stmt->line);
            
            patch_jump(down_exit);
            emit_byte(VSS_OP_POP, stmt->line);
            
            for (int k = 0; k < down_loop.leave_count; k++) {
                patch_jump(down_loop.leave_jumps[k]);
            }
            free(down_loop.leave_jumps);
            current_loop = down_loop.enclosing;
            
            end_scope(stmt->line);
            
            patch_jump(end_jump);
            end_scope(stmt->line);
            break;
        }
        case VSS_STMT_REPEAT_EACH: {
            compile_expr(stmt->as.repeat_each.collection);
            
            begin_scope();
            add_local("$list", true, stmt->line);
            emit_constant(vss_value_new_number(0.0), stmt->line);
            add_local("$index", false, stmt->line);
            
            int loop_start = current_chunk()->count;
            int list_slot = resolve_local(current_compiler, "$list");
            int index_slot = resolve_local(current_compiler, "$index");
            
            emit_bytes(VSS_OP_GET_LOCAL, (uint8_t)index_slot, stmt->line);
            emit_bytes(VSS_OP_GET_LOCAL, (uint8_t)list_slot, stmt->line);
            emit_byte(VSS_OP_SIZE_OF, stmt->line);
            emit_byte(VSS_OP_BELOW, stmt->line);
            
            int exit_jump = emit_jump(VSS_OP_JUMP_IF_FALSE, stmt->line);
            emit_byte(VSS_OP_POP, stmt->line);
            
            begin_scope();
            emit_bytes(VSS_OP_GET_LOCAL, (uint8_t)list_slot, stmt->line);
            emit_bytes(VSS_OP_GET_LOCAL, (uint8_t)index_slot, stmt->line);
            emit_byte(VSS_OP_GET_ITEM, stmt->line);
            add_local(stmt->as.repeat_each.var_name, false, stmt->line);
            
            LoopCompiler loop;
            loop.enclosing = current_loop;
            loop.is_during = false;
            loop.start_offset = loop_start;
            loop.leave_jumps = NULL;
            loop.leave_count = 0;
            loop.leave_capacity = 0;
            loop.skip_jumps = NULL;
            loop.skip_count = 0;
            loop.skip_capacity = 0;
            current_loop = &loop;
            
            compile_block(stmt->as.repeat_each.body, false, stmt->line);
            
            end_scope(stmt->line);
            
            for (int i = 0; i < loop.skip_count; i++) {
                patch_jump(loop.skip_jumps[i]);
            }
            free(loop.skip_jumps);
            
            emit_bytes(VSS_OP_GET_LOCAL, (uint8_t)index_slot, stmt->line);
            emit_constant(vss_value_new_number(1.0), stmt->line);
            emit_byte(VSS_OP_ADD, stmt->line);
            emit_bytes(VSS_OP_SET_LOCAL, (uint8_t)index_slot, stmt->line);
            emit_byte(VSS_OP_POP, stmt->line);
            
            emit_loop(loop_start, stmt->line);
            
            patch_jump(exit_jump);
            emit_byte(VSS_OP_POP, stmt->line);
            
            for (int k = 0; k < loop.leave_count; k++) {
                patch_jump(loop.leave_jumps[k]);
            }
            free(loop.leave_jumps);
            current_loop = loop.enclosing;
            
            end_scope(stmt->line);
            break;
        }
        case VSS_STMT_DURING: {
            int loop_start = current_chunk()->count;
            compile_expr(stmt->as.during.condition);
            int exit_jump = emit_jump(VSS_OP_JUMP_IF_FALSE, stmt->line);
            emit_byte(VSS_OP_POP, stmt->line);
            
            LoopCompiler loop;
            loop.enclosing = current_loop;
            loop.is_during = true;
            loop.start_offset = loop_start;
            loop.leave_jumps = NULL;
            loop.leave_count = 0;
            loop.leave_capacity = 0;
            loop.skip_jumps = NULL;
            loop.skip_count = 0;
            loop.skip_capacity = 0;
            current_loop = &loop;
            
            compile_block(stmt->as.during.body, true, stmt->line);
            
            emit_loop(loop_start, stmt->line);
            
            patch_jump(exit_jump);
            emit_byte(VSS_OP_POP, stmt->line);
            
            for (int k = 0; k < loop.leave_count; k++) {
                patch_jump(loop.leave_jumps[k]);
            }
            free(loop.leave_jumps);
            current_loop = loop.enclosing;
            break;
        }
        case VSS_STMT_LEAVE: {
            if (current_loop == NULL) {
                fprintf(stderr, "Leave statement outside loop.\n");
                break;
            }
            int jump = emit_jump(VSS_OP_JUMP, stmt->line);
            
            if (current_loop->leave_count >= current_loop->leave_capacity) {
                current_loop->leave_capacity = current_loop->leave_capacity < 4 ? 4 : current_loop->leave_capacity * 2;
                current_loop->leave_jumps = realloc(current_loop->leave_jumps, current_loop->leave_capacity * sizeof(int));
            }
            current_loop->leave_jumps[current_loop->leave_count++] = jump;
            break;
        }
        case VSS_STMT_SKIP: {
            if (current_loop == NULL) {
                fprintf(stderr, "Skip statement outside loop.\n");
                break;
            }
            if (current_loop->is_during) {
                emit_loop(current_loop->start_offset, stmt->line);
            } else {
                int jump = emit_jump(VSS_OP_JUMP, stmt->line);
                if (current_loop->skip_count >= current_loop->skip_capacity) {
                    current_loop->skip_capacity = current_loop->skip_capacity < 4 ? 4 : current_loop->skip_capacity * 2;
                    current_loop->skip_jumps = realloc(current_loop->skip_jumps, current_loop->skip_capacity * sizeof(int));
                }
                current_loop->skip_jumps[current_loop->skip_count++] = jump;
            }
            break;
        }
        case VSS_STMT_TASK: {
            Compiler task_compiler;
            compiler_init(&task_compiler, stmt->as.task.name, TYPE_TASK);
            
            begin_scope();
            for (size_t i = 0; i < stmt->as.task.param_count; i++) {
                add_local(stmt->as.task.params[i], false, stmt->line);
            }
            
            VSS_Block body;
            body.statements = stmt->as.task.body.statements;
            body.count = stmt->as.task.body.count;
            compile_block(body, false, stmt->line);
            
            emit_return(stmt->line);
            VSS_ObjFunction *compiled_func = compiler_end();
            
            int func_const = make_constant(vss_value_new_function(compiled_func), stmt->line);
            vss_function_release(compiled_func); // release compilation reference
            
            emit_byte(VSS_OP_CLOSURE, stmt->line);
            emit_byte((uint8_t)func_const, stmt->line);
            
            for (int i = 0; i < compiled_func->upvalue_count; i++) {
                emit_byte(task_compiler.upvalues[i].is_local ? 1 : 0, stmt->line);
                emit_byte((uint8_t)task_compiler.upvalues[i].index, stmt->line);
            }
            
            if (current_compiler->scope_depth == 0) {
                int name_const = make_constant(vss_value_new_string(stmt->as.task.name), stmt->line);
                emit_byte(VSS_OP_DEFINE_GLOBAL, stmt->line);
                emit_byte((uint8_t)name_const, stmt->line);
                emit_byte(0, stmt->line);
            } else {
                add_local(stmt->as.task.name, false, stmt->line);
            }
            break;
        }
        case VSS_STMT_ATTEMPT: {
            int rescue_jump = emit_jump(VSS_OP_ATTEMPT, stmt->line);
            compile_block(stmt->as.attempt.try_body, true, stmt->line);
            emit_byte(VSS_OP_END_ATTEMPT, stmt->line);
            int end_jump = emit_jump(VSS_OP_JUMP, stmt->line);
            
            patch_jump(rescue_jump);
            
            begin_scope();
            add_local(stmt->as.attempt.problem_var, true, stmt->line);
            compile_block(stmt->as.attempt.rescue_body, false, stmt->line);
            end_scope(stmt->line);
            
            patch_jump(end_jump);
            break;
        }
        case VSS_STMT_CHOOSE: {
            compile_expr(stmt->as.choose.expr);
            
            begin_scope();
            add_local("$choose_target", true, stmt->line);
            int target_slot = resolve_local(current_compiler, "$choose_target");
            
            int *end_jumps = malloc(sizeof(int) * stmt->as.choose.case_count);
            for (size_t i = 0; i < stmt->as.choose.case_count; i++) {
                emit_bytes(VSS_OP_GET_LOCAL, (uint8_t)target_slot, stmt->line);
                compile_expr(stmt->as.choose.cases[i].expr);
                emit_byte(VSS_OP_SAME_AS, stmt->line);
                
                int false_jump = emit_jump(VSS_OP_JUMP_IF_FALSE, stmt->line);
                emit_byte(VSS_OP_POP, stmt->line);
                
                compile_block(stmt->as.choose.cases[i].block, true, stmt->line);
                end_jumps[i] = emit_jump(VSS_OP_JUMP, stmt->line);
                
                patch_jump(false_jump);
                emit_byte(VSS_OP_POP, stmt->line);
            }
            
            if (stmt->as.choose.otherwise_branch.count > 0) {
                compile_block(stmt->as.choose.otherwise_branch, true, stmt->line);
            }
            
            for (size_t i = 0; i < stmt->as.choose.case_count; i++) {
                patch_jump(end_jumps[i]);
            }
            free(end_jumps);
            
            end_scope(stmt->line);
            break;
        }
        case VSS_STMT_PUT: {
            compile_expr(stmt->as.put.value);
            compile_expr(stmt->as.put.list);
            emit_byte(VSS_OP_PUT_ITEM, stmt->line);
            break;
        }
        case VSS_STMT_SET_FIELD: {
            compile_expr(stmt->as.set_field.map);
            compile_expr(stmt->as.set_field.field);
            compile_expr(stmt->as.set_field.value);
            emit_byte(VSS_OP_SET_FIELD, stmt->line);
            break;
        }
        case VSS_STMT_HI_HTMVSS: {
            emit_byte(VSS_OP_HI_HTMVSS, stmt->line);
            break;
        }
        case VSS_STMT_BYE_HTMVSS: {
            emit_byte(VSS_OP_BYE_HTMVSS, stmt->line);
            break;
        }
        case VSS_STMT_EXPR: {
            compile_expr(stmt->as.expr_stmt.expression);
            emit_byte(VSS_OP_POP, stmt->line);
            break;
        }
        case VSS_STMT_GRAB: {
            int name_const = make_constant(vss_value_new_string(stmt->as.grab.module_name), stmt->line);
            emit_bytes(VSS_OP_GRAB, (uint8_t)name_const, stmt->line);
            break;
        }
        default: break;
    }
}

VSS_ObjFunction *vss_compile_program(VSS_Block program) {
    Compiler compiler;
    compiler_init(&compiler, "__main__", TYPE_MAIN);
    
    compile_block(program, false, 0);
    
    emit_return(0);
    VSS_ObjFunction *func = compiler_end();
    return func;
}
