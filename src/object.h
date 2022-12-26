#ifndef clox_object_h
#define clox_object_h

#include "common.h"
#include "chunk.h"
#include "value.h"

#define OBJ_TYPE(value) (AS_OBJ(value)->type)
#define IS_CLOSURE(value) (is_obj_type(value, OBJ_CLOSURE))
#define IS_FUNCTION(value) is_obj_type(value, OBJ_FUNCTION)
#define IS_NATIVE(value) is_obj_type(value, OBJ_NATIVE)
#define IS_STRING(value) is_obj_type(value, OBJ_STRING)

#define AS_CLOSURE(value) ((obj_closure_t*)AS_OBJ(value))
#define AS_FUNCTION(value) ((obj_function_t*)AS_OBJ(value))
#define AS_NATIVE(value) (((obj_native_t*)AS_OBJ(value))->function)
#define AS_STRING(value) ((obj_string_t*)AS_OBJ(value))
#define AS_CSTRING(value) (((obj_string_t*)AS_OBJ(value))->chars)

typedef enum {
    OBJ_CLOSURE,
    OBJ_FUNCTION,
    OBJ_NATIVE,
    OBJ_STRING,
    OBJ_UPVALUE,
} obj_type_t;

struct obj_t {
    obj_type_t type;
    bool is_marked;
    struct obj_t *next;
};

typedef struct {
    obj_t obj;
    int arity;
    int upvalue_count;
    chunk_t chunk;
    obj_string_t *name;
} obj_function_t;

typedef value_t (*native_fn_t)(int arg_count, value_t *args);
typedef struct {
    obj_t obj;
    native_fn_t function;
} obj_native_t;

struct obj_string_t {
    obj_t obj;
    int length;
    uint32_t hash;
    // char chars[]; // FAM
    char *chars;
};

typedef struct obj_upvalue {
    obj_t obj;
    value_t *location;
    value_t closed;
    struct obj_upvalue *next;
} obj_upvalue_t;

typedef struct {
    obj_t obj;
    obj_function_t *function;
    obj_upvalue_t **upvalues;
    int upvalue_count;
} obj_closure_t;

obj_function_t *new_obj_function_t(void);
obj_native_t *new_obj_native_t(native_fn_t function);
obj_closure_t *new_obj_closure_t(obj_function_t *function);
obj_upvalue_t *new_obj_upvalue_t(value_t *slot);

obj_string_t *take_string(char *chars, const int length);
obj_string_t *copy_string(const char *chars, const int length);
//FAM obj_string_t *concatenate_string(const obj_string_t *a, const obj_string_t *b);
void print_object(const value_t value);

static inline bool is_obj_type(const value_t value, const obj_type_t type)
{
    return IS_OBJ(value) && AS_OBJ(value)->type == type;
}
#endif
