#include <stdio.h>
#include <string.h>

#include "memory.h"
#include "object.h"
#include "table.h"
#include "value.h"
#include "vm.h"

#define ALLOCATE_OBJ(type, object_type) \
    (type*)allocate_object(sizeof(type), object_type)

static obj_t *allocate_object(const size_t size, const obj_type_t type) {
    obj_t *object = (obj_t*)reallocate(NULL, 0, size);
    object->type = type;
    object->next = vm.objects; // add to our vm's linked list of objects so we always have a reference to it
    vm.objects = object;
    return object;
}

static obj_string_t *make_string(const int length)
{
    obj_string_t *str = (obj_string_t *)allocate_object(sizeof(obj_string_t) + length + 1, OBJ_STRING);
    str->length = length;
    str->hash = 0;
    return str;
}

static uint32_t hash_string(const char *key, const int length)
{
    uint32_t hash = 2166136261u;
    for (int i = 0; i < length; i++) {
        hash ^= (uint8_t)key[i];
        hash *= 16777619;
    }
    return hash;
}

obj_string_t *copy_string(const char *chars, const int length)
{
    const uint32_t hash = hash_string(chars, length);
    obj_string_t *interned = find_string_table_t(&vm.strings, chars, length, hash);
    if (interned != NULL) {
        return interned;
    }

    obj_string_t *str = make_string(length);
    memcpy(str->chars, chars, length);
    str->chars[length] = '\0';
    str->hash = hash;
    set_table_t(&vm.strings, str, NIL_VAL);
    return str;
}

obj_string_t *concatenate_string(const obj_string_t *a, const obj_string_t *b)
{
    const int length = a->length + b->length;
    obj_string_t *str = make_string(length);

    memcpy(str->chars, a->chars, a->length);
    memcpy(str->chars + a->length, b->chars, b->length);
    str->chars[length] = '\0';
    str->hash = hash_string(str->chars, length);

    obj_string_t *interned = find_string_table_t(&vm.strings, str->chars, length, str->hash);
    if (interned != NULL) {
        FREE(obj_string_t, str);
        return interned;
    }

    set_table_t(&vm.strings, str, NIL_VAL);
    return str;
}

static void print_function(const obj_function_t *function)
{
    if (function->name == NULL) {
        printf("<script>");
    } else {
        printf("<fn %s>", function->name->chars);
    }
}

void print_object(const value_t value)
{
    switch (OBJ_TYPE(value)) {
        case OBJ_FUNCTION: print_function(AS_FUNCTION(value)); break;
        case OBJ_NATIVE: printf("<native fn>"); break;
        case OBJ_STRING: printf("%s", AS_CSTRING(value)); break;
        default: DEBUG_LOGGER("Unhandled default\n",); exit(EXIT_FAILURE);
    }
}

obj_function_t *new_obj_function_t(void)
{
    obj_function_t *function = ALLOCATE_OBJ(obj_function_t, OBJ_FUNCTION);
    function->arity = 0;
    function->name = NULL;
    init_chunk(&function->chunk);
    return function;
}

obj_native_t *new_obj_native_t(native_fn_t function)
{
    obj_native_t *native = ALLOCATE_OBJ(obj_native_t, OBJ_NATIVE);
    native->function = function;
    return native;
}
