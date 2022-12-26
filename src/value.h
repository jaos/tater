#ifndef clox_value_h
#define clox_value_h

#include <stdbool.h>

#include "common.h"

typedef struct obj_t obj_t;
typedef struct obj_string_t obj_string_t;

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

#define IS_BOOL(value)   ((value).type == VAL_BOOL)
#define IS_NIL(value)    ((value).type == VAL_NIL)
#define IS_NUMBER(value) ((value).type == VAL_NUMBER)
#define IS_OBJ(value)    ((value).type == VAL_OBJ)
#define IS_EMPTY(value)  ((value).type == VAL_EMPTY)

#define AS_OBJ(value)    ((value).as.obj)
#define AS_BOOL(value)   ((value).as.boolean)
#define AS_NUMBER(value) ((value).as.number)

#define BOOL_VAL(value)     ((value_t){VAL_BOOL, {.boolean = value}})
#define NIL_VAL             ((value_t){VAL_NIL, {.number = 0}})
#define NUMBER_VAL(value)   ((value_t){VAL_NUMBER, {.number = value}})
#define OBJ_VAL(object)     ((value_t){VAL_OBJ, {.obj = (obj_t*)object}})
#define EMPTY_VAL           ((value_t){VAL_EMPTY, {.number = 0}})

typedef struct {
    int capacity;
    int count;
    value_t *values;
} value_array_t;

bool values_equal(const value_t a, const value_t b);
void init_value_array_t(value_array_t *array);
void write_value_array_t(value_array_t *array, const value_t value);
void free_value_array_t(value_array_t *array);
void print_value(const value_t value);
uint32_t hash_value(const value_t value);

#endif
