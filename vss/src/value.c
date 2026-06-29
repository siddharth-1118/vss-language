#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "value.h"
#include "env.h"
#include "ast.h"
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

Value value_new_number(double n) {
    Value v;
    v.type = VAL_NUMBER;
    v.as.number = n;
    return v;
}

Value value_new_string(const char *s) {
    Value v;
    v.type = VAL_STRING;
    v.as.string = malloc(sizeof(ValString));
    v.as.string->ref_count = 1;
    v.as.string->chars = safe_strdup(s);
    return v;
}

Value value_new_bool(bool b) {
    Value v;
    v.type = VAL_BOOL;
    v.as.boolean = b;
    return v;
}

Value value_new_empty(void) {
    Value v;
    v.type = VAL_EMPTY;
    return v;
}

Value value_new_list(void) {
    Value v;
    v.type = VAL_LIST;
    v.as.list = malloc(sizeof(ValList));
    v.as.list->ref_count = 1;
    v.as.list->items = NULL;
    v.as.list->count = 0;
    v.as.list->capacity = 0;
    return v;
}

Value value_new_map(void) {
    Value v;
    v.type = VAL_MAP;
    v.as.map = malloc(sizeof(ValMap));
    v.as.map->ref_count = 1;
    v.as.map->entries = NULL;
    v.as.map->count = 0;
    v.as.map->capacity = 0;
    return v;
}

Value value_new_task(char **params, size_t param_count, struct Stmt **body, size_t body_count, struct Env *closure) {
    Value v;
    v.type = VAL_TASK;
    v.as.task = malloc(sizeof(ValTask));
    v.as.task->ref_count = 1;
    
    v.as.task->param_count = param_count;
    v.as.task->params = malloc(sizeof(char*) * param_count);
    for (size_t i = 0; i < param_count; i++) {
        v.as.task->params[i] = safe_strdup(params[i]);
    }
    
    v.as.task->body_count = body_count;
    v.as.task->body = malloc(sizeof(struct Stmt*) * body_count);
    for (size_t i = 0; i < body_count; i++) {
        v.as.task->body[i] = body[i]; // Statements are owned by AST
    }
    
    v.as.task->closure = closure;
    if (closure) {
        env_retain(closure);
    }
    
    return v;
}

Value value_new_native(NativeFnPtr func) {
    Value v;
    v.type = VAL_NATIVE;
    v.as.native = func;
    return v;
}

Value value_new_closure(ObjClosure *closure) {
    Value v;
    v.type = VAL_CLOSURE;
    v.as.closure = closure;
    return v;
}

Value value_new_function(ObjFunction *func) {
    Value v;
    v.type = VAL_FUNCTION;
    v.as.function = func;
    return v;
}

void value_retain(Value v) {
    switch (v.type) {
        case VAL_STRING:
            v.as.string->ref_count++;
            break;
        case VAL_LIST:
            v.as.list->ref_count++;
            break;
        case VAL_MAP:
            v.as.map->ref_count++;
            break;
        case VAL_TASK:
            v.as.task->ref_count++;
            break;
        case VAL_CLOSURE:
            closure_retain(v.as.closure);
            break;
        case VAL_FUNCTION:
            function_retain(v.as.function);
            break;
        default:
            break;
    }
}

void value_release(Value v) {
    switch (v.type) {
        case VAL_STRING:
            v.as.string->ref_count--;
            if (v.as.string->ref_count == 0) {
                free(v.as.string->chars);
                free(v.as.string);
            }
            break;
        case VAL_LIST:
            v.as.list->ref_count--;
            if (v.as.list->ref_count == 0) {
                for (size_t i = 0; i < v.as.list->count; i++) {
                    value_release(v.as.list->items[i]);
                }
                free(v.as.list->items);
                free(v.as.list);
            }
            break;
        case VAL_MAP:
            v.as.map->ref_count--;
            if (v.as.map->ref_count == 0) {
                for (size_t i = 0; i < v.as.map->count; i++) {
                    free(v.as.map->entries[i].key);
                    value_release(v.as.map->entries[i].value);
                }
                free(v.as.map->entries);
                free(v.as.map);
            }
            break;
        case VAL_TASK:
            v.as.task->ref_count--;
            if (v.as.task->ref_count == 0) {
                for (size_t i = 0; i < v.as.task->param_count; i++) {
                    free(v.as.task->params[i]);
                }
                free(v.as.task->params);
                free(v.as.task->body);
                if (v.as.task->closure) {
                    env_release(v.as.task->closure);
                }
                free(v.as.task);
            }
            break;
        case VAL_CLOSURE:
            closure_release(v.as.closure);
            break;
        case VAL_FUNCTION:
            function_release(v.as.function);
            break;
        default:
            break;
    }
}

bool value_same_as(Value a, Value b) {
    if (a.type != b.type) return false;
    switch (a.type) {
        case VAL_NUMBER:
            return a.as.number == b.as.number;
        case VAL_STRING:
            return strcmp(a.as.string->chars, b.as.string->chars) == 0;
        case VAL_BOOL:
            return a.as.boolean == b.as.boolean;
        case VAL_EMPTY:
            return true;
        case VAL_LIST:
            if (a.as.list == b.as.list) return true;
            if (a.as.list->count != b.as.list->count) return false;
            for (size_t i = 0; i < a.as.list->count; i++) {
                if (!value_same_as(a.as.list->items[i], b.as.list->items[i])) return false;
            }
            return true;
        case VAL_MAP:
            if (a.as.map == b.as.map) return true;
            if (a.as.map->count != b.as.map->count) return false;
            for (size_t i = 0; i < a.as.map->count; i++) {
                // Find matching key in b
                bool found = false;
                for (size_t j = 0; j < b.as.map->count; j++) {
                    if (strcmp(a.as.map->entries[i].key, b.as.map->entries[j].key) == 0) {
                        if (!value_same_as(a.as.map->entries[i].value, b.as.map->entries[j].value)) {
                            return false;
                        }
                        found = true;
                        break;
                    }
                }
                if (!found) return false;
            }
            return true;
        case VAL_TASK:
            return a.as.task == b.as.task;
        case VAL_NATIVE:
            return a.as.native == b.as.native;
        case VAL_CLOSURE:
            return a.as.closure == b.as.closure;
        case VAL_FUNCTION:
            return a.as.function == b.as.function;
    }
    return false;
}

bool value_truthy(Value v) {
    switch (v.type) {
        case VAL_BOOL:
            return v.as.boolean;
        case VAL_EMPTY:
            return false;
        case VAL_NUMBER:
            return v.as.number != 0.0;
        case VAL_STRING:
            return strlen(v.as.string->chars) > 0;
        case VAL_LIST:
            return v.as.list->count > 0;
        default:
            return true;
    }
}

char *value_to_string(Value v) {
    char buffer[256];
    switch (v.type) {
        case VAL_NUMBER:
            // Format number cleanly
            snprintf(buffer, sizeof(buffer), "%g", v.as.number);
            return safe_strdup(buffer);
        case VAL_STRING:
            return safe_strdup(v.as.string->chars);
        case VAL_BOOL:
            return safe_strdup(v.as.boolean ? "yes" : "no");
        case VAL_EMPTY:
            return safe_strdup("empty");
        case VAL_LIST: {
            // Estimate size and build string
            size_t total_len = 3; // "[" + "]" + null
            for (size_t i = 0; i < v.as.list->count; i++) {
                char *s = value_to_string(v.as.list->items[i]);
                total_len += strlen(s) + 2; // string + ", "
                free(s);
            }
            char *result = malloc(total_len);
            strcpy(result, "[");
            for (size_t i = 0; i < v.as.list->count; i++) {
                char *s = value_to_string(v.as.list->items[i]);
                strcat(result, s);
                free(s);
                if (i < v.as.list->count - 1) {
                    strcat(result, ", ");
                }
            }
            strcat(result, "]");
            return result;
        }
        case VAL_MAP: {
            size_t total_len = 8; // "map [ " + "]" + null
            for (size_t i = 0; i < v.as.map->count; i++) {
                char *val_str = value_to_string(v.as.map->entries[i].value);
                total_len += strlen(v.as.map->entries[i].key) + strlen(val_str) + 6; // '"' + key + '": ' + val_str + ' '
                free(val_str);
            }
            char *result = malloc(total_len);
            strcpy(result, "map [ ");
            for (size_t i = 0; i < v.as.map->count; i++) {
                strcat(result, "\"");
                strcat(result, v.as.map->entries[i].key);
                strcat(result, "\": ");
                char *val_str = value_to_string(v.as.map->entries[i].value);
                strcat(result, val_str);
                free(val_str);
                strcat(result, " ");
            }
            strcat(result, "]");
            return result;
        }
        case VAL_TASK:
            return safe_strdup("<task>");
        case VAL_NATIVE:
            return safe_strdup("<native task>");
        case VAL_CLOSURE:
            return safe_strdup("<task>");
        case VAL_FUNCTION:
            return safe_strdup("<task>");
    }
    return safe_strdup("<unknown>");
}

void value_say(Value v) {
    if (v.type == VAL_STRING) {
        // Spec 10: "say prints a readable text form of the value followed by a newline."
        // For string, this is its raw characters.
        printf("%s\n", v.as.string->chars);
    } else {
        char *s = value_to_string(v);
        printf("%s\n", s);
        free(s);
    }
}
