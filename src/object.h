#ifndef clox_object_h
#define clox_object_h

#include "common.h"
#include "chunk.h"
#include "table.h"
#include "value.h"

#define OBJ_TYPE(value) (AS_OBJ(value)->type)
#define IS_BOUND_METHOD(value) is_obj_type(value, OBJ_BOUND_METHOD)
#define IS_CLASS(value) is_obj_type(value, OBJ_CLASS)
#define IS_CLOSURE(value) is_obj_type(value, OBJ_CLOSURE)
#define IS_FUNCTION(value) is_obj_type(value, OBJ_FUNCTION)
#define IS_INSTANCE(value) is_obj_type(value, OBJ_INSTANCE)
#define IS_NATIVE(value) is_obj_type(value, OBJ_NATIVE)
#define IS_STRING(value) is_obj_type(value, OBJ_STRING)

#define AS_BOUND_METHOD(value) ((obj_bound_method_t*)AS_OBJ(value))
#define AS_CLASS(value) ((obj_class_t*)AS_OBJ(value))
#define AS_CLOSURE(value) ((obj_closure_t*)AS_OBJ(value))
#define AS_FUNCTION(value) ((obj_function_t*)AS_OBJ(value))
#define AS_INSTANCE(value) ((obj_instance_t*)AS_OBJ(value))
#define AS_NATIVE(value) (((obj_native_t*)AS_OBJ(value))->function)
#define AS_STRING(value) ((obj_string_t*)AS_OBJ(value))
#define AS_CSTRING(value) (((obj_string_t*)AS_OBJ(value))->chars)

typedef enum {
    OBJ_BOUND_METHOD,
    OBJ_CLASS,
    OBJ_CLOSURE,
    OBJ_FUNCTION,
    OBJ_INSTANCE,
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

typedef value_t (*native_fn_t)(const int arg_count, const value_t *args);
typedef struct {
    obj_t obj;
    native_fn_t function;
} obj_native_t;

struct obj_string_t {
    obj_t obj;
    int length;
    uint32_t hash;
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

typedef struct {
    obj_t obj;
    obj_string_t *name;
    table_t methods;
} obj_class_t;

typedef struct {
    obj_t obj;
    obj_class_t *cls;
    table_t fields;
} obj_instance_t;

typedef struct {
    obj_t obj;
    value_t receiving_instance;
    obj_closure_t *method;
} obj_bound_method_t;

obj_bound_method_t *obj_bound_method_t_allocate(value_t receiving_instance, obj_closure_t *method);
obj_function_t *obj_function_t_allocate(void);
obj_native_t *obj_native_t_allocate(native_fn_t function);
obj_closure_t *obj_closure_t_allocate(obj_function_t *function);
obj_upvalue_t *obj_upvalue_t_allocate(value_t *slot);
obj_class_t *obj_class_t_allocate(obj_string_t *name);
obj_instance_t *obj_instance_t_allocate(obj_class_t *cls);

obj_string_t *obj_string_t_copy_own(char *chars, const int length);
obj_string_t *obj_string_t_copy_from(const char *chars, const int length);
void obj_t_print(const value_t value);

static inline bool is_obj_type(const value_t value, const obj_type_t type)
{
    return IS_OBJ(value) && AS_OBJ(value)->type == type;
}
#endif
