#ifndef VSS_OBJECT_H
#define VSS_OBJECT_H

#include "common.h"
#include "value.h"
#include "chunk.h"

typedef struct ObjFunction {
    int ref_count;
    char *name;
    size_t param_count;
    Chunk chunk;
    int upvalue_count;
} ObjFunction;

typedef struct Upvalue Upvalue;
struct Upvalue {
    int ref_count;
    Value *location;  // Points to slot on stack (if open) or closed_value (if closed)
    Value closed_value;
    Upvalue *next;    // Linked list of open upvalues
};

typedef struct ObjClosure {
    int ref_count;
    ObjFunction *function;
    Upvalue **upvalues;
    int upvalue_count;
} ObjClosure;

ObjFunction *function_new(const char *name, size_t param_count);
void function_retain(ObjFunction *func);
void function_release(ObjFunction *func);

Upvalue *upvalue_new(Value *slot);
void upvalue_retain(Upvalue *uv);
void upvalue_release(Upvalue *uv);

ObjClosure *closure_new(ObjFunction *func);
void closure_retain(ObjClosure *closure);
void closure_release(ObjClosure *closure);

#endif
