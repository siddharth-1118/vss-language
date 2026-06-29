#include <stdlib.h>
#include <string.h>
#include "object.h"

// Helper to duplicate string safely
static char *safe_strdup(const char *s) {
    if (!s) return NULL;
    char *dup = malloc(strlen(s) + 1);
    if (dup) {
        strcpy(dup, s);
    }
    return dup;
}

ObjFunction *function_new(const char *name, size_t param_count) {
    ObjFunction *func = malloc(sizeof(ObjFunction));
    func->ref_count = 1;
    func->name = safe_strdup(name);
    func->param_count = param_count;
    func->upvalue_count = 0;
    chunk_init(&func->chunk);
    return func;
}

void function_retain(ObjFunction *func) {
    if (!func) return;
    func->ref_count++;
}

void function_release(ObjFunction *func) {
    if (!func) return;
    func->ref_count--;
    if (func->ref_count == 0) {
        free(func->name);
        chunk_free(&func->chunk);
        free(func);
    }
}

Upvalue *upvalue_new(Value *slot) {
    Upvalue *uv = malloc(sizeof(Upvalue));
    uv->ref_count = 1;
    uv->location = slot;
    uv->closed_value = value_new_empty();
    uv->next = NULL;
    return uv;
}

void upvalue_retain(Upvalue *uv) {
    if (!uv) return;
    uv->ref_count++;
}

void upvalue_release(Upvalue *uv) {
    if (!uv) return;
    uv->ref_count--;
    if (uv->ref_count == 0) {
        value_release(uv->closed_value);
        free(uv);
    }
}

ObjClosure *closure_new(ObjFunction *func) {
    ObjClosure *closure = malloc(sizeof(ObjClosure));
    closure->ref_count = 1;
    closure->function = func;
    function_retain(func);
    
    closure->upvalue_count = func->upvalue_count;
    if (closure->upvalue_count > 0) {
        closure->upvalues = malloc(sizeof(Upvalue*) * closure->upvalue_count);
        for (int i = 0; i < closure->upvalue_count; i++) {
            closure->upvalues[i] = NULL;
        }
    } else {
        closure->upvalues = NULL;
    }
    return closure;
}

void closure_retain(ObjClosure *closure) {
    if (!closure) return;
    closure->ref_count++;
}

void closure_release(ObjClosure *closure) {
    if (!closure) return;
    closure->ref_count--;
    if (closure->ref_count == 0) {
        function_release(closure->function);
        if (closure->upvalue_count > 0) {
            for (int i = 0; i < closure->upvalue_count; i++) {
                if (closure->upvalues[i]) {
                    upvalue_release(closure->upvalues[i]);
                }
            }
            free(closure->upvalues);
        }
        free(closure);
    }
}
