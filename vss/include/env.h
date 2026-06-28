#ifndef VSS_ENV_H
#define VSS_ENV_H

#include "common.h"
#include "value.h"

typedef struct {
    char *name;
    Value value;
    bool is_constant;
} Binding;

typedef struct Env {
    int ref_count;
    Binding *items;
    size_t count;
    size_t capacity;
    struct Env *parent;
} Env;

Env *env_new(Env *parent);
void env_retain(Env *env);
void env_release(Env *env);

bool env_define(Env *env, const char *name, Value value);
bool env_define_const(Env *env, const char *name, Value value);
bool env_assign(Env *env, const char *name, Value value);
bool env_get(Env *env, const char *name, Value *out_value);
bool env_exists_local(Env *env, const char *name);

#endif
