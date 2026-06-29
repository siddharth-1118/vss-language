#ifndef VSS_VM_H
#define VSS_VM_H

#include <setjmp.h>
#include "common.h"
#include "value.h"
#include "object.h"
#include "env.h"

#define STACK_MAX 256
#define FRAMES_MAX 64
#define TRAPS_MAX 16

typedef struct {
    ObjClosure *closure;
    uint8_t *ip;
    Value *slots;
} CallFrame;

typedef struct {
    int depth;
    uint8_t *handler_ip;
} TrapFrame;

typedef struct {
    CallFrame frames[FRAMES_MAX];
    int frame_count;
    
    Value stack[STACK_MAX];
    Value *stack_top;
    
    TrapFrame traps[TRAPS_MAX];
    int trap_count;
    
    Upvalue *open_upvalues;
    Env *globals;
    jmp_buf jump_buffer;
} VM;

void vm_init(VM *vm, Env *global_env);
void vm_free(VM *vm);
bool vm_run(ObjFunction *func, Env *global_env);

#endif
