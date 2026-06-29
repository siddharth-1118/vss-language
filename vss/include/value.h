#ifndef VSS_VALUE_H
#define VSS_VALUE_H

#include "common.h"

typedef enum {
    VSS_VAL_NUMBER,
    VSS_VAL_STRING,
    VSS_VAL_BOOL,
    VSS_VAL_EMPTY,
    VSS_VAL_LIST,
    VSS_VAL_MAP,
    VSS_VAL_TASK,
    VSS_VAL_NATIVE,
    VSS_VAL_CLOSURE,
    VSS_VAL_FUNCTION
} VSS_ValueType;

typedef struct VSS_ValString VSS_ValString;
typedef struct VSS_ValList VSS_ValList;
typedef struct VSS_ValMap VSS_ValMap;
typedef struct VSS_ValTask VSS_ValTask;
typedef struct VSS_ObjClosure VSS_ObjClosure;
typedef struct VSS_ObjFunction VSS_ObjFunction;
struct VSS_Stmt;
struct VSS_Env;

typedef struct VSS_Value (*VSS_NativeFnPtr)(size_t arg_count, struct VSS_Value *args, bool *out_error, char **out_error_msg);

typedef struct VSS_Value {
    VSS_ValueType type;
    union {
        double number;
        bool boolean;
        VSS_ValString *string;
        VSS_ValList *list;
        VSS_ValMap *map;
        VSS_ValTask *task;
        VSS_NativeFnPtr native;
        VSS_ObjClosure *closure;
        VSS_ObjFunction *function;
    } as;
} VSS_Value;

struct VSS_ValString {
    int ref_count;
    char *chars;
};

struct VSS_ValList {
    int ref_count;
    VSS_Value *items;
    size_t count;
    size_t capacity;
};

typedef struct {
    char *key;
    VSS_Value value;
} VSS_ValMapEntry;

struct VSS_ValMap {
    int ref_count;
    VSS_ValMapEntry *entries;
    size_t count;
    size_t capacity;
};

struct VSS_ValTask {
    int ref_count;
    char **params;
    size_t param_count;
    struct VSS_Stmt **body;
    size_t body_count;
    struct VSS_Env *closure;
};

// Constructors
VSS_Value vss_value_new_number(double n);
VSS_Value vss_value_new_string(const char *s);
VSS_Value vss_value_new_bool(bool b);
VSS_Value vss_value_new_empty(void);
VSS_Value vss_value_new_list(void);
VSS_Value vss_value_new_map(void);
VSS_Value vss_value_new_task(char **params, size_t param_count, struct VSS_Stmt **body, size_t body_count, struct VSS_Env *closure);
VSS_Value vss_value_new_native(VSS_NativeFnPtr func);
VSS_Value vss_value_new_closure(VSS_ObjClosure *closure);
VSS_Value vss_value_new_function(VSS_ObjFunction *func);

// Reference counting
void vss_value_retain(VSS_Value v);
void vss_value_release(VSS_Value v);

// Utility functions
bool vss_value_same_as(VSS_Value a, VSS_Value b);
bool vss_value_truthy(VSS_Value v);
void vss_value_say(VSS_Value v);
char *vss_value_to_string(VSS_Value v);

#endif
