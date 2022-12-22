#ifndef clox_object_h
#define clox_object_h

#include "common.h"
#include "value.h"

#define OBJ_TYPE(value) (AS_OBJ(value)->type)
#define IS_STRING(value) is_obj_type(value, OBJ_STRING)

#define AS_STRING(value) ((obj_string_t*)AS_OBJ(value))
#define AS_CSTRING(value) (((obj_string_t*)AS_OBJ(value))->chars)

typedef enum {
    OBJ_STRING,
} obj_type_t;

struct obj_t {
    obj_type_t type;
    struct obj_t *next;
};

struct obj_string_t {
    obj_t obj;
    int length;
    uint32_t hash;
    char chars[]; // FAM
};


obj_string_t *copy_string(const char *chars, const int length);
obj_string_t *concatenate_string(const obj_string_t *a, const obj_string_t *b);
void print_object(const value_t value);

static inline bool is_obj_type(const value_t value, const obj_type_t type)
{
    return IS_OBJ(value) && AS_OBJ(value)->type == type;
}
#endif
