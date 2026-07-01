#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "vm.h"
#include "lexer.h"
#include "parser.h"
#include "compiler.h"
#include "interpreter.h" // for vss_register_builtins

typedef struct {
    char *name;
    VSS_Value exports;
    bool is_loading;
} CachedModule;

static CachedModule *module_cache = NULL;
static size_t module_cache_count = 0;

static void clear_module_cache(void) {
    for (size_t i = 0; i < module_cache_count; i++) {
        free(module_cache[i].name);
        vss_value_release(module_cache[i].exports);
    }
    free(module_cache);
    module_cache = NULL;
    module_cache_count = 0;
}

static CachedModule *get_cached_module(const char *name) {
    for (size_t i = 0; i < module_cache_count; i++) {
        if (strcmp(module_cache[i].name, name) == 0) return &module_cache[i];
    }
    return NULL;
}

static char *safe_strdup(const char *s) {
    if (!s) return NULL;
    char *dup = malloc(strlen(s) + 1);
    if (dup) strcpy(dup, s);
    return dup;
}

static void add_cached_module(const char *name, VSS_Value exports, bool is_loading) {
    module_cache = realloc(module_cache, sizeof(CachedModule) * (module_cache_count + 1));
    module_cache[module_cache_count].name = safe_strdup(name);
    module_cache[module_cache_count].exports = exports;
    vss_value_retain(exports);
    module_cache[module_cache_count].is_loading = is_loading;
    module_cache_count++;
}

static VSS_VM *current_vm_instance = NULL;

static void runtime_error(const char *format, ...);

void vss_vm_init(VSS_VM *vm, VSS_Env *global_env) {
    vm->stack_top = vm->stack;
    vm->frame_count = 0;
    vm->trap_count = 0;
    vm->open_upvalues = NULL;
    vm->globals = global_env;
    if (global_env) {
        vss_env_retain(global_env);
    }
    vm->prev_vm_instance = current_vm_instance;
    current_vm_instance = vm;
}

void vss_vm_free(VSS_VM *vm) {
    if (vm->globals) {
        vss_env_release(vm->globals);
    }
    while (vm->stack_top > vm->stack) {
        vm->stack_top--;
        vss_value_release(*vm->stack_top);
    }
    current_vm_instance = vm->prev_vm_instance;
}

static void push(VSS_Value value) {
    if (current_vm_instance->stack_top >= current_vm_instance->stack + VSS_STACK_MAX) {
        fprintf(stderr, "Stack overflow.\n");
        exit(1);
    }
    *current_vm_instance->stack_top = value;
    current_vm_instance->stack_top++;
}

static VSS_Value pop(void) {
    if (current_vm_instance->stack_top == current_vm_instance->stack) {
        fprintf(stderr, "Stack underflow.\n");
        exit(1);
    }
    current_vm_instance->stack_top--;
    return *current_vm_instance->stack_top;
}

static VSS_Value peek(int distance) {
    return *(current_vm_instance->stack_top - 1 - distance);
}

static VSS_Upvalue *capture_upvalue(VSS_VM *vm, VSS_Value *local) {
    VSS_Upvalue *prev = NULL;
    VSS_Upvalue *curr = vm->open_upvalues;
    while (curr != NULL && curr->location > local) {
        prev = curr;
        curr = curr->next;
    }
    if (curr != NULL && curr->location == local) {
        return curr;
    }
    VSS_Upvalue *created = vss_upvalue_new(local);
    created->next = curr;
    if (prev == NULL) {
        vm->open_upvalues = created;
    } else {
        prev->next = created;
    }
    return created;
}

static void close_upvalues(VSS_VM *vm, VSS_Value *last) {
    while (vm->open_upvalues != NULL && vm->open_upvalues->location >= last) {
        VSS_Upvalue *uv = vm->open_upvalues;
        uv->closed_value = *uv->location;
        vss_value_retain(uv->closed_value);
        uv->location = &uv->closed_value;
        vm->open_upvalues = uv->next;
    }
}

static void runtime_error(const char *format, ...) {
    VSS_VM *vm = current_vm_instance;
    VSS_CallFrame *frame = &vm->frames[vm->frame_count - 1];
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
        VSS_TrapFrame trap = vm->traps[vm->trap_count];
        
        // Restore stack
        while (vm->stack_top > vm->stack + trap.depth) {
            vss_value_release(pop());
        }
        
        // Push error message as string
        char err_msg[512];
        va_start(args, format);
        vsnprintf(err_msg, sizeof(err_msg), format, args);
        va_end(args);
        
        push(vss_value_new_string(err_msg));
        
        // Jump to handler
        frame->ip = trap.handler_ip;
        longjmp(vm->jump_buffer, 1);
    }
    
    // Stack trace
    for (int i = vm->frame_count - 1; i >= 0; i--) {
        VSS_CallFrame *f = &vm->frames[i];
        VSS_ObjFunction *func = f->closure->function;
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

bool vss_vm_run(VSS_ObjFunction *func, VSS_Env *global_env) {
    if (strcmp(func->name, "__main__") == 0) {
        clear_module_cache();
    }
    VSS_VM vm;
    vss_vm_init(&vm, global_env);
    
    VSS_ObjClosure *closure = vss_closure_new(func);
    push(vss_value_new_closure(closure));
    
    VSS_CallFrame *frame = &vm.frames[vm.frame_count++];
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
            case VSS_OP_CONSTANT: {
                uint8_t constant = *frame->ip++;
                push(frame->closure->function->chunk.constants[constant]);
                vss_value_retain(peek(0));
                break;
            }
            case VSS_OP_CONSTANT_LONG: {
                uint8_t b1 = *frame->ip++;
                uint8_t b2 = *frame->ip++;
                uint8_t b3 = *frame->ip++;
                uint32_t constant = b1 | (b2 << 8) | (b3 << 16);
                push(frame->closure->function->chunk.constants[constant]);
                vss_value_retain(peek(0));
                break;
            }
            case VSS_OP_EMPTY:
                push(vss_value_new_empty());
                break;
            case VSS_OP_TRUE:
                push(vss_value_new_bool(true));
                break;
            case VSS_OP_FALSE:
                push(vss_value_new_bool(false));
                break;
            case VSS_OP_POP:
                vss_value_release(pop());
                break;
            case VSS_OP_GET_LOCAL: {
                uint8_t slot = *frame->ip++;
                VSS_Value val = frame->slots[slot];
                vss_value_retain(val);
                push(val);
                break;
            }
            case VSS_OP_SET_LOCAL: {
                uint8_t slot = *frame->ip++;
                VSS_Value val = peek(0);
                vss_value_retain(val);
                vss_value_release(frame->slots[slot]);
                frame->slots[slot] = val;
                break;
            }
            case VSS_OP_GET_GLOBAL: {
                uint8_t constant = *frame->ip++;
                VSS_Value name_val = frame->closure->function->chunk.constants[constant];
                const char *name = name_val.as.string->chars;
                VSS_Value val;
                if (strcmp(name, "mine") == 0 && frame->closure->receiver.type != VSS_VAL_EMPTY) {
                    val = frame->closure->receiver;
                    vss_value_retain(val);
                    push(val);
                } else {
                    if (!vss_env_get(vm.globals, name, &val)) {
                        runtime_error("Undefined variable '%s'.", name);
                        return false;
                    }
                    vss_value_retain(val);
                    push(val);
                }
                break;
            }
            case VSS_OP_DEFINE_GLOBAL: {
                uint8_t constant = *frame->ip++;
                uint8_t is_const = *frame->ip++;
                VSS_Value name_val = frame->closure->function->chunk.constants[constant];
                const char *name = name_val.as.string->chars;
                VSS_Value val = pop();
                if (is_const) {
                    vss_env_define_const(vm.globals, name, val);
                } else {
                    vss_env_define(vm.globals, name, val);
                }
                vss_value_release(val);
                break;
            }
            case VSS_OP_SET_GLOBAL: {
                uint8_t constant = *frame->ip++;
                VSS_Value name_val = frame->closure->function->chunk.constants[constant];
                const char *name = name_val.as.string->chars;
                VSS_Value val = peek(0);
                if (!vss_env_assign(vm.globals, name, val)) {
                    runtime_error("Cannot reassign to '%s' (either constant or undefined).", name);
                    return false;
                }
                break;
            }
            case VSS_OP_GET_UPVALUE: {
                uint8_t slot = *frame->ip++;
                VSS_Value val = *frame->closure->upvalues[slot]->location;
                vss_value_retain(val);
                push(val);
                break;
            }
            case VSS_OP_SET_UPVALUE: {
                uint8_t slot = *frame->ip++;
                VSS_Value val = peek(0);
                vss_value_retain(val);
                vss_value_release(*frame->closure->upvalues[slot]->location);
                *frame->closure->upvalues[slot]->location = val;
                break;
            }
            case VSS_OP_ADD: {
                VSS_Value r = pop();
                VSS_Value l = pop();
                if (l.type == VSS_VAL_NUMBER && r.type == VSS_VAL_NUMBER) {
                    push(vss_value_new_number(l.as.number + r.as.number));
                } else if (l.type == VSS_VAL_STRING || r.type == VSS_VAL_STRING) {
                    char *l_str = vss_value_to_string(l);
                    char *r_str = vss_value_to_string(r);
                    char *joined = malloc(strlen(l_str) + strlen(r_str) + 1);
                    strcpy(joined, l_str);
                    strcat(joined, r_str);
                    push(vss_value_new_string(joined));
                    free(joined);
                    free(l_str);
                    free(r_str);
                } else {
                    vss_value_release(l); vss_value_release(r);
                    runtime_error("Can only add numbers or join strings.");
                    return false;
                }
                vss_value_release(l); vss_value_release(r);
                break;
            }
            case VSS_OP_SUB:
            case VSS_OP_MUL:
            case VSS_OP_DIV:
            case VSS_OP_MOD: {
                VSS_Value r = pop();
                VSS_Value l = pop();
                if (l.type != VSS_VAL_NUMBER || r.type != VSS_VAL_NUMBER) {
                    vss_value_release(l); vss_value_release(r);
                    runtime_error("Arithmetic operands must be numbers.");
                    return false;
                }
                double lv = l.as.number;
                double rv = r.as.number;
                vss_value_release(l); vss_value_release(r);
                if (instruction == VSS_OP_SUB) push(vss_value_new_number(lv - rv));
                else if (instruction == VSS_OP_MUL) push(vss_value_new_number(lv * rv));
                else if (instruction == VSS_OP_DIV) {
                    if (rv == 0.0) {
                        runtime_error("Division by zero.");
                        return false;
                    }
                    push(vss_value_new_number(lv / rv));
                } else {
                    if (rv == 0.0) {
                        runtime_error("Modulo by zero.");
                        return false;
                    }
                    push(vss_value_new_number((long long)lv % (long long)rv));
                }
                break;
            }
            case VSS_OP_NOT: {
                VSS_Value val = pop();
                push(vss_value_new_bool(!vss_value_truthy(val)));
                vss_value_release(val);
                break;
            }
            case VSS_OP_NEGATE: {
                VSS_Value val = pop();
                if (val.type != VSS_VAL_NUMBER) {
                    vss_value_release(val);
                    runtime_error("Operand to '-' must be a number.");
                    return false;
                }
                push(vss_value_new_number(-val.as.number));
                vss_value_release(val);
                break;
            }
            case VSS_OP_ABOVE:
            case VSS_OP_BELOW:
            case VSS_OP_AT_LEAST:
            case VSS_OP_AT_MOST: {
                VSS_Value r = pop();
                VSS_Value l = pop();
                if (l.type != VSS_VAL_NUMBER || r.type != VSS_VAL_NUMBER) {
                    vss_value_release(l); vss_value_release(r);
                    runtime_error("Comparison operands must be numbers.");
                    return false;
                }
                double lv = l.as.number;
                double rv = r.as.number;
                vss_value_release(l); vss_value_release(r);
                if (instruction == VSS_OP_ABOVE) push(vss_value_new_bool(lv > rv));
                else if (instruction == VSS_OP_BELOW) push(vss_value_new_bool(lv < rv));
                else if (instruction == VSS_OP_AT_LEAST) push(vss_value_new_bool(lv >= rv));
                else push(vss_value_new_bool(lv <= rv));
                break;
            }
            case VSS_OP_SAME_AS:
            case VSS_OP_NOT_SAME_AS: {
                VSS_Value r = pop();
                VSS_Value l = pop();
                bool same = vss_value_same_as(l, r);
                push(vss_value_new_bool(instruction == VSS_OP_SAME_AS ? same : !same));
                vss_value_release(l); vss_value_release(r);
                break;
            }
            case VSS_OP_SAY: {
                VSS_Value val = pop();
                vss_value_say(val);
                vss_value_release(val);
                break;
            }
            case VSS_OP_JUMP: {
                uint16_t offset = (uint16_t)(frame->ip[0] << 8) | frame->ip[1];
                frame->ip += 2;
                frame->ip += offset;
                break;
            }
            case VSS_OP_JUMP_IF_FALSE: {
                uint16_t offset = (uint16_t)(frame->ip[0] << 8) | frame->ip[1];
                frame->ip += 2;
                if (!vss_value_truthy(peek(0))) {
                    frame->ip += offset;
                }
                break;
            }
            case VSS_OP_LOOP: {
                uint16_t offset = (uint16_t)(frame->ip[0] << 8) | frame->ip[1];
                frame->ip += 2;
                frame->ip -= offset;
                break;
            }
            case VSS_OP_CALL: {
                uint8_t arg_count = *frame->ip++;
                VSS_Value callee_val = peek(arg_count);
                if (callee_val.type != VSS_VAL_CLOSURE && callee_val.type != VSS_VAL_NATIVE && callee_val.type != VSS_VAL_CLASS) {
                    runtime_error("VSS_Value is not callable.");
                    return false;
                }
                
                if (callee_val.type == VSS_VAL_CLASS) {
                    VSS_Value inst = vss_value_new_instance(callee_val.as.klass);
                    VSS_ObjClass *curr = callee_val.as.klass;
                    VSS_Value create_val;
                    bool found = false;
                    while (curr) {
                        for (size_t i = 0; i < curr->methods->count; i++) {
                            if (strcmp(curr->methods->entries[i].key, "create") == 0) {
                                create_val = curr->methods->entries[i].value;
                                found = true;
                                break;
                            }
                        }
                        if (found) break;
                        curr = curr->parent;
                    }
                    
                    if (found) {
                        VSS_ObjClosure *bound = NULL;
                        if (create_val.type == VSS_VAL_CLOSURE) {
                            bound = vss_closure_new(create_val.as.closure->function);
                            for (int i = 0; i < bound->upvalue_count; i++) {
                                bound->upvalues[i] = create_val.as.closure->upvalues[i];
                                if (bound->upvalues[i]) {
                                    vss_upvalue_retain(bound->upvalues[i]);
                                }
                            }
                            bound->receiver = inst;
                            vss_value_retain(inst);
                        } else if (create_val.type == VSS_VAL_FUNCTION) {
                            bound = vss_closure_new(create_val.as.function);
                            bound->receiver = inst;
                            vss_value_retain(inst);
                        } else {
                            for (int i = 0; i <= arg_count; i++) {
                                vss_value_release(pop());
                            }
                            push(inst);
                            break;
                        }
                        
                        vss_value_release(*(vm.stack_top - arg_count - 1));
                        *(vm.stack_top - arg_count - 1) = vss_value_new_closure(bound);
                        
                        if (arg_count != bound->function->param_count) {
                            runtime_error("Expected %zu arguments but got %zu.", bound->function->param_count, arg_count);
                            return false;
                        }
                        
                        if (vm.frame_count >= VSS_FRAMES_MAX) {
                            runtime_error("Stack overflow (max call frames reached).");
                            return false;
                        }
                        
                        VSS_CallFrame *next_frame = &vm.frames[vm.frame_count++];
                        next_frame->closure = bound;
                        next_frame->ip = bound->function->chunk.code;
                        next_frame->slots = vm.stack_top - arg_count - 1;
                        
                        frame = next_frame;
                    } else {
                        if (arg_count != 0) {
                            runtime_error("Constructor 'create' not found on class '%s'.", callee_val.as.klass->name);
                            vss_value_release(inst);
                            return false;
                        }
                        vss_value_release(pop());
                        push(inst);
                    }
                } else if (callee_val.type == VSS_VAL_CLOSURE) {
                    VSS_ObjClosure *callee = callee_val.as.closure;
                    if (arg_count != callee->function->param_count) {
                        runtime_error("Expected %zu arguments but got %zu.", callee->function->param_count, arg_count);
                        return false;
                    }
                    
                    if (vm.frame_count >= VSS_FRAMES_MAX) {
                        runtime_error("Stack overflow (max call frames reached).");
                        return false;
                    }
                    
                    VSS_CallFrame *next_frame = &vm.frames[vm.frame_count++];
                    next_frame->closure = callee;
                    next_frame->ip = callee->function->chunk.code;
                    next_frame->slots = vm.stack_top - arg_count - 1;
                    
                    frame = next_frame;
                } else {
                    VSS_NativeFnPtr native = callee_val.as.native;
                    VSS_Value *args = vm.stack_top - arg_count;
                    
                    bool err = false;
                    char *err_msg = NULL;
                    VSS_Value ret = native(arg_count, args, &err, &err_msg);
                    
                    if (err) {
                        runtime_error("%s", err_msg);
                        free(err_msg);
                        return false;
                    }
                    
                    for (int i = 0; i <= arg_count; i++) {
                        vss_value_release(pop());
                    }
                    push(ret);
                }
                break;
            }
            case VSS_OP_CLOSURE: {
                uint8_t func_idx = *frame->ip++;
                VSS_Value func_val = frame->closure->function->chunk.constants[func_idx];
                VSS_ObjFunction *func = func_val.as.function;
                
                VSS_ObjClosure *cls = vss_closure_new(func);
                cls->receiver = frame->closure->receiver;
                vss_value_retain(cls->receiver);
                VSS_Value cls_val = vss_value_new_closure(cls);
                push(cls_val);
                
                for (int i = 0; i < func->upvalue_count; i++) {
                    uint8_t is_local = *frame->ip++;
                    uint8_t index = *frame->ip++;
                    if (is_local) {
                        cls->upvalues[i] = capture_upvalue(&vm, frame->slots + index);
                        vss_upvalue_retain(cls->upvalues[i]);
                    } else {
                        cls->upvalues[i] = frame->closure->upvalues[index];
                        vss_upvalue_retain(cls->upvalues[i]);
                    }
                }
                break;
            }
            case VSS_OP_RETURN: {
                VSS_Value ret_val = pop(); // pop return value
                close_upvalues(&vm, frame->slots);
                
                if (frame->closure->receiver.type != VSS_VAL_EMPTY && strcmp(frame->closure->function->name, "create") == 0) {
                    vss_value_release(ret_val);
                    ret_val = frame->closure->receiver;
                    vss_value_retain(ret_val);
                }
                
                vm.frame_count--;
                if (vm.frame_count == 0) {
                    vss_value_release(pop()); // pop closure
                    push(ret_val);
                    vss_vm_free(&vm);
                    if (strcmp(func->name, "__main__") == 0) {
                        clear_module_cache();
                    }
                    return true;
                }
                
                // pop frame locals
                while (vm.stack_top > frame->slots) {
                    vss_value_release(pop());
                }
                
                push(ret_val);
                frame = &vm.frames[vm.frame_count - 1];
                break;
            }
            case VSS_OP_BUILD_LIST: {
                uint8_t count = *frame->ip++;
                VSS_Value list_val = vss_value_new_list();
                VSS_ValList *l = list_val.as.list;
                
                l->count = count;
                l->capacity = count;
                l->items = malloc(sizeof(VSS_Value) * count);
                
                // Pop items in reverse order
                for (int i = count - 1; i >= 0; i--) {
                    l->items[i] = pop();
                }
                push(list_val);
                break;
            }
            case VSS_OP_BUILD_MAP: {
                uint8_t count = *frame->ip++;
                VSS_Value map_val = vss_value_new_map();
                VSS_ValMap *m = map_val.as.map;
                
                m->count = count;
                m->capacity = count;
                m->entries = malloc(sizeof(VSS_ValMapEntry) * count);
                
                // Alternating keys and values on stack: key1, val1, key2, val2...
                // Pop them in reverse order
                for (int i = count - 1; i >= 0; i--) {
                    m->entries[i].value = pop();
                    VSS_Value key_val = pop();
                    m->entries[i].key = safe_strdup(key_val.as.string->chars);
                    vss_value_release(key_val);
                }
                push(map_val);
                break;
            }
            case VSS_OP_GET_ITEM: {
                VSS_Value idx = pop();
                VSS_Value list = pop();
                if (list.type != VSS_VAL_LIST || idx.type != VSS_VAL_NUMBER) {
                    vss_value_release(list); vss_value_release(idx);
                    runtime_error("List item access expects a list and a numeric index.");
                    return false;
                }
                long long index = (long long)idx.as.number;
                VSS_ValList *l = list.as.list;
                if (index < 0 || (size_t)index >= l->count) {
                    vss_value_release(list); vss_value_release(idx);
                    runtime_error("List index out of range (got %lld, size is %zu).", index, l->count);
                    return false;
                }
                VSS_Value item = l->items[index];
                vss_value_retain(item);
                push(item);
                vss_value_release(list); vss_value_release(idx);
                break;
            }
            case VSS_OP_PUT_ITEM: {
                VSS_Value list = pop();
                VSS_Value val = pop(); // stack has val, list
                if (list.type != VSS_VAL_LIST) {
                    vss_value_release(list); vss_value_release(val);
                    runtime_error("put statement expects a list.");
                    return false;
                }
                VSS_ValList *l = list.as.list;
                if (l->count >= l->capacity) {
                    l->capacity = l->capacity == 0 ? 8 : l->capacity * 2;
                    l->items = realloc(l->items, sizeof(VSS_Value) * l->capacity);
                }
                l->items[l->count++] = val; // ownership transferred
                vss_value_release(list);
                break;
            }
            case VSS_OP_GET_FIELD: {
                VSS_Value field = pop();
                VSS_Value receiver = pop();
                if (field.type != VSS_VAL_STRING) {
                    vss_value_release(receiver); vss_value_release(field);
                    runtime_error("Field access expects a string key.");
                    return false;
                }
                const char *key = field.as.string->chars;
                
                if (receiver.type == VSS_VAL_MAP) {
                    VSS_ValMap *m = receiver.as.map;
                    bool found = false;
                    for (size_t i = 0; i < m->count; i++) {
                        if (strcmp(m->entries[i].key, key) == 0) {
                            VSS_Value fval = m->entries[i].value;
                            vss_value_retain(fval);
                            push(fval);
                            found = true;
                            break;
                        }
                    }
                    vss_value_release(receiver); vss_value_release(field);
                    if (!found) {
                        runtime_error("Map key '%s' not found.", key);
                        return false;
                    }
                } else if (receiver.type == VSS_VAL_INSTANCE) {
                    VSS_ValMap *m = receiver.as.instance->fields;
                    bool found = false;
                    for (size_t i = 0; i < m->count; i++) {
                        if (strcmp(m->entries[i].key, key) == 0) {
                            VSS_Value fval = m->entries[i].value;
                            vss_value_retain(fval);
                            push(fval);
                            found = true;
                            break;
                        }
                    }
                    if (found) {
                        vss_value_release(receiver); vss_value_release(field);
                        break;
                    }
                    
                    VSS_ObjClass *curr = receiver.as.instance->klass;
                    VSS_Value method_val;
                    while (curr) {
                        VSS_ValMap *meths = curr->methods;
                        for (size_t i = 0; i < meths->count; i++) {
                            if (strcmp(meths->entries[i].key, key) == 0) {
                                method_val = meths->entries[i].value;
                                found = true;
                                break;
                            }
                        }
                        if (found) break;
                        curr = curr->parent;
                    }
                    
                    if (found) {
                        if (method_val.type == VSS_VAL_CLOSURE) {
                            VSS_ObjClosure *bound = vss_closure_new(method_val.as.closure->function);
                            for (int i = 0; i < bound->upvalue_count; i++) {
                                bound->upvalues[i] = method_val.as.closure->upvalues[i];
                                if (bound->upvalues[i]) {
                                    vss_upvalue_retain(bound->upvalues[i]);
                                }
                            }
                            bound->receiver = receiver;
                            vss_value_retain(receiver);
                            push(vss_value_new_closure(bound));
                        } else if (method_val.type == VSS_VAL_FUNCTION) {
                            VSS_ObjClosure *bound = vss_closure_new(method_val.as.function);
                            bound->receiver = receiver;
                            vss_value_retain(receiver);
                            push(vss_value_new_closure(bound));
                        } else {
                            vss_value_retain(method_val);
                            push(method_val);
                        }
                        vss_value_release(receiver); vss_value_release(field);
                        break;
                    }
                    
                    vss_value_release(receiver); vss_value_release(field);
                    runtime_error("Field or method '%s' not found on instance.", key);
                    return false;
                } else if (receiver.type == VSS_VAL_CLASS) {
                    VSS_ObjClass *curr = receiver.as.klass;
                    bool found = false;
                    VSS_Value method_val;
                    while (curr) {
                        VSS_ValMap *meths = curr->methods;
                        for (size_t i = 0; i < meths->count; i++) {
                            if (strcmp(meths->entries[i].key, key) == 0) {
                                method_val = meths->entries[i].value;
                                found = true;
                                break;
                            }
                        }
                        if (found) break;
                        curr = curr->parent;
                    }
                    
                    if (found) {
                        vss_value_retain(method_val);
                        push(method_val);
                        vss_value_release(receiver); vss_value_release(field);
                        break;
                    }
                    vss_value_release(receiver); vss_value_release(field);
                    runtime_error("Static method '%s' not found on class.", key);
                    return false;
                } else if (receiver.type == VSS_VAL_ENUM) {
                    VSS_ValMap *m = receiver.as.enm->members;
                    bool found = false;
                    for (size_t i = 0; i < m->count; i++) {
                        if (strcmp(m->entries[i].key, key) == 0) {
                            VSS_Value eval = m->entries[i].value;
                            vss_value_retain(eval);
                            push(eval);
                            found = true;
                            break;
                        }
                    }
                    vss_value_release(receiver); vss_value_release(field);
                    if (!found) {
                        runtime_error("Enum member '%s' not found.", key);
                        return false;
                    }
                } else {
                    vss_value_release(receiver); vss_value_release(field);
                    runtime_error("Cannot access fields on non-map, non-instance value.");
                    return false;
                }
                break;
            }
            case VSS_OP_SET_FIELD: {
                VSS_Value val = pop();
                VSS_Value field = pop();
                VSS_Value receiver = pop();
                if (field.type != VSS_VAL_STRING) {
                    vss_value_release(receiver); vss_value_release(field); vss_value_release(val);
                    runtime_error("Field setting expects a string key.");
                    return false;
                }
                const char *key = field.as.string->chars;
                
                if (receiver.type == VSS_VAL_MAP) {
                    VSS_ValMap *m = receiver.as.map;
                    bool found = false;
                    for (size_t i = 0; i < m->count; i++) {
                        if (strcmp(m->entries[i].key, key) == 0) {
                            vss_value_release(m->entries[i].value);
                            m->entries[i].value = val;
                            vss_value_retain(val);
                            found = true;
                            break;
                        }
                    }
                    if (!found) {
                        m->entries = realloc(m->entries, sizeof(VSS_ValMapEntry) * (m->count + 1));
                        m->entries[m->count].key = safe_strdup(key);
                        m->entries[m->count].value = val;
                        vss_value_retain(val);
                        m->count++;
                    }
                } else if (receiver.type == VSS_VAL_INSTANCE) {
                    VSS_ValMap *m = receiver.as.instance->fields;
                    bool found = false;
                    for (size_t i = 0; i < m->count; i++) {
                        if (strcmp(m->entries[i].key, key) == 0) {
                            vss_value_release(m->entries[i].value);
                            m->entries[i].value = val;
                            vss_value_retain(val);
                            found = true;
                            break;
                        }
                    }
                    if (!found) {
                        m->entries = realloc(m->entries, sizeof(VSS_ValMapEntry) * (m->count + 1));
                        m->entries[m->count].key = safe_strdup(key);
                        m->entries[m->count].value = val;
                        vss_value_retain(val);
                        m->count++;
                    }
                } else {
                    vss_value_release(receiver); vss_value_release(field); vss_value_release(val);
                    runtime_error("Cannot set fields on non-map, non-instance value.");
                    return false;
                }
                vss_value_release(receiver); vss_value_release(field); vss_value_release(val);
                break;
            }
            case VSS_OP_CLASS: {
                uint8_t constant = *frame->ip++;
                VSS_Value name_val = frame->closure->function->chunk.constants[constant];
                const char *name = name_val.as.string->chars;
                
                VSS_Value parent_val = pop();
                VSS_ObjClass *parent = NULL;
                if (parent_val.type != VSS_VAL_EMPTY) {
                    if (parent_val.type != VSS_VAL_CLASS) {
                        vss_value_release(parent_val);
                        runtime_error("Parent class must be a class.");
                        return false;
                    }
                    parent = parent_val.as.klass;
                }
                
                VSS_Value klass = vss_value_new_class(name, parent);
                push(klass);
                
                vss_value_release(parent_val);
                break;
            }
            case VSS_OP_ENUM: {
                uint8_t name_const = *frame->ip++;
                uint8_t member_count = *frame->ip++;
                VSS_Value name_val = frame->closure->function->chunk.constants[name_const];
                const char *enum_name = name_val.as.string->chars;
                
                VSS_Value enm = vss_value_new_enum(enum_name);
                
                for (int i = (int)member_count - 1; i >= 0; i--) {
                    VSS_Value member_val = pop();
                    const char *member_name = member_val.as.string->chars;
                    VSS_Value eval = vss_value_new_enum_val(enum_name, member_name, i);
                    
                    VSS_ValMap *m = enm.as.enm->members;
                    m->entries = realloc(m->entries, sizeof(VSS_ValMapEntry) * (m->count + 1));
                    m->entries[m->count].key = safe_strdup(member_name);
                    m->entries[m->count].value = eval;
                    vss_value_retain(eval);
                    m->count++;
                    
                    vss_value_release(member_val);
                }
                push(enm);
                break;
            }
            case VSS_OP_SET_MEMBER: {
                VSS_Value method = pop();
                VSS_Value name_val = pop();
                VSS_Value klass_val = peek(0);
                
                if (klass_val.type != VSS_VAL_CLASS) {
                    vss_value_release(method); vss_value_release(name_val);
                    runtime_error("Can only set methods on a class.");
                    return false;
                }
                
                VSS_ObjClass *klass = klass_val.as.klass;
                const char *method_name = name_val.as.string->chars;
                VSS_ValMap *m = klass->methods;
                
                bool found = false;
                for (size_t i = 0; i < m->count; i++) {
                    if (strcmp(m->entries[i].key, method_name) == 0) {
                        vss_value_release(m->entries[i].value);
                        m->entries[i].value = method;
                        vss_value_retain(method);
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    m->entries = realloc(m->entries, sizeof(VSS_ValMapEntry) * (m->count + 1));
                    m->entries[m->count].key = safe_strdup(method_name);
                    m->entries[m->count].value = method;
                    vss_value_retain(method);
                    m->count++;
                }
                vss_value_release(method);
                vss_value_release(name_val);
                break;
            }
            case VSS_OP_GET_PARENT: {
                VSS_Value name_val = pop();
                VSS_Value mine_val = pop();
                
                if (mine_val.type != VSS_VAL_INSTANCE) {
                    vss_value_release(name_val); vss_value_release(mine_val);
                    runtime_error("Parent lookup expects a 'mine' instance.");
                    return false;
                }
                
                VSS_ObjClass *parent = mine_val.as.instance->klass->parent;
                if (!parent) {
                    vss_value_release(name_val); vss_value_release(mine_val);
                    runtime_error("Class has no parent to perform parent lookup.");
                    return false;
                }
                
                const char *method_name = name_val.as.string->chars;
                VSS_Value method_val;
                bool found = false;
                VSS_ObjClass *curr = parent;
                while (curr) {
                    VSS_ValMap *m = curr->methods;
                    for (size_t i = 0; i < m->count; i++) {
                        if (strcmp(m->entries[i].key, method_name) == 0) {
                            method_val = m->entries[i].value;
                            found = true;
                            break;
                        }
                    }
                    if (found) break;
                    curr = curr->parent;
                }
                
                if (!found) {
                    vss_value_release(name_val); vss_value_release(mine_val);
                    runtime_error("Method '%s' not found in parent class.", method_name);
                    return false;
                }
                
                if (method_val.type == VSS_VAL_CLOSURE) {
                    VSS_ObjClosure *bound = vss_closure_new(method_val.as.closure->function);
                    for (int i = 0; i < bound->upvalue_count; i++) {
                        bound->upvalues[i] = method_val.as.closure->upvalues[i];
                        if (bound->upvalues[i]) {
                            vss_upvalue_retain(bound->upvalues[i]);
                        }
                    }
                    bound->receiver = mine_val;
                    vss_value_retain(mine_val);
                    push(vss_value_new_closure(bound));
                } else if (method_val.type == VSS_VAL_FUNCTION) {
                    VSS_ObjClosure *bound = vss_closure_new(method_val.as.function);
                    bound->receiver = mine_val;
                    vss_value_retain(mine_val);
                    push(vss_value_new_closure(bound));
                } else {
                    vss_value_retain(method_val);
                    push(method_val);
                }
                
                vss_value_release(name_val);
                vss_value_release(mine_val);
                break;
            }
            case VSS_OP_SIZE_OF: {
                VSS_Value val = pop();
                if (val.type == VSS_VAL_LIST) push(vss_value_new_number(val.as.list->count));
                else if (val.type == VSS_VAL_MAP) push(vss_value_new_number(val.as.map->count));
                else if (val.type == VSS_VAL_STRING) push(vss_value_new_number(strlen(val.as.string->chars)));
                else {
                    vss_value_release(val);
                    runtime_error("size of expects list, map, or string.");
                    return false;
                }
                vss_value_release(val);
                break;
            }
            case VSS_OP_ATTEMPT: {
                uint16_t offset = (uint16_t)(frame->ip[0] << 8) | frame->ip[1];
                frame->ip += 2;
                if (vm.trap_count >= VSS_TRAPS_MAX) {
                    runtime_error("Too many nested rescue attempt traps.");
                    return false;
                }
                VSS_TrapFrame *trap = &vm.traps[vm.trap_count++];
                trap->depth = vm.stack_top - vm.stack;
                trap->handler_ip = frame->ip + offset;
                break;
            }
            case VSS_OP_END_ATTEMPT: {
                if (vm.trap_count > 0) vm.trap_count--;
                break;
            }
            case VSS_OP_GRAB: {
                uint8_t name_const = *frame->ip++;
                VSS_Value name_val = frame->closure->function->chunk.constants[name_const];
                const char *module_name = name_val.as.string->chars;
                
                CachedModule *cached = get_cached_module(module_name);
                VSS_Value exports;
                
                if (cached) {
                    if (cached->is_loading) {
                        runtime_error("Circular dependency detected grabbing module '%s'.", module_name);
                        return false;
                    }
                    exports = cached->exports;
                } else {
                    char filepath[256];
                    snprintf(filepath, sizeof(filepath), "%s.vss", module_name);
                    FILE *f = fopen(filepath, "rb");
                    if (!f) {
                        snprintf(filepath, sizeof(filepath), "packages/%s.vss", module_name);
                        f = fopen(filepath, "rb");
                    }
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
                    VSS_Lexer mod_lexer;
                    vss_lexer_init(&mod_lexer, source);
                    VSS_Parser mod_parser;
                    vss_parser_init(&mod_parser, &mod_lexer);
                    VSS_Block mod_ast = vss_parse_program(&mod_parser);
                    free(source);
                    
                    if (mod_parser.had_error) {
                        vss_block_free(mod_ast);
                        runtime_error("Syntax error in module '%s'.", module_name);
                        return false;
                    }
                    
                    VSS_ObjFunction *mod_func = vss_compile_program(mod_ast);
                    vss_block_free(mod_ast);
                    
                    VSS_Env *mod_env = vss_env_new(NULL);
                    vss_register_builtins(mod_env);
                    
                    VSS_Value empty_map = vss_value_new_map();
                    add_cached_module(module_name, empty_map, true);
                    vss_value_release(empty_map);
                    
                    bool run_success = vss_vm_run(mod_func, mod_env);
                    vss_function_release(mod_func);
                    
                    if (!run_success) {
                        vss_env_release(mod_env);
                        return false;
                    }
                    
                    exports = vss_value_new_map();
                    VSS_ValMap *m = exports.as.map;
                    for (size_t i = 0; i < mod_env->count; i++) {
                        if (strncmp(mod_env->items[i].name, "__", 2) == 0) continue;
                        
                        m->entries = realloc(m->entries, sizeof(VSS_ValMapEntry) * (m->count + 1));
                        m->entries[m->count].key = safe_strdup(mod_env->items[i].name);
                        m->entries[m->count].value = mod_env->items[i].value;
                        vss_value_retain(mod_env->items[i].value);
                        m->count++;
                    }
                    
                    CachedModule *load_entry = get_cached_module(module_name);
                    vss_value_release(load_entry->exports);
                    load_entry->exports = exports;
                    vss_value_retain(exports);
                    load_entry->is_loading = false;
                    
                    vss_env_release(mod_env);
                }
                
                vss_env_define(vm.globals, module_name, exports);
                
                VSS_ValMap *m = exports.as.map;
                for (size_t i = 0; i < m->count; i++) {
                    vss_env_define(vm.globals, m->entries[i].key, m->entries[i].value);
                }
                
                break;
            }
            case VSS_OP_HI_HTMVSS:
                printf("<!DOCTYPE html>\n<html>\n<head>\n<script src=\"https://cdn.tailwindcss.com\"></script>\n</head>\n<body class=\"bg-slate-900 text-white font-sans flex flex-col items-center justify-center min-h-screen\">\n");
                break;
            case VSS_OP_BYE_HTMVSS:
                printf("</body>\n</html>\n");
                break;
            default:
                runtime_error("Unknown VSS_VM instruction %d.", instruction);
                return false;
        }
    }
}
