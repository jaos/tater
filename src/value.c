#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "memory.h"
#include "object.h"
#include "value.h"

bool values_equal(const value_t a, const value_t b)
{
    if (a.type != b.type) return false;
    switch (a.type) {
        case VAL_BOOL: return AS_BOOL(a) == AS_BOOL(b);
        case VAL_NIL: return true;
        case VAL_NUMBER: return AS_NUMBER(a) == AS_NUMBER(b);
        case VAL_OBJ: return AS_OBJ(a) == AS_OBJ(b);
        case VAL_EMPTY: return true;
        default: return false; // unreachable
    }
}

static uint32_t hash_double(const double value)
{
    union bitcast {
        double value;
        uint32_t ints[2];
    };
    union bitcast cast;
    cast.value = (value) + 1.0;
    return cast.ints[0] + cast.ints[1];
}

uint32_t hash_value(const value_t value)
{
    switch (value.type) {
        case VAL_BOOL: return AS_BOOL(value) ? 3 : 5; // arbitrary hash values
        case VAL_NIL: return 7; // arbitrary hash value
        case VAL_NUMBER: return hash_double(AS_NUMBER(value));
        case VAL_OBJ: return AS_STRING(value)->hash;
        case VAL_EMPTY: return 0; // arbitrary hash value
        default: return 0; // unreachable
    }
}

void init_value_array_t(value_array_t *array)
{
    array->values = NULL;
    array->capacity = 0;
    array->count = 0;
}

void write_value_array_t(value_array_t *array, const value_t value)
{
    if (array->capacity < array->count + 1) {
        int old_capacity = array->capacity;
        array->capacity = GROW_CAPACITY(old_capacity);
        array->values = GROW_ARRAY(value_t, array->values, old_capacity, array->capacity);
    }

    array->values[array->count] = value;
    array->count++;
}

void free_value_array_t(value_array_t *array)
{
    FREE_ARRAY(value_t, array->values, array->capacity);
    init_value_array_t(array);
}

void print_value(const value_t value)
{
    switch (value.type) {
        case VAL_BOOL: printf(AS_BOOL(value) ? "true" : "false"); break;
        case VAL_NIL: printf("nil"); break;
        case VAL_NUMBER: printf("%g", AS_NUMBER(value)); break;
        case VAL_OBJ: print_object(value); break;
        case VAL_EMPTY: printf("<empty>"); break;
    }
}
