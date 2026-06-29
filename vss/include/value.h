#ifndef VSS_VALUE_H
#define VSS_VALUE_H

#include "common.h"

typedef enum {
    VAL_NUMBER,
    VAL_STRING,
    VAL_BOOL,
    VAL_EMPTY,
    VAL_LIST,
    VAL_MAP,
    VAL_TASK,
    VAL_NATIVE,
    VAL_CLOSURE,
    VAL_FUNCTION
} ValueType;

typedef struct ValString ValString;
typedef struct ValList ValList;
typedef struct ValMap ValMap;
typedef struct ValTask ValTask;
typedef struct ObjClosure ObjClosure;
typedef struct ObjFunction ObjFunction;
struct Stmt;
struct Env;

typedef struct Value (*NativeFnPtr)(size_t arg_count, struct Value *args, bool *out_error, char **out_error_msg);

typedef struct Value {
    ValueType type;
    union {
        double number;
        bool boolean;
        ValString *string;
        ValList *list;
        ValMap *map;
        ValTask *task;
        NativeFnPtr native;
        ObjClosure *closure;
        ObjFunction *function;
    } as;
} Value;

struct ValString {
    int ref_count;
    char *chars;
};

struct ValList {
    int ref_count;
    Value *items;
    size_t count;
    size_t capacity;
};

typedef struct {
    char *key;
    Value value;
} ValMapEntry;

struct ValMap {
    int ref_count;
    ValMapEntry *entries;
    size_t count;
    size_t capacity;
};

struct ValTask {
    int ref_count;
    char **params;
    size_t param_count;
    struct Stmt **body;
    size_t body_count;
    struct Env *closure;
};

// Constructors
Value value_new_number(double n);
Value value_new_string(const char *s);
Value value_new_bool(bool b);
Value value_new_empty(void);
Value value_new_list(void);
Value value_new_map(void);
Value value_new_task(char **params, size_t param_count, struct Stmt **body, size_t body_count, struct Env *closure);
Value value_new_native(NativeFnPtr func);
Value value_new_closure(ObjClosure *closure);
Value value_new_function(ObjFunction *func);

// Reference counting
void value_retain(Value v);
void value_release(Value v);

// Utility functions
bool value_same_as(Value a, Value b);
bool value_truthy(Value v);
void value_say(Value v);
char *value_to_string(Value v);

#endif
