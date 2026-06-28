#include <stdlib.h>
#include <string.h>

#include "env.h"

// Helper to duplicate string safely
static char *safe_strdup(const char *s) {
    if (!s) return NULL;
    char *dup = malloc(strlen(s) + 1);
    if (dup) {
        strcpy(dup, s);
    }
    return dup;
}

Env *env_new(Env *parent) {
    Env *env = malloc(sizeof(Env));
    if (env) {
        env->ref_count = 1;
        env->items = NULL;
        env->count = 0;
        env->capacity = 0;
        env->parent = parent;
        if (parent) {
            env_retain(parent);
        }
    }
    return env;
}

void env_retain(Env *env) {
    if (env) {
        env->ref_count++;
    }
}

void env_release(Env *env) {
    if (!env) return;
    env->ref_count--;
    if (env->ref_count == 0) {
        for (size_t i = 0; i < env->count; i++) {
            free(env->items[i].name);
            value_release(env->items[i].value);
        }
        free(env->items);
        Env *parent = env->parent;
        free(env);
        if (parent) {
            env_release(parent);
        }
    }
}

bool env_define(Env *env, const char *name, Value value) {
    if (env_exists_local(env, name)) {
        return false; // Error: duplicate definition in same scope
    }
    
    if (env->count >= env->capacity) {
        env->capacity = env->capacity == 0 ? 8 : env->capacity * 2;
        env->items = realloc(env->items, sizeof(Binding) * env->capacity);
    }
    
    Binding *b = &env->items[env->count++];
    b->name = safe_strdup(name);
    b->value = value;
    b->is_constant = false;
    value_retain(value);
    return true;
}

bool env_define_const(Env *env, const char *name, Value value) {
    if (env_exists_local(env, name)) {
        return false;
    }
    
    if (env->count >= env->capacity) {
        env->capacity = env->capacity == 0 ? 8 : env->capacity * 2;
        env->items = realloc(env->items, sizeof(Binding) * env->capacity);
    }
    
    Binding *b = &env->items[env->count++];
    b->name = safe_strdup(name);
    b->value = value;
    b->is_constant = true;
    value_retain(value);
    return true;
}

bool env_assign(Env *env, const char *name, Value value) {
    Env *current = env;
    while (current) {
        for (size_t i = 0; i < current->count; i++) {
            if (strcmp(current->items[i].name, name) == 0) {
                if (current->items[i].is_constant) {
                    return false; // Error: assigning to constant
                }
                value_release(current->items[i].value);
                current->items[i].value = value;
                value_retain(value);
                return true;
            }
        }
        current = current->parent;
    }
    return false; // Error: not defined
}

bool env_get(Env *env, const char *name, Value *out_value) {
    Env *current = env;
    while (current) {
        for (size_t i = 0; i < current->count; i++) {
            if (strcmp(current->items[i].name, name) == 0) {
                *out_value = current->items[i].value;
                value_retain(*out_value);
                return true;
            }
        }
        current = current->parent;
    }
    return false;
}

bool env_exists_local(Env *env, const char *name) {
    if (!env) return false;
    for (size_t i = 0; i < env->count; i++) {
        if (strcmp(env->items[i].name, name) == 0) {
            return true;
        }
    }
    return false;
}
