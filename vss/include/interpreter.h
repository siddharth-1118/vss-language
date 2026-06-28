#ifndef VSS_INTERPRETER_H
#define VSS_INTERPRETER_H

#include "ast.h"
#include "env.h"
#include "value.h"

typedef enum {
    FLOW_NORMAL,
    FLOW_SEND,
    FLOW_LEAVE,
    FLOW_SKIP,
    FLOW_ERROR
} FlowType;

typedef struct {
    FlowType type;
    Value value;     // For FLOW_SEND
    char *error_msg; // For FLOW_ERROR
    int line;
    int column;
} FlowResult;

FlowResult interpret(Block block, Env *env);
void register_builtins(Env *env);

#endif
