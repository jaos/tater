#ifndef clox_object_h
#define clox_object_h

#include "common.h"
#include "vmopcodes.h"

#define OBJ_TYPE(value) (AS_OBJ(value)->type)
#define IS_BOUND_METHOD(value) is_obj_type(value, OBJ_BOUND_METHOD)
#define IS_CLASS(value) is_obj_type(value, OBJ_CLASS)
#define IS_CLOSURE(value) is_obj_type(value, OBJ_CLOSURE)
#define IS_FUNCTION(value) is_obj_type(value, OBJ_FUNCTION)
#define IS_INSTANCE(value) is_obj_type(value, OBJ_INSTANCE)
#define IS_NATIVE(value) is_obj_type(value, OBJ_NATIVE)
#define IS_BOUND_NATIVE_METHOD(value) is_obj_type(value, OBJ_BOUND_NATIVE_METHOD)
#define IS_STRING(value) is_obj_type(value, OBJ_STRING)
#define IS_LIST(value) is_obj_type(value, OBJ_LIST)
#define IS_MAP(value) is_obj_type(value, OBJ_MAP)

#define AS_BOUND_METHOD(value) ((obj_bound_method_t*)AS_OBJ(value))
#define AS_CLASS(value) ((obj_class_t*)AS_OBJ(value))
#define AS_CLOSURE(value) ((obj_closure_t*)AS_OBJ(value))
#define AS_FUNCTION(value) ((obj_function_t*)AS_OBJ(value))
#define AS_INSTANCE(value) ((obj_instance_t*)AS_OBJ(value))
#define AS_NATIVE(value) (((obj_native_t*)AS_OBJ(value)))
#define AS_NATIVE_FN(value) (((obj_native_t*)AS_OBJ(value))->function)
#define AS_BOUND_NATIVE_METHOD(value) ((obj_bound_native_method_t*)AS_OBJ(value))

#define AS_STRING(value) ((obj_string_t*)AS_OBJ(value))
#define AS_CSTRING(value) (((obj_string_t*)AS_OBJ(value))->chars)
#define AS_LIST(value) (((obj_list_t*)AS_OBJ(value)))
#define AS_MAP(value) (((obj_map_t*)AS_OBJ(value)))

#define IS_BOOL(value)   ((value).type == VAL_BOOL)
#define IS_NIL(value)    ((value).type == VAL_NIL)
#define IS_NUMBER(value) ((value).type == VAL_NUMBER)
#define IS_OBJ(value)    ((value).type == VAL_OBJ)
#define IS_EMPTY(value)  ((value).type == VAL_EMPTY)
#define IS_TRUE(value)   (IS_BOOL(value) && AS_BOOL(value) == true)
#define IS_FALSE(value)  (IS_BOOL(value) && AS_BOOL(value) == false)

#define AS_OBJ(value)    ((value).as.obj)
#define AS_BOOL(value)   ((value).as.boolean)
#define AS_NUMBER(value) ((value).as.number)

#define BOOL_VAL(value)     ((value_t){VAL_BOOL, {.boolean = value}})
#define NIL_VAL             ((value_t){VAL_NIL, {.number = 0}})
#define NUMBER_VAL(value)   ((value_t){VAL_NUMBER, {.number = value}})
#define OBJ_VAL(object)     ((value_t){VAL_OBJ, {.obj = (obj_t*)object}})
#define EMPTY_VAL           ((value_t){VAL_EMPTY, {.number = 0}})

#define FALSE_VAL (BOOL_VAL(false))
#define TRUE_VAL (BOOL_VAL(true))

typedef enum {
    OBJ_BOUND_METHOD,
    OBJ_CLASS,
    OBJ_CLOSURE,
    OBJ_FUNCTION,
    OBJ_INSTANCE,
    OBJ_NATIVE,
    OBJ_STRING,
    OBJ_UPVALUE,
    OBJ_LIST,
    OBJ_MAP,
    OBJ_BOUND_NATIVE_METHOD,
} obj_type_t;

typedef struct obj_t {
    obj_type_t type;
    bool is_marked;
    struct obj_t *next;
} obj_t;

typedef struct obj_string_t {
    obj_t obj;
    int length;
    uint32_t hash;
    char *chars;
} obj_string_t;

typedef enum {
    VAL_BOOL,
    VAL_NIL,
    VAL_NUMBER,
    VAL_OBJ,
    VAL_EMPTY,
} value_type_t;

typedef struct {
    value_type_t type;
    union {
        bool boolean;
        double number;
        obj_t *obj;
    } as;
} value_t;

typedef struct {
    int capacity;
    int count;
    value_t *values;
} value_array_t;

typedef struct {
    int offset;
    int line;
} line_info_t;

typedef struct {
    int count;
    int capacity;
    uint8_t *code;
    value_array_t constants;
    int line_count;
    int line_capacity;
    line_info_t *lines;
} chunk_t;

typedef struct {
    value_t key;
    value_t value;
} entry_t;

typedef struct {
    int count;
    int capacity;
    entry_t *entries;
} table_t;

typedef struct {
    obj_t obj;
    int arity;
    int upvalue_count;
    chunk_t chunk;
    obj_string_t *name;
} obj_function_t;

typedef bool (*native_fn_t)(const int arg_count, const value_t *args);

typedef struct {
    obj_t obj;
    int arity;
    const obj_string_t *name;
    native_fn_t function;
} obj_native_t;

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

typedef  bool (*native_method_fn_t)(const obj_string_t *method, const int arg_count, const value_t *args);

typedef struct {
    obj_t obj;
    obj_string_t *name;
    value_t receiving_instance;
    native_method_fn_t function;
} obj_bound_native_method_t;

typedef struct {
    obj_t obj;
    value_array_t elements;
} obj_list_t;

typedef struct {
    obj_t obj;
    table_t table;
} obj_map_t;

obj_bound_method_t *obj_bound_method_t_allocate(value_t receiving_instance, obj_closure_t *method);
obj_bound_native_method_t * obj_bound_native_method_t_allocate(value_t receiving_instance, obj_string_t *name, native_method_fn_t function);
obj_function_t *obj_function_t_allocate(void);
obj_native_t *obj_native_t_allocate(native_fn_t function, const obj_string_t *name, const int arity);
obj_closure_t *obj_closure_t_allocate(obj_function_t *function);
obj_upvalue_t *obj_upvalue_t_allocate(value_t *slot);
obj_class_t *obj_class_t_allocate(obj_string_t *name);
obj_instance_t *obj_instance_t_allocate(obj_class_t *cls);
obj_list_t *obj_list_t_allocate(void);
obj_map_t *obj_map_t_allocate(void);

obj_string_t *obj_string_t_copy_own(char *chars, const int length);
obj_string_t *obj_string_t_copy_from(const char *chars, const int length);
void obj_t_print(const value_t value);
obj_string_t *obj_t_to_obj_string_t(const value_t value);
void obj_t_mark(obj_t *obj);

static inline bool is_obj_type(const value_t value, const obj_type_t type)
{
    return IS_OBJ(value) && AS_OBJ(value)->type == type;
}

bool value_t_equal(const value_t a, const value_t b);
void value_array_t_init(value_array_t *array);
void value_array_t_add(value_array_t *array, const value_t value);
void value_array_t_free(value_array_t *array);
void value_t_print(const value_t value);
obj_string_t *value_t_to_obj_string_t(const value_t value);
uint32_t value_t_hash(const value_t value);
void value_t_mark(value_t value);

void table_t_init(table_t *table);
void table_t_free(table_t *table);
bool table_t_set(table_t *table, value_t key, const value_t value);
bool table_t_get(table_t *table, const value_t key, value_t *value);
bool table_t_delete(table_t *table, const value_t key);
obj_string_t *table_t_find_key_by_str(const table_t *table, const char *chars, const int length, const uint32_t hash);
void table_t_remove_white(table_t *table);
void table_t_mark(table_t *table);
void table_t_copy_to(const table_t *from, table_t *to);

void chunk_t_init(chunk_t *chunk);
void chunk_t_free(chunk_t *chunk);
void chunk_t_write(chunk_t *chunk, const uint8_t byte, const int line);
int chunk_t_add_constant(chunk_t *chunk, const value_t value);
int chunk_t_get_line(const chunk_t *chunk, const int instruction);

#endif
