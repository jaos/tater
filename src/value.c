#include <stdio.h>

#include "memory.h"
#include "value.h"

void init_value_array_t(value_array_t *array)
{
    array->values = NULL;
    array->capacity = 0;
    array->count = 0;
}

void write_value_array_t(value_array_t *array, value_t value)
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

void print_value(value_t value)
{
    printf("%g", value);
}