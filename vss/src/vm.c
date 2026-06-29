#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "vm.h"
#include "lexer.h"
#include "parser.h"
#include "compiler.h"
#include "interpreter.h" // for register_builtins

static VM *current_vm_instance = NULL;

static void runtime_error(const char *format, ...);

static char *safe_strdup(const char *s) {
    if (!s) return NULL;
    char *dup = malloc(strlen(s) + 1);
    if (dup) strcpy(dup, s);
    return dup;
}

void vm_init(VM *vm, Env *global_env) {
    vm->frame_count = 0;
    vm->stack_top = vm->stack;
    vm->trap_count = 0;
    vm->open_upvalues = NULL;
    vm->globals = global_env;
    if (global_env) {
        env_retain(global_env);
    }
    current_vm_instance = vm;
}

void vm_free(VM *vm) {
    if (vm->globals) {
        env_release(vm->globals);
    }
    while (vm->stack_top > vm->stack) {
        vm->stack_top--;
        value_release(*vm->stack_top);
    }
    current_vm_instance = NULL;
}

static void push(Value value) {
    if (current_vm_instance->stack_top >= current_vm_instance->stack + STACK_MAX) {
        fprintf(stderr, "Stack overflow.\n");
        exit(1);
    }
    *current_vm_instance->stack_top = value;
    current_vm_instance->stack_top++;
}

static Value pop(void) {
    if (current_vm_instance->stack_top == current_vm_instance->stack) {
        fprintf(stderr, "Stack underflow.\n");
        exit(1);
    }
    current_vm_instance->stack_top--;
    return *current_vm_instance->stack_top;
}

static Value peek(int distance) {
    return *(current_vm_instance->stack_top - 1 - distance);
}

static Upvalue *capture_upvalue(VM *vm, Value *local) {
    Upvalue *prev = NULL;
    Upvalue *curr = vm->open_upvalues;
    while (curr != NULL && curr->location > local) {
        prev = curr;
        curr = curr->next;
    }
    if (curr != NULL && curr->location == local) {
        return curr;
    }
    Upvalue *created = upvalue_new(local);
    created->next = curr;
    if (prev == NULL) {
        vm->open_upvalues = created;
    } else {
        prev->next = created;
    }
    return created;
}

static void close_upvalues(VM *vm, Value *last) {
    while (vm->open_upvalues != NULL && vm->open_upvalues->location >= last) {
        Upvalue *uv = vm->open_upvalues;
        uv->closed_value = *uv->location;
        value_retain(uv->closed_value);
        uv->location = &uv->closed_value;
        vm->open_upvalues = uv->next;
    }
}

static void runtime_error(const char *format, ...) {
    VM *vm = current_vm_instance;
    CallFrame *frame = &vm->frames[vm->frame_count - 1];
    size_t offset = frame->ip - frame->closure->function->chunk.code - 1;
    int line = frame->closure->function->chunk.lines[offset];
    
    fprintf(stderr, "runtime error line %d: ", line);
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fprintf(stderr, "\n");
    
    // Check if we have an active try/rescue trap
    if (vm->trap_count > 0) {
        // Pop trap
        vm->trap_count--;
        TrapFrame trap = vm->traps[vm->trap_count];
        
        // Restore stack
        while (vm->stack_top > vm->stack + trap.depth) {
            value_release(pop());
        }
        
        // Push error message as string
        char err_msg[512];
        va_start(args, format);
        vsnprintf(err_msg, sizeof(err_msg), format, args);
        va_end(args);
        
        push(value_new_string(err_msg));
        
        // Jump to handler
        frame->ip = trap.handler_ip;
        longjmp(vm->jump_buffer, 1);
    }
    
    // Stack trace
    for (int i = vm->frame_count - 1; i >= 0; i--) {
        CallFrame *f = &vm->frames[i];
        ObjFunction *func = f->closure->function;
        size_t off = f->ip - func->chunk.code - 1;
        fprintf(stderr, "  at %s() [line %d]\n", func->name[0] == '\0' ? "script" : func->name, func->chunk.lines[off]);
    }
    
    exit(1);
}

static char *read_file_text(const char *path) {
    FILE *file = fopen(path, "rb");
    if (!file) return NULL;
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    rewind(file);
    char *buffer = malloc(size + 1);
    size_t read_bytes = fread(buffer, 1, size, file);
    fclose(file);
    buffer[read_bytes] = '\0';
    return buffer;
}

bool vm_run(ObjFunction *func, Env *global_env) {
    VM vm;
    vm_init(&vm, global_env);
    
    ObjClosure *closure = closure_new(func);
    push(value_new_closure(closure));
    
    CallFrame *frame = &vm.frames[vm.frame_count++];
    frame->closure = closure;
    frame->ip = func->chunk.code;
    frame->slots = vm.stack;
    
    if (setjmp(vm.jump_buffer) != 0) {
        // Trap triggered, frame IP is updated.
    }
    
    for (;;) {
        frame = &vm.frames[vm.frame_count - 1];
        uint8_t instruction = *frame->ip++;
        switch (instruction) {
            case OP_CONSTANT: {
                uint8_t constant = *frame->ip++;
                push(frame->closure->function->chunk.constants[constant]);
                value_retain(peek(0));
                break;
            }
            case OP_CONSTANT_LONG: {
                uint8_t b1 = *frame->ip++;
                uint8_t b2 = *frame->ip++;
                uint8_t b3 = *frame->ip++;
                uint32_t constant = b1 | (b2 << 8) | (b3 << 16);
                push(frame->closure->function->chunk.constants[constant]);
                value_retain(peek(0));
                break;
            }
            case OP_EMPTY:
                push(value_new_empty());
                break;
            case OP_TRUE:
                push(value_new_bool(true));
                break;
            case OP_FALSE:
                push(value_new_bool(false));
                break;
            case OP_POP:
                value_release(pop());
                break;
            case OP_GET_LOCAL: {
                uint8_t slot = *frame->ip++;
                Value val = frame->slots[slot];
                value_retain(val);
                push(val);
                break;
            }
            case OP_SET_LOCAL: {
                uint8_t slot = *frame->ip++;
                Value val = peek(0);
                value_retain(val);
                value_release(frame->slots[slot]);
                frame->slots[slot] = val;
                break;
            }
            case OP_GET_GLOBAL: {
                uint8_t constant = *frame->ip++;
                Value name_val = frame->closure->function->chunk.constants[constant];
                const char *name = name_val.as.string->chars;
                Value val;
                if (!env_get(vm.globals, name, &val)) {
                    runtime_error("Undefined variable '%s'.", name);
                    return false;
                }
                value_retain(val);
                push(val);
                break;
            }
            case OP_DEFINE_GLOBAL: {
                uint8_t constant = *frame->ip++;
                uint8_t is_const = *frame->ip++;
                Value name_val = frame->closure->function->chunk.constants[constant];
                const char *name = name_val.as.string->chars;
                Value val = pop();
                if (is_const) {
                    env_define_const(vm.globals, name, val);
                } else {
                    env_define(vm.globals, name, val);
                }
                value_release(val);
                break;
            }
            case OP_SET_GLOBAL: {
                uint8_t constant = *frame->ip++;
                Value name_val = frame->closure->function->chunk.constants[constant];
                const char *name = name_val.as.string->chars;
                Value val = peek(0);
                if (!env_assign(vm.globals, name, val)) {
                    runtime_error("Cannot reassign to '%s' (either constant or undefined).", name);
                    return false;
                }
                break;
            }
            case OP_GET_UPVALUE: {
                uint8_t slot = *frame->ip++;
                Value val = *frame->closure->upvalues[slot]->location;
                value_retain(val);
                push(val);
                break;
            }
            case OP_SET_UPVALUE: {
                uint8_t slot = *frame->ip++;
                Value val = peek(0);
                value_retain(val);
                value_release(*frame->closure->upvalues[slot]->location);
                *frame->closure->upvalues[slot]->location = val;
                break;
            }
            case OP_ADD: {
                Value r = pop();
                Value l = pop();
                if (l.type == VAL_NUMBER && r.type == VAL_NUMBER) {
                    push(value_new_number(l.as.number + r.as.number));
                } else if (l.type == VAL_STRING && r.type == VAL_STRING) {
                    char *joined = malloc(strlen(l.as.string->chars) + strlen(r.as.string->chars) + 1);
                    strcpy(joined, l.as.string->chars);
                    strcat(joined, r.as.string->chars);
                    push(value_new_string(joined));
                    free(joined);
                } else {
                    value_release(l); value_release(r);
                    runtime_error("Can only add numbers or join strings.");
                    return false;
                }
                value_release(l); value_release(r);
                break;
            }
            case OP_SUB:
            case OP_MUL:
            case OP_DIV:
            case OP_MOD: {
                Value r = pop();
                Value l = pop();
                if (l.type != VAL_NUMBER || r.type != VAL_NUMBER) {
                    value_release(l); value_release(r);
                    runtime_error("Arithmetic operands must be numbers.");
                    return false;
                }
                double lv = l.as.number;
                double rv = r.as.number;
                value_release(l); value_release(r);
                if (instruction == OP_SUB) push(value_new_number(lv - rv));
                else if (instruction == OP_MUL) push(value_new_number(lv * rv));
                else if (instruction == OP_DIV) {
                    if (rv == 0.0) {
                        runtime_error("Division by zero.");
                        return false;
                    }
                    push(value_new_number(lv / rv));
                } else {
                    if (rv == 0.0) {
                        runtime_error("Modulo by zero.");
                        return false;
                    }
                    push(value_new_number((long long)lv % (long long)rv));
                }
                break;
            }
            case OP_NOT: {
                Value val = pop();
                push(value_new_bool(!value_truthy(val)));
                value_release(val);
                break;
            }
            case OP_NEGATE: {
                Value val = pop();
                if (val.type != VAL_NUMBER) {
                    value_release(val);
                    runtime_error("Operand to '-' must be a number.");
                    return false;
                }
                push(value_new_number(-val.as.number));
                value_release(val);
                break;
            }
            case OP_ABOVE:
            case OP_BELOW:
            case OP_AT_LEAST:
            case OP_AT_MOST: {
                Value r = pop();
                Value l = pop();
                if (l.type != VAL_NUMBER || r.type != VAL_NUMBER) {
                    value_release(l); value_release(r);
                    runtime_error("Comparison operands must be numbers.");
                    return false;
                }
                double lv = l.as.number;
                double rv = r.as.number;
                value_release(l); value_release(r);
                if (instruction == OP_ABOVE) push(value_new_bool(lv > rv));
                else if (instruction == OP_BELOW) push(value_new_bool(lv < rv));
                else if (instruction == OP_AT_LEAST) push(value_new_bool(lv >= rv));
                else push(value_new_bool(lv <= rv));
                break;
            }
            case OP_SAME_AS:
            case OP_NOT_SAME_AS: {
                Value r = pop();
                Value l = pop();
                bool same = value_same_as(l, r);
                push(value_new_bool(instruction == OP_SAME_AS ? same : !same));
                value_release(l); value_release(r);
                break;
            }
            case OP_SAY: {
                Value val = pop();
                value_say(val);
                value_release(val);
                break;
            }
            case OP_JUMP: {
                uint16_t offset = (uint16_t)(frame->ip[0] << 8) | frame->ip[1];
                frame->ip += 2;
                frame->ip += offset;
                break;
            }
            case OP_JUMP_IF_FALSE: {
                uint16_t offset = (uint16_t)(frame->ip[0] << 8) | frame->ip[1];
                frame->ip += 2;
                if (!value_truthy(peek(0))) {
                    frame->ip += offset;
                }
                break;
            }
            case OP_LOOP: {
                uint16_t offset = (uint16_t)(frame->ip[0] << 8) | frame->ip[1];
                frame->ip += 2;
                frame->ip -= offset;
                break;
            }
            case OP_CALL: {
                uint8_t arg_count = *frame->ip++;
                Value callee_val = peek(arg_count);
                if (callee_val.type != VAL_CLOSURE && callee_val.type != VAL_NATIVE) {
                    runtime_error("Value is not callable.");
                    return false;
                }
                
                if (callee_val.type == VAL_CLOSURE) {
                    ObjClosure *callee = callee_val.as.closure;
                    if (arg_count != callee->function->param_count) {
                        runtime_error("Expected %zu arguments but got %zu.", callee->function->param_count, arg_count);
                        return false;
                    }
                    
                    if (vm.frame_count >= FRAMES_MAX) {
                        runtime_error("Stack overflow (max call frames reached).");
                        return false;
                    }
                    
                    CallFrame *next_frame = &vm.frames[vm.frame_count++];
                    next_frame->closure = callee;
                    next_frame->ip = callee->function->chunk.code;
                    next_frame->slots = vm.stack_top - arg_count - 1;
                    
                    frame = next_frame;
                } else {
                    // Native task
                    NativeFnPtr native = callee_val.as.native;
                    Value *args = vm.stack_top - arg_count;
                    
                    bool err = false;
                    char *err_msg = NULL;
                    Value ret = native(arg_count, args, &err, &err_msg);
                    
                    if (err) {
                        runtime_error("%s", err_msg);
                        free(err_msg);
                        return false;
                    }
                    
                    // Pop args + native function
                    for (int i = 0; i <= arg_count; i++) {
                        value_release(pop());
                    }
                    push(ret);
                }
                break;
            }
            case OP_CLOSURE: {
                uint8_t func_idx = *frame->ip++;
                Value func_val = frame->closure->function->chunk.constants[func_idx];
                ObjFunction *func = func_val.as.function;
                
                ObjClosure *cls = closure_new(func);
                Value cls_val = value_new_closure(cls);
                push(cls_val);
                
                for (int i = 0; i < func->upvalue_count; i++) {
                    uint8_t is_local = *frame->ip++;
                    uint8_t index = *frame->ip++;
                    if (is_local) {
                        cls->upvalues[i] = capture_upvalue(&vm, frame->slots + index);
                        upvalue_retain(cls->upvalues[i]);
                    } else {
                        cls->upvalues[i] = frame->closure->upvalues[index];
                        upvalue_retain(cls->upvalues[i]);
                    }
                }
                break;
            }
            case OP_RETURN: {
                Value ret_val = pop(); // pop return value
                close_upvalues(&vm, frame->slots);
                
                vm.frame_count--;
                if (vm.frame_count == 0) {
                    value_release(pop()); // pop closure
                    push(ret_val);
                    vm_free(&vm);
                    return true;
                }
                
                // pop frame locals
                while (vm.stack_top > frame->slots) {
                    value_release(pop());
                }
                
                push(ret_val);
                frame = &vm.frames[vm.frame_count - 1];
                break;
            }
            case OP_BUILD_LIST: {
                uint8_t count = *frame->ip++;
                Value list_val = value_new_list();
                ValList *l = list_val.as.list;
                
                l->count = count;
                l->capacity = count;
                l->items = malloc(sizeof(Value) * count);
                
                // Pop items in reverse order
                for (int i = count - 1; i >= 0; i--) {
                    l->items[i] = pop();
                }
                push(list_val);
                break;
            }
            case OP_BUILD_MAP: {
                uint8_t count = *frame->ip++;
                Value map_val = value_new_map();
                ValMap *m = map_val.as.map;
                
                m->count = count;
                m->capacity = count;
                m->entries = malloc(sizeof(ValMapEntry) * count);
                
                // Alternating keys and values on stack: key1, val1, key2, val2...
                // Pop them in reverse order
                for (int i = count - 1; i >= 0; i--) {
                    m->entries[i].value = pop();
                    Value key_val = pop();
                    m->entries[i].key = safe_strdup(key_val.as.string->chars);
                    value_release(key_val);
                }
                push(map_val);
                break;
            }
            case OP_GET_ITEM: {
                Value idx = pop();
                Value list = pop();
                if (list.type != VAL_LIST || idx.type != VAL_NUMBER) {
                    value_release(list); value_release(idx);
                    runtime_error("List item access expects a list and a numeric index.");
                    return false;
                }
                long long index = (long long)idx.as.number;
                ValList *l = list.as.list;
                if (index < 0 || (size_t)index >= l->count) {
                    value_release(list); value_release(idx);
                    runtime_error("List index out of range (got %lld, size is %zu).", index, l->count);
                    return false;
                }
                Value item = l->items[index];
                value_retain(item);
                push(item);
                value_release(list); value_release(idx);
                break;
            }
            case OP_PUT_ITEM: {
                Value list = pop();
                Value val = pop(); // stack has val, list
                if (list.type != VAL_LIST) {
                    value_release(list); value_release(val);
                    runtime_error("put statement expects a list.");
                    return false;
                }
                ValList *l = list.as.list;
                if (l->count >= l->capacity) {
                    l->capacity = l->capacity == 0 ? 8 : l->capacity * 2;
                    l->items = realloc(l->items, sizeof(Value) * l->capacity);
                }
                l->items[l->count++] = val; // ownership transferred
                value_release(list);
                break;
            }
            case OP_GET_FIELD: {
                Value field = pop();
                Value map = pop();
                if (map.type != VAL_MAP || field.type != VAL_STRING) {
                    value_release(map); value_release(field);
                    runtime_error("Map field access expects a map and a string key.");
                    return false;
                }
                const char *key = field.as.string->chars;
                ValMap *m = map.as.map;
                bool found = false;
                for (size_t i = 0; i < m->count; i++) {
                    if (strcmp(m->entries[i].key, key) == 0) {
                        Value fval = m->entries[i].value;
                        value_retain(fval);
                        push(fval);
                        found = true;
                        break;
                    }
                }
                value_release(map); value_release(field);
                if (!found) {
                    runtime_error("Map key '%s' not found.", key);
                    return false;
                }
                break;
            }
            case OP_SET_FIELD: {
                Value val = pop();
                Value field = pop();
                Value map = pop();
                if (map.type != VAL_MAP || field.type != VAL_STRING) {
                    value_release(map); value_release(field); value_release(val);
                    runtime_error("set statement expects a map, string key, and value.");
                    return false;
                }
                const char *key = field.as.string->chars;
                ValMap *m = map.as.map;
                bool found = false;
                for (size_t i = 0; i < m->count; i++) {
                    if (strcmp(m->entries[i].key, key) == 0) {
                        value_release(m->entries[i].value);
                        m->entries[i].value = val; // transferred
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
                    m->entries[m->count].value = val;
                    m->count++;
                }
                value_release(map); value_release(field);
                break;
            }
            case OP_SIZE_OF: {
                Value val = pop();
                if (val.type == VAL_LIST) push(value_new_number(val.as.list->count));
                else if (val.type == VAL_MAP) push(value_new_number(val.as.map->count));
                else if (val.type == VAL_STRING) push(value_new_number(strlen(val.as.string->chars)));
                else {
                    value_release(val);
                    runtime_error("size of expects list, map, or string.");
                    return false;
                }
                value_release(val);
                break;
            }
            case OP_ATTEMPT: {
                uint16_t offset = (uint16_t)(frame->ip[0] << 8) | frame->ip[1];
                frame->ip += 2;
                if (vm.trap_count >= TRAPS_MAX) {
                    runtime_error("Too many nested rescue attempt traps.");
                    return false;
                }
                TrapFrame *trap = &vm.traps[vm.trap_count++];
                trap->depth = vm.stack_top - vm.stack;
                trap->handler_ip = frame->ip + offset;
                break;
            }
            case OP_END_ATTEMPT: {
                if (vm.trap_count > 0) vm.trap_count--;
                break;
            }
            case OP_GRAB: {
                uint8_t name_const = *frame->ip++;
                Value name_val = frame->closure->function->chunk.constants[name_const];
                const char *module_name = name_val.as.string->chars;
                
                char filepath[256];
                snprintf(filepath, sizeof(filepath), "%s.vss", module_name);
                FILE *f = fopen(filepath, "rb");
                if (!f) {
                    snprintf(filepath, sizeof(filepath), "examples/%s.vss", module_name);
                    f = fopen(filepath, "rb");
                }
                if (!f) {
                    runtime_error("Grab module '%s' not found.", module_name);
                    return false;
                }
                fclose(f);
                
                char *source = read_file_text(filepath);
                Lexer mod_lexer;
                lexer_init(&mod_lexer, source);
                Parser mod_parser;
                parser_init(&mod_parser, &mod_lexer);
                Block mod_ast = parse_program(&mod_parser);
                free(source);
                
                if (mod_parser.had_error) {
                    block_free(mod_ast);
                    runtime_error("Syntax error in module '%s'.", module_name);
                    return false;
                }
                
                ObjFunction *mod_func = compile_program(mod_ast);
                block_free(mod_ast);
                
                Env *mod_env = env_new(NULL);
                register_builtins(mod_env);
                
                bool run_success = vm_run(mod_func, mod_env);
                function_release(mod_func);
                
                if (!run_success) {
                    env_release(mod_env);
                    return false;
                }
                
                // Copy all module bindings to current vm globals
                for (size_t i = 0; i < mod_env->count; i++) {
                    if (strncmp(mod_env->items[i].name, "__", 2) == 0) continue;
                    
                    if (mod_env->items[i].is_constant) {
                        env_define_const(vm.globals, mod_env->items[i].name, mod_env->items[i].value);
                    } else {
                        env_define(vm.globals, mod_env->items[i].name, mod_env->items[i].value);
                    }
                }
                
                env_release(mod_env);
                break;
            }
            case OP_HI_HTMVSS:
                printf("<!DOCTYPE html>\n<html>\n<head>\n<script src=\"https://cdn.tailwindcss.com\"></script>\n</head>\n<body class=\"bg-slate-900 text-white font-sans flex flex-col items-center justify-center min-h-screen\">\n");
                break;
            case OP_BYE_HTMVSS:
                printf("</body>\n</html>\n");
                break;
            default:
                runtime_error("Unknown VM instruction %d.", instruction);
                return false;
        }
    }
}
